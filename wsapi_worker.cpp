#include "wsapi_worker.h"

#include <spdlog/spdlog.h>

#include <chrono>

using namespace std::chrono_literals;

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

/*!
 * \class JSApiWorker
 *	mainly here to do things that can't be done immediately in the io thread and which would unduly block that, in particular
 *	- any operations on external programs such as super collider
 */

WSApiWorker::WSApiWorker(wsapi::cmdq_t& _cmdq, wsapi::results_t& _results)
	: cmdq(_cmdq), results(_results)
{}

WSApiWorker::~WSApiWorker() { stop(); }

/*!
 * launch the worker and return immediately. we check that the dongle is open here, and if we have enough permission,
 * we list whatever files we find.
 */
void WSApiWorker::run()
{
	if (!isRunning.exchange(true)) {
		debug("JSApiWorker::run() launching main thread");
		myThread = std::thread([this]() { runner(); });
	}
}

/*!
 * stop the worker thread and wait until it completes. then clean up the dongle
 */
void WSApiWorker::stop()
{
	if (isRunning.exchange(false)) {
		cmdq.disableWait();
		if (myThread.joinable()) myThread.join();
	}
}

/*!
 * main body of the work queue processor
 */
void WSApiWorker::runner()
{
	cmdq.enableWait();
	while (isRunning) {
		auto optWork = cmdq.front();
		if (optWork.second) {
			// specifically make a new reference to the shared_ptr to work with, so we can leave the work at the top of q
			// workRef should still be valid even if it is no longer front ("ping" is pushed to front, not end of q)
			// work in process should still be marked as in queue in case we get polled while in progress (it would be highly
			// embarassing for a "get" to not be able to find an item just because we were working on it)
			auto& work = optWork.first;
			auto currentResultId = work->id;

			try {
				auto res = work->process();
				auto &r = res.second;
				debug("processed!");
				if (res.first == wsapi::cmd_t::status::CMD_ERROR) {
					debug("JSApiWorker({}) fails with error message {}", currentResultId, r["error"].get<std::string>());
				}
				results.insert(currentResultId, wsapi::result_t(currentResultId, r));
			} catch (const std::exception& e) {
				error("JSApiWorker() gets exception: {}", e.what());
			}
			// we've either done a thing, or had an exception
			// so now take the item from the queue. as we push to either end of the queue, our front element from before
			// is not necessarily still the front element, but the optWork ref will still be valid
			cmdq.remove(optWork.first);
		} else {
			if (isRunning && !cmdq.waitEnabled()) std::this_thread::sleep_for(10us);
		}
	}
}
