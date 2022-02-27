#include "osc_server.h"

// hack to avoid a warning about deprecated boost headers included by boost. seriously.
#include <boost/core/scoped_enum.hpp>
#define BOOST_DETAIL_SCOPED_ENUM_EMULATION_HPP
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <spdlog/spdlog.h>
#include <memory>

/*!
 * \class Server
 * handles the basic connection management via boost::asio
 */

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

namespace asio = boost::asio;
using udp = boost::asio::ip::udp;

OSCServer::OSCServer(asio::io_service& _ioService, uint16_t port, std::shared_ptr<OSCHandler> _api)
	: socket(_ioService, udp::endpoint(udp::v4(), port)),
	  signal(_ioService, SIGINT, SIGTERM),
	  ioService(_ioService),
	  api(_api)
{}

/*!
 * kick off the socket listening and the async waiters on signals and connections. returns normally.
 */
void OSCServer::start()
{
	start_receive();
}

/*!
 * take a received buffer, parse the OSC message contained, and send that processed message onwards
 */
void OSCServer::recv_handler(boost::system::error_code ec, std::size_t bytes_recvd, std::shared_ptr<buf_t> buf, std::shared_ptr<udp::endpoint> endp) {
	if (!ec && bytes_recvd > 0) {
		// handle the incoming OSC, except if it is from our endpoint. because maybe we broadcast outputs
	}
	start_receive();
}

void OSCServer::start_receive() {
	auto inBuf = std::make_shared<buf_t>();
	auto srcEndpoint = std::make_shared<udp::endpoint>();
	socket.async_receive_from(
		boost::asio::buffer(*inBuf),
		*srcEndpoint,
		boost::bind(&OSCServer::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, inBuf, srcEndpoint)
	);
}

/*!
 * we're not overly interested in doing anything after sending at the moment. but binding to this handler holds our buffer
 */
void OSCServer::send_handler(boost::system::error_code ec, std::size_t bytes_recvd, std::shared_ptr<buf_t> buf, std::shared_ptr<udp::endpoint> endp) {
}

void OSCServer::send_message() {
	auto outBuf = std::make_shared<buf_t>();
	auto dstEndpoint = std::make_shared<udp::endpoint>();
	socket.async_send_to(
		boost::asio::buffer(*outBuf),
		*dstEndpoint,
		boost::bind(&OSCServer::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, outBuf, dstEndpoint)
	);
}

