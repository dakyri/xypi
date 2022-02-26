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
	using buf_t = std::array<uint8_t, kBufSize>;
private:
	void start_receive();
	void recv_handler(boost::system::error_code ec, std::size_t bytes_recvd, std::shared_ptr<buf_t> buf, std::shared_ptr<udp::endpoint> endp);
	void send_message();
	void send_handler(boost::system::error_code ec, std::size_t bytes_recvd, std::shared_ptr<buf_t> buf, std::shared_ptr<udp::endpoint> endp);

	udp::socket socket;
	boost::asio::signal_set signal;
	boost::asio::io_service& ioService;

	std::shared_ptr<OSCHandler> api;
};