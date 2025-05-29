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
 *   the bit that does stuff. we run in our own thread, outside of the asio threadpool. we are basically scanning our messsage queue
 * and broadcasting anything appropriate to the OSC socket. Presumably this queue is added to be either from the duino via the spi input, or by any locally
 * connected midi ports
 *  TODO:
 * - some commonality with the ws/json api worker
 */

OSCWorker::OSCWorker(OSCServer &_oscurver, xymsg::q_t& _msgq)
	: oscurver(_oscurver), msgq(_msgq)
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
		msgq.disableWait();
		msgq.enable(false);
		if (myThread.joinable()) myThread.join();
	}
}

/*!
 * main body of the work queue processor
 */
void OSCWorker::runner()
{
	msgq.enable();
	msgq.enableWait();
	while (isRunning) {
		// TODO: perhaps the whole current queue could be bundled
		auto optMsg = msgq.front();
		if (optMsg.second) {
			// specifically make a new reference to the shared_ptr to work with, so we can leave the work at the top of q
			// workRef should still be valid even if it is no longer front
			auto& msg = optMsg.first;
			try {
				oscurver.send_message(msg);
			} catch (const std::exception& e) {
				error("OSCWorker() gets exception: {}", e.what());
			}
			msgq.remove(optMsg.first); // now it's safe to remove!
		} else {
			if (isRunning && !msgq.waitEnabled()) std::this_thread::sleep_for(10us);
		}
	}
}
