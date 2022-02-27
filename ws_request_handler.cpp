#include "ws_request_handler.h"
#include "json_handler.h"

#include <boost/bind.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read.hpp>
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

namespace asio = boost::asio;

WSRequestHandler::WSRequestHandler(boost::asio::io_service &_ioService,
								const std::shared_ptr<tcp::socket> _socket,
								asio::yield_context _yield,
								boost::asio::io_service::strand& _strand,
								std::shared_ptr<JSONHandler> _api)
	: socket(std::move(_socket))
	, socketDeadlineTimer(_ioService)
	, yieldCtxt(std::move(_yield))
	, strand(_strand)
	, startTime{boost::posix_time::microsec_clock::local_time()}
	, maxRetries(6)
	, deadlineSecs(6)
	, jsonHandler(_api)
{
	auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
	id = fmt::format("[{:x}]", threadId & 0xffff);
	info("WSRequestHandler({}) says 'Koo! Incoming request'", id);
}

WSRequestHandler::~WSRequestHandler() { debug("WSRequestHandler({}) says 'Net tsapa!'", id); }

void WSRequestHandler::run()
{
	trace("WSRequestHandler({})::run()", id);
	bool wasError = false;
	std::string errorMessage;
	try {
		while (true) {
			auto req = readJson();
			if (!req.second) {
				trace("WSRequestHandler({})::run() end of stream", id);
				break;
			}
			trace("WSRequestHandler({})::run() processing next element in stream", id);
			auto res = jsonHandler->process(req.first);
			writeJson(res);
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
			sendError(errorMessage);
		} catch (const std::exception& ex) {
			info("Failed to send error response: {}", ex.what());
		}
	}
}

/*!
 * generic deadline timeout handler. we make a special case of operation_aborted, which can be generated
 * by the cancellation of an async wait, which we are kind of hoping for: that's just the normall cancellation
 * of the timer if everything is ok.
 */
void WSRequestHandler::handleTimeout(const boost::system::error_code& ec)
{
	if (ec == asio::error::operation_aborted) { // we were cancelled. yay!!!
		trace("WSRequestHandler({}) timeout cancelled.", id);
		return;
	}

	retryCnt++;
	debug("WSRequestHandler({}) socket timeout received; retrying (#{})", id, retryCnt);

	boost::system::error_code ecc;
	socket->cancel(ecc);
	if (ecc) { debug("WSRequestHandler({}) socket cancel error: {} ({})", id, ec.message(), std::to_string(ec.value())); }
}

/*!
 * write a packet of bytes. framing is a simple 4 byte size prefix
 */
void WSRequestHandler::writeFramedBuffer(std::vector<uint8_t> buf)
{
	trace("WSRequestHandler({}) writing a {} byte frame ...", id, buf.size());
	const uint32_t size = buf.size();
	boost::system::error_code ec;
	asio::async_write(*socket, asio::buffer(&size, sizeof(size)), yieldCtxt[ec]);
	if (ec) {
		socket->close();
		debug("WSRequestHandler({}) error {} ({}) writing preamble ...", id, ec.message(), ec.value());
		throw std::runtime_error(fmt::format("write error: {} ({})", ec.message(), ec.value()));
	}

	asio::async_write(*socket, asio::buffer(buf), yieldCtxt[ec]);
	if (ec) {
		socket->close();
		debug("WSRequestHandler({}) error {} ({}) writing data ...", id, ec.message(), ec.value());
		throw std::runtime_error(fmt::format("write error: {} ({})", ec.message(), ec.value()));
	}
}

/*!
 * read a packet of bytes. framing is a simple 4 byte size prefix
 *  - \return optional<vector<bytes>> a vector of bytes uf tgere is a complete valid message, or nullopt if we hit end of stream cleanly
 *  - \throws a range of asio or runtime exceptions if there is a problem at this stage
 */
