#include "osc_server.h"

// hack to avoid a warning about deprecated boost headers included by boost. seriously.
#include <boost/core/scoped_enum.hpp>
#define BOOST_DETAIL_SCOPED_ENUM_EMULATION_HPP

#include <spdlog/spdlog.h>

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

void OSCServer::start_receive() {
	socket.async_receive_from(
		boost::asio::buffer(inBuf),
		senderEndpoint,
		[this](boost::system::error_code ec, std::size_t bytes_recvd) {
			if (!ec && bytes_recvd > 0) {
				// handle the incoming OSC
			}
		start_receive();
	});
}
