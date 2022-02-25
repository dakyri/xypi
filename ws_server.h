#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

class JSONHandler;

/*!
 * manage the main socket threads and connections
 */
class WSServer
{
public:
	WSServer(boost::asio::io_service& ioContext, uint16_t port, std::shared_ptr<JSONHandler> api);

	void start();

private:
	void accept();

	boost::asio::ip::tcp::acceptor acceptor;
	boost::asio::signal_set sigWaiter;
	boost::asio::io_service& ioService;

	std::shared_ptr<JSONHandler> jsonHandler;
};