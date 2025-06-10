#pragma once

#include <memory>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

class WSApiHandler;
using tcp = boost::asio::ip::tcp;

namespace asio = boost::asio;            // from <boost/asio.hpp>

/*!
 * manage the main socket threads and connections
 */
class WSServer : public std::enable_shared_from_this<WSServer>
{
public:
	WSServer(asio::io_service& ioContext, const tcp::endpoint _endpoint, std::shared_ptr<WSApiHandler> api);

	void start();

protected:
	void accept();
	void accept_handler(boost::system::error_code ec, tcp::socket endp);

	tcp::endpoint endpoint;
	tcp::acceptor acceptor;
	asio::signal_set sigWaiter;
	asio::io_service& ioService;

	std::shared_ptr<WSApiHandler> wscmdHandler;
};