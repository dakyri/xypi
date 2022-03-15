#include "xypi_hub.h"

#include "osc_api.h"
#include "osc_server.h"
#include "osc_worker.h"
#include "json_handler.h"
#include "ws_server.h"

#include "spdlog/spdlog.h"

using spdlog::info;
using spdlog::debug;

/*!
 * \class XypiHub
 * main wrapper for the osc server and spi reader. owns all the shared data structures (queues, results map),
 * the server, the io context and the worker.
 */

/*!
 * create our hub
 *  \param serverPort uint16_t what is says on the box
 *	\param threadCount uint16_t number of threads to launch. if 0, we'll make a reasonable estimate
 */
XypiHub::XypiHub(std::string dst_osc_adr, uint16_t dst_osc_prt, uint16_t rcv_osc_port, uint16_t ws_port, uint16_t threadCount)
	: threadCount(threadCount > 0 ? threadCount : 1)
{
	info("Xypi servers running on {} threads: OSC server on port {}, WS server on port {}.", threadCount, rcv_osc_port, ws_port);
//	oscParser = std::make_shared<oscapi::Processor>(oscOutQ);
//	oscServer = std::make_unique<OSCServer>(ioService, rcv_osc_port, oscParser);
//	oscServer->set_current_destination(dst_osc_adr, dst_osc_prt);
	jsonApi = std::make_shared<JSONHandler>(jsapi::workq_t(), jsapi::results_t());
	wsServer = std::make_unique<WSServer>(ioService, ws_port, jsonApi);

	oscWorker = std::make_unique<OSCWorker>(*oscServer.get(), oscOutQ);
}

XypiHub::~XypiHub() = default;

/*!
 * sets up the server, starts the worker thread, and runs the io context on a thread pool.
 * does not return unless we've been specifically cancelled.
 */
void XypiHub::run()
{
//	oscServer->start();
//	oscWorker->run();
	wsServer->start();
	info("Xypi::run(): Servers started and worker running ;)");
#ifdef SINGLE_THREADED_IO
	ioService.run();
#else
	std::vector<std::thread> ioThreads;
	for (int i = 0; i < threadCount; ++i) {
		ioThreads.emplace_back([this]() { ioService.run(); });
	}

	for (auto& thread : ioThreads) {
		if (thread.joinable()) thread.join();
	}
#endif
	info("Xypi::run(): io_context threads joined and completed. :o");
	oscWorker->stop();
	info("Xypi::run() shut down successfully. :)");
}

/*!
 * just finish! probably not safely. not sure if we need this even
 */
void XypiHub::stop()
{
	ioService.stop(); // should be posted perhaps?
	// it would be polite to wait for all those loose threads in the local ioThreads vector. TODO: perhaps make the vector of threads a member so we can do that.
	oscWorker->stop();
}