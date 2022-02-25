#include "xypi_hub.h"

#include "osc_handler.h"
#include "osc_server.h"
#include "worker.h"

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
XypiHub::XypiHub(uint16_t serverPort, uint16_t threadCount)
	: threadCount(threadCount > 0 ? threadCount : 1)
{
	info("OSC server on port {} with {} threads.", serverPort, threadCount);
	oscHandler = std::make_shared<OSCHandler>(m_workQ, m_results);
	oscServer = std::make_unique<OSCServer>(oscService, serverPort, oscHandler);
	oscWorker = std::make_unique<Worker>(m_workQ, m_results);
}

XypiHub::~XypiHub() = default;

/*!
 * sets up the server, starts the worker thread, and runs the io context on a thread pool.
 * does not return unless we've been specifically cancelled.
 */
void XypiHub::run()
{
	oscServer->start();
	oscWorker->run();
	info("Kutya::run(): Server started and worker running ;)");
#ifdef SINGLE_THREADED_IO
	oscService.run();
#else
	std::vector<std::thread> ioThreads;
	for (int i = 0; i < threadCount; ++i) {
		ioThreads.emplace_back([this]() { oscService.run(); });
	}

	for (auto& thread : ioThreads) {
		if (thread.joinable()) thread.join();
	}
#endif
	info("Kutya::run(): io_context threads joined and completed. :o");
	oscWorker->stop();
	info("Kutya::run() shut down successfully. :)");
}

/*!
 * just finish! probably not safely. not sure if we need this even
 */
void XypiHub::stop()
{
	oscService.stop(); // should be posted perhaps?
	// it would be polite to wait for all those loose threads in the local ioThreads vector. TODO: perhaps make the vector of threads a member so we can do that.
	oscWorker->stop();
}