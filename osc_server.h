#pragma once

#include <string>
#include <vector>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/signal_set.hpp>

#include "message.h"

namespace oscapi {
	class Processor;
}
using udp = boost::asio::ip::udp;

/*!
 * manage the osc socket threads and connections
 */
class OSCServer
{
public:
	OSCServer(boost::asio::io_service& _ioService, uint16_t port, std::shared_ptr<oscapi::Processor> _handler);

	void start();
	void send_message(const std::shared_ptr<xymsg::msg_t> msg);
	void send_message(const std::string& path, const std::vector<int> & params = {});
	boost::system::error_code set_current_destination(std::string ip_address, uint16_t port_num);

	static const int kBufSize = 1024;
	using buf_t = std::array<uint8_t, kBufSize>;
private:
	void start_receive();
	void recv_handler(boost::system::error_code ec, std::size_t bytes_recvd, std::shared_ptr<buf_t> buf, std::shared_ptr<udp::endpoint> endp);
	void send_handler(boost::system::error_code ec, std::size_t bytes_recvd, std::shared_ptr<buf_t> buf, std::shared_ptr<udp::endpoint> endp);

	udp::socket socket;
	udp::endpoint currentDestination;
	boost::asio::signal_set sigWaiter;
	boost::asio::io_service& ioService;

	std::shared_ptr<oscapi::Processor> handler;
};