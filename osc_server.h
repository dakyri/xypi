#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/signal_set.hpp>

class OSCHandler;
using udp = boost::asio::ip::udp;

/*!
 * manage the main socket threads and connections
 */
class OSCServer
{
public:
	OSCServer(boost::asio::io_service& _ioService, uint16_t port, std::shared_ptr<OSCHandler> _api);

	void start();

	static const int kBufSize = 1024;
private:
	void start_receive();

	udp::socket socket;
	udp::endpoint senderEndpoint;
	boost::asio::signal_set signal;
	boost::asio::io_service& ioService;
	std::array<uint8_t, kBufSize> inBuf;

	std::shared_ptr<OSCHandler> api;
};