std::pair<std::vector<uint8_t>, bool> WSRequestHandler::readFramedBuffer()
{
	retryCnt = 0;
	while (true) {
		trace("WSRequestHandler({}) reading a frame...", id);
		socketDeadlineTimer.expires_from_now(boost::posix_time::seconds(20));
		auto self = shared_from_this();
/* TODO: XXXX: fixme later. c++17 boost 1.66 code breaks for c++14 boost 1.62. C14FIX is the attempt to do it correctly.
   C14FIX seems to work, but I'm still not 100% about the changes */
#define C14FIX
#ifdef C17B166
		auto handler = std::bind(strand, [self](const boost::system::error_code ec) {
			self->handleTimeout(ec);
		});
#else
#ifdef C14FIX
		auto fn = [self](boost::system::error_code ec) {
			self->handleTimeout(ec);
		};
		auto handler = strand.wrap(fn);
#else
		auto handler = [self](const boost::system::error_code ec) {
			self->handleTimeout(ec);
		};
#endif
#endif
		socketDeadlineTimer.async_wait(handler);

		boost::system::error_code ec;
		boost::system::error_code ect;
		uint32_t size;

		uint32_t nr = asio::async_read(*socket, asio::buffer(&size, sizeof(size)), yieldCtxt[ec]);
		if (nr == 0) {
			// we might get an eof here, but unless it's end of stream with zero bytes we should flag as an error
			socketDeadlineTimer.cancel(ect); // and don't timeout!
			return{ std::vector<uint8_t>(), false };
		}
		std::vector<uint8_t> buf(size, 0); // pre-allocation of buffer seems necessary
		if (!ec) {
			nr = asio::async_read(*socket, asio::buffer(buf, size), yieldCtxt[ec]);
			if (nr != size) {
				error("WSRequestHandler({}) read error. Frame is short (expect {}, but got {})", id, size, nr);
				ec = asio::error::eof;
			} else {
				trace("WSRequestHandler({}) read a frame of {} bytes", id, size);
			}
		}

		socketDeadlineTimer.cancel(ect);

		if (ec) {
			if (ec != asio::error::operation_aborted && retryCnt < maxRetries) {
				debug("WSRequestHandler({}) retriable ({}/{}) read error {}, {}", id, retryCnt, maxRetries, ec.value(), ec.message());
			} else {
				debug("WSRequestHandler({}) terminal error ({} retries) ; closing socket", id, retryCnt);
				socket->close();
				const auto readError = fmt::format("Read error: {} ({})", ec.message(), ec.value());
				error(readError);
				throw std::runtime_error(readError);
			}
		} else {
			return { buf, true };
		}
	}
}

/*!
 * write the given json message to the socket.
 *  \throws anything thrown by writeFramedBuffer or the json serializer
 */
void WSRequestHandler::writeJson(const nlohmann::json& msg)
{
	auto serialized = msg.dump();
	debug("Requesthandler({}) => {}", id, serialized);
	std::vector<uint8_t> buf(serialized.begin(), serialized.end());
	writeFramedBuffer(buf);
}

/*!
 * return a parsed json object from the next message on the socket
 * if we can find one, or nullopt if we hit the end of stream cleanly.
 * partial results and unexpected end of stream will throw.
 *  - \return optional<json> a json object if there is a complete valid json object, or nullopt if we hit end of stream cleanly
 *  - \throws anything that can be thrown by readFramedBuffer or json::parse()
 */
std::pair<nlohmann::json, bool> WSRequestHandler::readJson()
{
	auto bytes = readFramedBuffer();
	if (!bytes.second) return {nlohmann::json(), false};
	std::string instr(bytes.first.begin(), bytes.first.end());
	return { nlohmann::json::parse(instr), true };
}

void WSRequestHandler::sendError(const std::string& msg)
{
	error("WSRequestHandler({}) error '{}'", id, msg);
	writeJson({{"error", msg}});
}

void WSRequestHandler::setMaxRetry(uint32_t rts) { maxRetries = rts; }
void WSRequestHandler::setDeadlineSecs(uint32_t dlt) { deadlineSecs = dlt; }
