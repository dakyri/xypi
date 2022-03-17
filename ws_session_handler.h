#pragma once

#include "jsapi_workq.h"

// supresses a ridiculous warning. ffs boost! go home, you are drunk!
#define BOOST_DETAIL_SCOPED_ENUM_EMULATION_HPP
#include <boost/core/scoped_enum.hpp>

#include <memory>
#include <utility>

#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <nlohmann/json_fwd.hpp>

class JSONHandler;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;


/*!
 * handle the basic raw processing of a single stream of socket data and appropriate responses.
 * turns the byte stream into a json command stream and sends that to the ApiHandler
 * all the framing, and low level io are our responsibility here
 */
class WSSessionHandler : public std::enable_shared_from_this<WSSessionHandler>
{
public:
	WSSessionHandler(boost::asio::io_service &_ioService,
					const std::shared_ptr<tcp::socket> _socket,
					boost::asio::yield_context _yield,
					boost::asio::io_service::strand& _strand,
					std::shared_ptr<JSONHandler> _api);
	~WSSessionHandler();
	void run();
	void setTimoutSecs(uint32_t dlt);

private:
	void sendError(const std::string& msg);
	void writeJson(const nlohmann::json& msg);

	websocket::stream<tcp::socket> ws;
	boost::asio::yield_context yieldCtxt;
	boost::asio::io_service::strand& strand;
	const boost::posix_time::ptime startTime;

	std::string id;
	std::shared_ptr<JSONHandler> jsonHandler;
};
