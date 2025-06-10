#include "ws_session_handler_coro.h"
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

using namespace boost::placeholders; 
namespace asio = boost::asio;
static const int kHttpBuffSize = 1024;
using http_buf_t = std::array<char, kHttpBuffSize>;
using regex = boost::regex;

WSSessionHandler::WSSessionHandler(boost::asio::io_context &_ioService,
								const std::shared_ptr<tcp::socket> _socket,
								asio::yield_context _yield,
								boost::asio::io_context::strand& _strand,
								std::shared_ptr<WSApiHandler> _api)
	: ws(std::move(*_socket))
	, yieldCtxt(std::move(_yield))
	, strand(_strand)
	, startTime{boost::posix_time::microsec_clock::local_time()}
	, wsapiHandler(_api)
{
	auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
	id = fmt::format("[{:x}]", threadId & 0xffff);
	info("WSSessionHandler({}) says 'Koo! Incoming request'", id);
}

WSSessionHandler::~WSSessionHandler() { debug("WSSessionHandler({}) exiting", id); }

void WSSessionHandler::run()
{
	trace("WSSessionHandler({})::run()", id);

	boost::system::error_code ec;
	auto close_code = websocket::close_code::normal;

	ws.async_accept(yieldCtxt[ec]);
	if (ec) {
		ws.close(websocket::close_reason{ websocket::close_code::going_away });
		debug("WSSessionHandler({})::run() accept fails {}: {}", id, ec.value(), ec.message());
		throw std::runtime_error(fmt::format("websocket accept fails {}", ec.message()));
	}

	bool wasError = false;
	std::string errorMessage;
	try {
		while (true) {
			boost::beast::multi_buffer buffer;
			ws.async_read(buffer, yieldCtxt[ec]);
			if (ec) {
				if (ec != websocket::error::closed) {
					// so, an actual error, not a clean close
					wasError = true;
					errorMessage = fmt::format("Websocket read error {}: {}", ec.value(), ec.message());
					close_code = websocket::close_code::bad_payload;
				}
				break;
			}
			if (!ws.got_text()) {
				wasError = true;
				errorMessage = fmt::format("Websocket unexpected binary message {}: {}", ec.value(), ec.message());
				close_code = websocket::close_code::bad_payload;
			}
			auto bytes = buffer.data();
			auto msgStr = boost::beast::buffers_to_string(bytes);
			auto msgJsn = nlohmann::json::parse(msgStr);

			debug("WSSessionHandler({})::run() processing next element in stream", id);
			auto res = wsapiHandler->process(msgJsn);
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
	ws.close(websocket::close_reason{ close_code });
}


/*!
 * write the given json message to the socket.
 *  \throws anything thrown by writeFramedBuffer or the json serializer
 */
void WSSessionHandler::writeResponse(const nlohmann::json& msg)
{
	auto serialized = msg.dump();
	debug("WSSessionHandler({}) write JSON => {}", id, serialized);
	std::string response_bytes(serialized.begin(), serialized.end());
	boost::system::error_code ec;
	boost::beast::multi_buffer buffer;
	boost::beast::ostream(buffer) << response_bytes;
	ws.text(true);
	ws.async_write(buffer.data(), yieldCtxt[ec]);
	if (ec) {
		error("WSSessionHandler({}) error '{}'", id, ec.message());
		throw std::runtime_error(fmt::format("bad write on websocket: {}, {}", ec.value(), ec.message()));
	}
}

void WSSessionHandler::sendError(const std::string& msg)
{
	error("WSSessionHandler({}) error '{}'", id, msg);
	writeResponse({{"error", msg}});
}

void WSSessionHandler::setTimoutSecs(uint32_t to_secs) { debug("WSSessionHandler({}) setting timeout {} unimplemented", id, to_secs); }
