#include "ws_server.h"
#include "ws_session_handler.h"

// hack to avoid a warning about deprecated boost headers included by boost. seriously.
#include <boost/core/scoped_enum.hpp>
#define BOOST_DETAIL_SCOPED_ENUM_EMULATION_HPP
#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/strand.hpp>
#include <boost/bind.hpp>

#include <spdlog/spdlog.h>
//#include <sys/wait.h>

/*!
 * \class Server
 * handles the basic connection management via boost::asio
 */

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

namespace asio = boost::asio;

WSServer::WSServer(asio::io_service& _ioservice, uint16_t port, std::shared_ptr<JSONHandler> api)
	: acceptor(_ioservice, tcp::endpoint(tcp::v4(), port)), sigWaiter(_ioservice, SIGINT, SIGTERM), ioService(_ioservice), jsonHandler(api)
{}

/*!
 * kick off the socket listening and the async waiters on signals and connections. returns normally.
 */
void WSServer::start()
{
	boost::system::error_code ec;
	acceptor.listen(asio::socket_base::max_connections, ec);
	sigWaiter.async_wait([this](boost::system::error_code, int sig) {
		if (acceptor.is_open()) {
			info("WSServer::start() SIGTERM received");
			ioService.post( [this]() { acceptor.cancel(); });
		}
	});
	accept();
}


/*!
 * initiates an an async acceptence of a connection on the given socket.
 * unless we're specifically cancelled, we relaunch.
 * we're potentially running the iocontext across multiple threads.
 */
void WSServer::accept()
{

	auto new_socket = std::make_shared<tcp::socket>(ioService);
	acceptor.async_accept(*new_socket, boost::bind(&WSServer::accept_handler, this, boost::asio::placeholders::error, new_socket));
}

void WSServer::accept_handler(boost::system::error_code ec, std::shared_ptr<tcp::socket> socket)
{
	if (ec == asio::error::operation_aborted) {
		debug("WSServer::accept() canceled (asio::error::operation_aborted)");
		acceptor.close();
		sigWaiter.cancel();
		return;
	}

	if (ec) {
		warn("WSServer::accept()  failed to accept connection, error: {0} ({1})", ec.message(), ec.value());
		accept();
		return;
	}

	debug("WSServer::accept() making new connection \\o/");
	auto strand = asio::io_service::strand(ioService); // the strand is copied into the RequestHandler
	asio::spawn(strand, [socket, &strand, this](asio::yield_context yield) {
		auto handler = std::make_shared<WSSessionHandler>(ioService, socket, yield, strand, jsonHandler);
		try {
			handler->run();
		}
		catch (const std::exception& e) {
			error("WSServer::accept() got excption from request handler: {}", e.what());
		}
	});
	accept();
}
