#include "ws_session_handler.h"
#include "json_handler.h"

#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <util/sha1.hpp>
#include <util/base64.h>

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::trace;
using spdlog::warn;

namespace asio = boost::asio;
static const int kHttpBuffSize = 1024;
using http_buf_t = std::array<char, kHttpBuffSize>;
using regex = boost::regex;

WSSessionHandler::WSSessionHandler(boost::asio::io_service &_ioService,
								const std::shared_ptr<tcp::socket> _socket,
								asio::yield_context _yield,
								boost::asio::io_service::strand& _strand,
								std::shared_ptr<JSONHandler> _api,
								bool _upgraded)
	: socket(std::move(_socket))
	, socketDeadlineTimer(_ioService)
	, yieldCtxt(std::move(_yield))
	, strand(_strand)
	, startTime{boost::posix_time::microsec_clock::local_time()}
	, maxRetries(6)
	, deadlineSecs(6)
	, jsonHandler(_api)
	, isUpgraded(_upgraded)
{
	auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
	id = fmt::format("[{:x}]", threadId & 0xffff);
	info("WSSessionHandler({}) says 'Koo! Incoming request'", id);
}

WSSessionHandler::~WSSessionHandler() { debug("WSSessionHandler({}) exiting", id); }

void WSSessionHandler::run()
{
	trace("WSSessionHandler({})::run()", id);
	bool wasError = false;
	std::string errorMessage;
	try {
		if (!isUpgraded) {
			handleUpgradeRequest();
		}
		while (true) {
			auto req = readJson();
			if (!req.second) {
				debug("WSSessionHandler({})::run() end of stream", id);
				break;
			}
			debug("WSSessionHandler({})::run() processing next element in stream", id);
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
void WSSessionHandler::handleTimeout(const boost::system::error_code& ec)
{
	if (ec == asio::error::operation_aborted) { // we were cancelled. yay!!!
		debug("WSSessionHandler({}) timeout cancelled.", id);
		return;
	}

	retryCnt++;
	debug("WSSessionHandler({}) socket timeout received; retrying (#{})", id, retryCnt);

	boost::system::error_code ecc;
	socket->cancel(ecc);
	if (ecc) { debug("WSSessionHandler({}) socket cancel error: {} ({})", id, ec.message(), std::to_string(ec.value())); }
}

/*
upgrade headers look like ...
GET / HTTP/1.1
Host: 127.0.0.1:8080
User-Agent: Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:75.0) Gecko/20100101 Firefox/75.0
Accept:
Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate
Sec-WebSocket-Version: 13
Origin: null
Sec-WebSocket-Extensions: permessage-deflate
Sec-WebSocket-Key: 3O2LuHqHtVBHX4i1zxn/pw==
Connection: keep-alive, Upgrade
Pragma: no-cache
Cache-Control: no-cache
Upgrade: websocket

*/

inline std::string HexToBytes(const std::string& hex) {
	std::string bytes;

	for (unsigned int i = 0; i < hex.length(); i += 2) {
		std::string byteString = hex.substr(i, 2);
		char byte = (char)strtol(byteString.c_str(), NULL, 16);
		bytes.push_back(byte);
	}

	return bytes;
}
regex upgrade_re("(?<REQ>GET .+ HTTP.+\r\n)"
		"((Sec-WebSocket-Key: (?<KEY>.*)\r\n)"
		"|(?<UPG>Upgrade: websocket\r\n)"
		"|(.+:.+\r\n))+"
	"(?<HEND>\r\n)");
std::string response_template(
	"HTTP/1.1 101 Switching Protocols\r\n"
	"Upgrade: websocket\r\n"
	"Connection: Upgrade\r\n"
	"Sec-WebSocket-Accept: {}\r\n"
	"\r\n");
std::string whiskey_magic("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
/*!
 * initial read of a http request, looking for upgrade headers
 */
void WSSessionHandler::handleUpgradeRequest()
{
	retryCnt = 0;
	std::string request;
	while (!isUpgraded) {
//		socketDeadlineTimer.expires_from_now(boost::posix_time::seconds(20));
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
//		auto fn = [self](boost::system::error_code ec) {
//			self->handleTimeout(ec);
//		};
//		auto handler = strand.wrap(fn);
#else
		auto handler = [self](const boost::system::error_code ec) {
			self->handleTimeout(ec);
		};
#endif
#endif
//		socketDeadlineTimer.async_wait(handler);

		boost::system::error_code ec;
		boost::system::error_code ect;
		http_buf_t http_buf;

		uint32_t n_recv = socket->async_read_some(asio::buffer(http_buf, http_buf.size()), yieldCtxt[ec]);
//		socketDeadlineTimer.cancel(ect);
		if (n_recv == 0 && ec == 0) { // if we get an eof before upgrading, this is not a good thing
			debug("WSSessionHandler({}) no bytes: unexpected eof", id);
			ec = asio::error::eof;
		}
		if (ec) {
			if (ec != asio::error::operation_aborted && retryCnt < maxRetries) {
				debug("WSSessionHandler({}) upgrade retriable ({}/{}) read error {}, {}", id, retryCnt, maxRetries, ec.value(), ec.message());
			} else {
				debug("WSSessionHandler({}) upgrade ({} retries) ; closing socket", id, retryCnt);
				socket->close();
				const auto readError = fmt::format("Read error while waiting for upgrade: {} ({})", ec.message(), ec.value());
				error(readError);
				throw std::runtime_error(readError);
			}
		} else { // odds are we get it all in one hit, and it's not large. the front end page is simple and known, but ... who knows?
			request.append(http_buf.data(), n_recv);
			boost::cmatch results;
			if (boost::regex_search(request.c_str(), results, upgrade_re, boost::match_extra | boost::match_not_dot_newline)) {
				if (results["HEND"].matched && results["UPG"].matched) {
					if (!results["KEY"].matched) {
						socket->close();
						debug("WSSessionHandler({}) upgrade error. no security key in request", id);
						throw std::runtime_error(fmt::format("websocket error. expected security key"));
					}
					auto k = results["KEY"].str();
					k += whiskey_magic;
					SHA1 digest;
					digest.update(k);
					auto update_response = fmt::format(response_template, macaron::Base64::Encode(HexToBytes(digest.final())));
					asio::async_write(*socket, asio::buffer(update_response.c_str(), update_response.size()), yieldCtxt[ec]);
					if (ec) {
						socket->close();
						debug("WSSessionHandler({}) upgrade error {} ({}) upgrade response", id, ec.message(), ec.value());
						throw std::runtime_error(fmt::format("write error: {} ({})", ec.message(), ec.value()));
					}
					isUpgraded = true;
					writeJson({ { "cmd", "ping" } });
				} else {
					debug("WSSessionHandler({}) upgrade missing upgrade key and/or header end in '{}'", id, request);
					const auto readError = fmt::format("Upgrade not properly requested");
					error(readError);
					throw std::runtime_error(readError);
				}
			} else {
				debug("WSSessionHandler({}) basic http request not recognised in '{}'", id, request);
				const auto readError = fmt::format("Upgrade not properly requested");
				error(readError);
				throw std::runtime_error(readError);
			}
		}
	}
}

/*!
 * write a packet of bytes. framing is a simple 4 byte size prefix
 */
void WSSessionHandler::writeFramedBuffer(std::vector<uint8_t> buf)
{
	debug("WSSessionHandler({}) writing a {} byte frame ...", id, buf.size());
	const uint32_t size = buf.size();
	boost::system::error_code ec;
	asio::async_write(*socket, asio::buffer(&size, sizeof(size)), yieldCtxt[ec]);
	if (ec) {
		socket->close();
		debug("WSSessionHandler({}) error {} ({}) writing preamble ...", id, ec.message(), ec.value());
		throw std::runtime_error(fmt::format("write error: {} ({})", ec.message(), ec.value()));
	}

	debug("WSSessionHandler({}) data writing a {} byte frame ...", id, buf.size());
	asio::async_write(*socket, asio::buffer(buf), yieldCtxt[ec]);
	if (ec) {
		socket->close();
		debug("WSSessionHandler({}) error {} ({}) writing data ...", id, ec.message(), ec.value());
		throw std::runtime_error(fmt::format("write error: {} ({})", ec.message(), ec.value()));
	}
}

/*!
 * read a packet of bytes. framing is a simple 4 byte size prefix
 *  - \return optional<vector<bytes>> a vector of bytes uf tgere is a complete valid message, or nullopt if we hit end of stream cleanly
 *  - \throws a range of asio or runtime exceptions if there is a problem at this stage
 */
std::pair<std::vector<uint8_t>, bool> WSSessionHandler::readFramedBuffer()
{
	retryCnt = 0;
	while (true) {
		trace("WSSessionHandler({}) reading a frame...", id);
//		socketDeadlineTimer.expires_from_now(boost::posix_time::seconds(20));
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
//		auto fn = [self](boost::system::error_code ec) {
//			self->handleTimeout(ec);
//		};
//		auto handler = strand.wrap(fn);
#else
		auto handler = [self](const boost::system::error_code ec) {
			self->handleTimeout(ec);
		};
#endif
#endif
//		socketDeadlineTimer.async_wait(handler);

		boost::system::error_code ec;
		boost::system::error_code ect;
		uint32_t size;

		uint32_t nr = asio::async_read(*socket, asio::buffer(&size, sizeof(size)), yieldCtxt[ec]);
		if (nr == 0) {
			// we might get an eof here, but unless it's end of stream with zero bytes we should flag as an error
//			socketDeadlineTimer.cancel(ect); // and don't timeout!
			return{ std::vector<uint8_t>(), false };
		}
		std::vector<uint8_t> buf(size, 0); // pre-allocation of buffer seems necessary
		if (!ec) {
			nr = asio::async_read(*socket, asio::buffer(buf, size), yieldCtxt[ec]);
			if (nr != size) {
				error("WSSessionHandler({}) read error. Frame is short (expect {}, but got {})", id, size, nr);
				ec = asio::error::eof;
			} else {
				trace("WSSessionHandler({}) read a frame of {} bytes", id, size);
			}
		}

//		socketDeadlineTimer.cancel(ect);

		if (ec) {
			if (ec != asio::error::operation_aborted && retryCnt < maxRetries) {
				debug("WSSessionHandler({}) retriable ({}/{}) read error {}, {}", id, retryCnt, maxRetries, ec.value(), ec.message());
			} else {
				debug("WSSessionHandler({}) terminal error ({} retries) ; closing socket", id, retryCnt);
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
void WSSessionHandler::writeJson(const nlohmann::json& msg)
{
	auto serialized = msg.dump();
	debug("WSSessionHandler({}) write JSON => {}", id, serialized);
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
std::pair<nlohmann::json, bool> WSSessionHandler::readJson()
{
	auto bytes = readFramedBuffer();
	if (!bytes.second) return {nlohmann::json(), false};
	std::string instr(bytes.first.begin(), bytes.first.end());
	return { nlohmann::json::parse(instr), true };
}

void WSSessionHandler::sendError(const std::string& msg)
{
	error("WSSessionHandler({}) error '{}'", id, msg);
	writeJson({{"error", msg}});
}

void WSSessionHandler::setMaxRetry(uint32_t rts) { maxRetries = rts; }
void WSSessionHandler::setDeadlineSecs(uint32_t dlt) { deadlineSecs = dlt; }
