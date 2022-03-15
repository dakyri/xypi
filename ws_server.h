#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

class JSONHandler;
using tcp = boost::asio::ip::tcp;

/*!
 * manage the main socket threads and connections
 */
class WSServer
{
public:
	WSServer(boost::asio::io_service& ioContext, uint16_t port, std::shared_ptr<JSONHandler> api);

	void start();

protected:
	void accept();
	void accept_handler(boost::system::error_code ec, std::shared_ptr<tcp::socket> endp);


	boost::asio::ip::tcp::acceptor acceptor;
	boost::asio::signal_set sigWaiter;
	boost::asio::io_service& ioService;


	std::shared_ptr<JSONHandler> jsonHandler;
};