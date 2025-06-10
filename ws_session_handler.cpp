#include "ws_session_handler.h"
#include "wsapi_handler.h"

#include <boost/bind/bind.hpp>
#include <boost/regex.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::trace;
using spdlog::warn;


namespace net = boost::asio;            // from <boost/asio.hpp>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>

using namespace boost::placeholders; 
namespace asio = boost::asio;
static const int kHttpBuffSize = 1024;
using http_buf_t = std::array<char, kHttpBuffSize>;
using regex = boost::regex;

WSSessionHandler::WSSessionHandler(tcp::socket && _socket,
								std::shared_ptr<WSApiHandler> _api)
	: ws(std::move(_socket))
	, startTime{boost::posix_time::microsec_clock::local_time()}
	, wscmdHandler(_api)
{
	auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
	id = fmt::format("[{:x}]", threadId & 0xffff);
	info("WSSessionHandler({}) says 'Koo! Incoming request'", id);
}

WSSessionHandler::~WSSessionHandler() { debug("WSSessionHandler({}) exiting", id); }

void
WSSessionHandler::run()
{
	// We need to be executing within a strand to perform async operations on the I/O objects in this session. Although not strictly necessary
	// for single-threaded contexts, this example code is written to bethread-safe by default.
	asio::dispatch(ws.get_executor(), beast::bind_front_handler(&WSSessionHandler::onRun, shared_from_this()));
}

void WSSessionHandler::onRun()
{
	trace("WSSessionHandler({})::onRun()", id);

	ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

// Set a decorator to change the Server of the handshake
	ws.set_option(websocket::stream_base::decorator(
		[] (websocket::response_type& res) {
			res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
		})
	);

	// Accept the websocket handshake
	ws.async_accept(beast::bind_front_handler(&WSSessionHandler::onAccept, shared_from_this()));
}

void
WSSessionHandler::onAccept(error_code ec)
{
	if (ec) {
		ws.close(websocket::close_reason{ websocket::close_code::going_away });
		debug("WSSessionHandler({})::run() accept fails {}: {}", id, ec.value(), ec.message());
		throw std::runtime_error(fmt::format("websocket accept fails {}", ec.message()));
	}
	read();
}

void WSSessionHandler::read()
{
	// Read a message into our buffer
	ws.async_read(i_buffer, beast::bind_front_handler(&WSSessionHandler::onRead, shared_from_this()));
}

void WSSessionHandler::onRead(error_code ec, std::size_t bytes_transferred)
{
	auto close_code = websocket::close_code::normal;
	bool wasError = false;
	std::string errorMessage;

	try {
		if (ec) {
			if (ec != websocket::error::closed) {
				// so, an actual error, not a clean close
				wasError = true;
				errorMessage = fmt::format("Websocket read error {}: {}", ec.value(), ec.message());
				close_code = websocket::close_code::bad_payload;
			} else {
				ws.close(websocket::close_reason{close_code});
			}
		} else if (!ws.got_text()) {
			wasError = true;
			errorMessage = fmt::format("Websocket unexpected binary message {}: {}", ec.value(), ec.message());
			close_code = websocket::close_code::bad_payload;
		} else {
			auto bytes = i_buffer.data();
			auto msgStr = boost::beast::buffers_to_string(bytes);

			debug("WSSessionHandler({})::run() processing next element in stream", id);
			auto res = wscmdHandler->process(msgStr);
			if (res.first) {
				writeResponse(res.second);
			}
		}
	} catch (const nlohmann::detail::exception& e) {
		// something awful happened while parsing the json
		wasError = true;
		errorMessage = fmt::format("JSON parse error, {}", e.what());
	} catch (const std::runtime_error& e) {
		// very likely caused by a read/write error in this or a lower module
		wasError = true;
		errorMessage = e.what();
	} catch (const std::exception& e) {
		// something happened, somewhere. don't panic. it's just a thing.
		wasError = true;
		errorMessage = e.what();
	}

	if (wasError) {
		try {
			if (close_code == websocket::close_code::normal) close_code = websocket::close_code::internal_error;
			error("WSSessionHandler({}) error '{}'", id, errorMessage);
		} catch (const std::exception& ex) {
			info("Failed to send error response: {}", ex.what());
		}
	}
}


void
WSSessionHandler::onWrite(error_code ec, std::size_t bytes_transferred)
{
	if (ec) {
		error("WSSessionHandler({}) error '{}'", id, ec.message());
		throw std::runtime_error(fmt::format("bad write on websocket: {}, {}", ec.value(), ec.message()));
	}
	o_buffer.consume(o_buffer.size()); 	// Clear the buffer
	read();
}


/*!
 * write the given json message to the socket.
 *  \throws anything thrown by writeFramedBuffer or the json serializer
 */
void WSSessionHandler::writeResponse(const std::string& response_bytes)
{
	boost::beast::ostream(o_buffer) << response_bytes;
	ws.text(true);
	ws.async_write(o_buffer.data(), beast::bind_front_handler( &WSSessionHandler::onWrite, shared_from_this()));
	debug("WSSessionHandler({}) write JSON => {}", id, response_bytes);
}

void WSSessionHandler::setTimoutSecs(uint32_t to_secs) { debug("WSSessionHandler({}) setting timeout {} unimplemented", id, to_secs); }