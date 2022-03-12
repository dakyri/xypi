#include "osc_worker.h"

#include <spdlog/spdlog.h>

#include <chrono>

using namespace std::chrono_literals;

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

/*!
 * \class OSCWorker
 *   the bit that does stuff. we run in our own thread, outside of the asio threadpool, and inspect the
 * shared queue for new arrivals, do them and pop results into the results structure. the queue has a condition variable
 * and the main work thread blocks waiting for queue items to process.
 *  TODO:
 * - some commonality with the ws/json api worker, and perhaps with whatever worker handles the spi bus. maybe these should
 *		be refactored into a base class
 */

OSCWorker::OSCWorker(OSCServer &_oscurver, oscapi::workq_t& _workq)
	: oscurver(_oscurver), workq(_workq)
{}

OSCWorker::~OSCWorker() { stop(); }

/*!
 * launch the OSCWorker and return immediately. we check that the dongle is open here, and if we have enough permission,
 * we list whatever files we find.
 */
void OSCWorker::run()
{
	if (!isRunning.exchange(true)) {
		debug("OSCWorker::run() launching main thread");
		myThread = std::thread([this]() { runner(); });
	}
}

/*!
 * stop the worker thread and wait until it completes. then clean up the dongle
 */
void OSCWorker::stop()
{
	if (isRunning.exchange(false)) {
		workq.disableWait();
		if (myThread.joinable()) myThread.join();
	}
}

/*!
 * main body of the work queue processor
 */
void OSCWorker::runner()
{
	workq.enableWait();
	while (isRunning) {
		// TODO: perhaps the whole current queue could be bundled
		auto optWork = workq.front();
		if (optWork.second) {
			// specifically make a new reference to the shared_ptr to work with, so we can leave the work at the top of q
			// workRef should still be valid even if it is no longer front
			auto& work = optWork.first;
			try {
				oscurver.send_message(work);
			} catch (const std::exception& e) {
				error("OSCWorker() gets exception: {}", e.what());
			}
			workq.remove(optWork.first); // now it's safe to remove!
		} else {
			if (isRunning && !workq.waitEnabled()) std::this_thread::sleep_for(10us);
		}
	}
}
