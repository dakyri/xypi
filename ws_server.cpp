#include "ws_server.h"
#include "ws_request_handler.h"

// hack to avoid a warning about deprecated boost headers included by boost. seriously.
#include <boost/core/scoped_enum.hpp>
#define BOOST_DETAIL_SCOPED_ENUM_EMULATION_HPP

#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>

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
using tcp = boost::asio::ip::tcp;

WSServer::WSServer(asio::io_service& _ioservice, uint16_t port, std::shared_ptr<JSONHandler> api)
	: acceptor(_ioservice, tcp::endpoint(tcp::v4(), port)), sigWaiter(ioService, SIGINT, SIGTERM), ioService(_ioservice), jsonHandler(api)
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
			info("Server::start() SIGTERM received");
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
	tcp::socket new_socket(ioService);
	acceptor.async_accept(new_socket, [this,&new_socket](boost::system::error_code ec) {
		if (ec == asio::error::operation_aborted) {
			debug("Server::accept() canceled (asio::error::operation_aborted)");
			acceptor.close();
			sigWaiter.cancel();
			return;
		}

		if (ec) {
			warn("Failed to accept connection, error: {0} ({1})", ec.message(), ec.value());
			accept();
			return;
		}

		debug("Server::accept() making new connection \\o/");
		auto strand = asio::io_service::strand(ioService); // the strand is copied into the RequestHandler
		auto socket = std::make_shared<tcp::socket>(std::move(new_socket));
		asio::spawn(strand, [socket, strand, this](asio::yield_context yield) {
			auto handler = std::make_shared<WSRequestHandler>(socket, yield, strand, jsonHandler);
			try {
				handler->run();
			} catch (const std::exception& e) {
				error("Server::accept() got excption from request handler: {}", e.what());
			}
		});
		accept();
	});
}
