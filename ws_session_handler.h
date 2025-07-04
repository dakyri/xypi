#pragma once

#include "wsapi_cmd.h"

// supresses a ridiculous warning. ffs boost! go home, you are drunk!
#define BOOST_DETAIL_SCOPED_ENUM_EMULATION_HPP
#include <boost/core/scoped_enum.hpp>

#include <memory>
#include <utility>

#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <nlohmann/json_fwd.hpp>

class WSApiHandler;
namespace websocket = boost::beast::websocket;
namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;
using error_code = boost::system::error_code;


/*!
 * handle the basic raw processing of a single stream of socket data and appropriate responses.
 * turns the byte stream into a command stream and sends that to the ApiHandler
 * all the framing, and low level io are our responsibility here
 */
class WSSessionHandler : public std::enable_shared_from_this<WSSessionHandler>
{
public:
	WSSessionHandler(tcp::socket && _socket, std::shared_ptr<WSApiHandler> _api);
	~WSSessionHandler();
	
	void run();
	void read();

	void onRun();
	void onAccept(error_code e);
	void onRead(error_code ec, std::size_t bytes_transferred);
	void onWrite(error_code ec, std::size_t bytes_transferred);

	void setTimoutSecs(uint32_t dlt);

private:
	void writeResponse(const std::string& msg);

	websocket::stream<tcp::socket> ws;
	const boost::posix_time::ptime startTime;
	beast::flat_buffer o_buffer;
	beast::flat_buffer i_buffer;

	std::string id;
	std::shared_ptr<WSApiHandler> wscmdHandler;
};
