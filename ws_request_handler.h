#pragma once

#include "workqueue.h"

// supresses a ridiculous warning. ffs boost! go home, you are drunk!
#define BOOST_DETAIL_SCOPED_ENUM_EMULATION_HPP
#include <boost/core/scoped_enum.hpp>

#include <memory>
#include <utility>

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <nlohmann/json_fwd.hpp>

class JSONHandler;

using io_strand_t = boost::asio::io_service::strand;
using tcp = boost::asio::ip::tcp;

/*!
 * handle the basic raw processing of a single stream of socket data and appropriate responses.
 * turns the byte stream into a json command stream and sends that to the ApiHandler
 * all the framing, and low level io are our responsibility here
 */
class WSRequestHandler : public std::enable_shared_from_this<WSRequestHandler>
{
public:
	WSRequestHandler(const std::shared_ptr<boost::asio::ip::tcp::socket> socket,
				   boost::asio::yield_context yield,
				   io_strand_t& strand,
				   std::shared_ptr<JSONHandler> api);
	~WSRequestHandler();
	void run();
	void setMaxRetry(uint32_t rts);
	void setDeadlineSecs(uint32_t dlt);

private:
	void handleTimeout(const boost::system::error_code& ec);
	void writeFramedBuffer(std::vector<uint8_t> buf);
	std::pair<std::vector<uint8_t>, bool> readFramedBuffer();
	void sendError(const std::string& msg);
	void writeJson(const nlohmann::json& msg);
	std::pair<nlohmann::json, bool> readJson();

	std::shared_ptr<tcp::socket> socket;
	boost::asio::deadline_timer socketDeadlineTimer;
	boost::asio::yield_context yieldCtxt;
	io_strand_t strand;
	const boost::posix_time::ptime startTime;

	uint32_t retryCnt;
	uint32_t maxRetries;
	uint32_t deadlineSecs;

	std::string id;
	std::shared_ptr<JSONHandler> jsonHandler;
};
