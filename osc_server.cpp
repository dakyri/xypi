#include "osc_server.h"
#include "osc_handler.h"

// hack to avoid a warning about deprecated boost headers included by boost. seriously.
#include <boost/core/scoped_enum.hpp>
#define BOOST_DETAIL_SCOPED_ENUM_EMULATION_HPP
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <spdlog/spdlog.h>
#include <memory>

/*!
 * \class Server
 * handles the basic osc messaging, in and out.
 * todo: would be neat if it had been done in a more coroutine-ish kinda way.
 */

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

namespace asio = boost::asio;
using udp = boost::asio::ip::udp;

OSCServer::OSCServer(asio::io_service& _ioService, uint16_t port, std::shared_ptr<oscapi::Processor> _handler)
	: socket(_ioService, udp::endpoint(udp::v4(), port)),
	  sigWaiter(_ioService, SIGINT, SIGTERM),
	  ioService(_ioService),
	  handler(_handler)
{
	set_current_destination("127.0.0.1", 57120);
}

/*!
 * kick off the socket listening and the async waiters on signals and connections. returns normally.
 */
void OSCServer::start()
{
// todo: multicast options are complex .. do I need that right now?
//	socket.set_option(boost::asio::ip::multicast::outbound_interface());

	start_receive();
	sigWaiter.async_wait([this](boost::system::error_code, int sig) {
		info("OSCServer::async_wait() SIGTERM received");
		ioService.post([this]() { socket.cancel(); });
	});

}

/*!
 * take a received buffer, parse the OSC message contained, and send that processed message onwards
 */
void OSCServer::recv_handler(boost::system::error_code ec, std::size_t bytes_recvd, std::shared_ptr<buf_t> buf, std::shared_ptr<udp::endpoint> endp) {
	if (!ec && bytes_recvd > 0) {
		// handle the incoming OSC, except if it is from our endpoint. because maybe we broadcast outputs
		if (endp->address() == socket.local_endpoint().address() && endp->port() == socket.local_endpoint().port()) {
			debug("OscServer rejecting a bounced packet ({}:{})", endp->address().to_string(), endp->port());
		} else {
			debug("OscServer receiving from {}:{}", endp->address().to_string(), endp->port());
			handler->parse(buf->data(), bytes_recvd);
			send_message("/viskas/gerai", { 1, 2, 1, 2, 3, 4 });
		}
	} else {
		if (ec.value() == asio::error::operation_aborted) {
			debug("OscServer got an abort! Bye for now....");
			return;
		} else {
			debug("OscServer recv_handler gets a crap packet, error {}: {}", ec.value(), ec.message());
		}
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
void OSCServer::send_handler(boost::system::error_code ec, std::size_t bytes_sent, std::shared_ptr<buf_t> buf, std::shared_ptr<udp::endpoint> endp) {
	debug("OSCServer::send_handler bytes {} error '{}' to {}:{}", bytes_sent, ec.message(), endp->address().to_string(), endp->port());
}

/*!
 * main message transmission wrapper.
 * TODO:
 *	- possibly shift the buffer type to a vector so we can be a bit more flexible. but 1024 as here should be adequate in almost any sane case.
 */
void OSCServer::send_message(const std::shared_ptr<oscapi::msg_t> msg) {
	auto outBuf = std::make_shared<buf_t>();
	auto dstEndpoint = std::make_shared<udp::endpoint>(currentDestination);
	std::size_t outBufLen = outBuf->size();
	if (handler->pack(outBuf->data(), outBufLen, msg)) {
		socket.async_send_to(
			boost::asio::buffer(*outBuf, outBufLen),
			*dstEndpoint,
			boost::bind(&OSCServer::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, outBuf, dstEndpoint)
		);
	}
}

/*!
 * message handler for a fixed message on the given path, with an arbitrary set of int parameters.
 * this method is mainly for testing, diagnostics and control messages
 */
void OSCServer::send_message(const std::string & path, const std::vector<int> & params)
{
	auto outBuf = std::make_shared<buf_t>();
	auto dstEndpoint = std::make_shared<udp::endpoint>(currentDestination);
	std::size_t outBufLen = outBuf->size();
	if (handler->pack(outBuf->data(), outBufLen, path, params)) {
		socket.async_send_to(
			boost::asio::buffer(*outBuf, outBufLen),
			*dstEndpoint,
			boost::bind(&OSCServer::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, outBuf, dstEndpoint)
		);
	}
}

/*!
 * make our current target(s).
 * todo: in the bigger picture needs to be threadsafe
 * todo: also maybe we could have multiple.
 * and to multicast or not to multicast?
 */
boost::system::error_code OSCServer::set_current_destination(std::string str_address, uint16_t port_num)
{
	boost::system::error_code ec;
	auto ip_address =
		asio::ip::address::from_string(str_address, ec);
	if (ec.value() != 0) {
		return ec;
	}
	currentDestination = asio::ip::udp::endpoint(ip_address, port_num);
	return boost::system::error_code();
}