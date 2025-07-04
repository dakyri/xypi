#include "ws_server.h"
#include "ws_session_handler.h"

// hack to avoid a warning about deprecated boost headers included by boost. seriously.
#include <boost/core/scoped_enum.hpp>
#define BOOST_DETAIL_SCOPED_ENUM_EMULATION_HPP
#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/strand.hpp>
#include <boost/bind/bind.hpp>

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

using namespace boost::placeholders; 
namespace beast = boost::beast;         // from <boost/beast.hpp>

WSServer::WSServer(asio::io_service& _ioservice, const tcp::endpoint _endpoint, std::shared_ptr<WSApiHandler> api)
	: endpoint(_endpoint)
	, acceptor(_ioservice)
	, sigWaiter(_ioservice, SIGINT, SIGTERM)
	, ioService(_ioservice)
	, wscmdHandler(api)
{}

/*!
 * kick off the socket listening and the async waiters on signals and connections. returns normally.
 */
void WSServer::start()
{
	boost::system::error_code ec;

	acceptor.open(endpoint.protocol(), ec);
	if(ec) {
		error("open error {}", ec.message());
		return;
	}
	acceptor.set_option(asio::socket_base::reuse_address(true), ec);
	if(ec) {
		error("set_option error {}", ec.message());
		return;
	}
	acceptor.bind(endpoint, ec);
	if(ec) {
		error("set_option error {}", ec.message());
		return;
	}
	acceptor.listen(asio::socket_base::max_listen_connections, ec);
	if(ec) {
		error("set_option error {}", ec.message());
		return;
	}
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
 * we're potentially running the ioService across multiple threads.
 */
void WSServer::accept()
{

	auto new_socket = std::make_shared<tcp::socket>(ioService);
/*
    acceptor.async_accept(
            asio::make_strand(ioService),
            beast::bind_front_handler(&WSServer::accept_handler, shared_from_this()));*/

}

void WSServer::accept_handler(boost::system::error_code ec, tcp::socket socket)
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
	std::make_shared<WSSessionHandler>(std::move(socket), wscmdHandler)->run();
	accept();	// Accept another connection
}
