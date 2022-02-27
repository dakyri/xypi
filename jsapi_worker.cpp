#include "jsapi_worker.h"

#include <spdlog/spdlog.h>

#include <chrono>

using namespace std::chrono_literals;

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

/*!
 * \class JSApiWorker
 *   the bit that does stuff. we run in our own thread, outside of the asio threadpool, and inspect the
 * shared queue for new arrivals, do them and pop results into the results structure. the queue has a condition variable
 * and the main work thread blocks waiting for queue items to process. 
 */

JSApiWorker::JSApiWorker(jsapi::workq_t& _workq, jsapi::results_t& _results)
	: workq(_workq), results(_results)
{}

JSApiWorker::~JSApiWorker() { stop(); }

/*!
 * launch the worker and return immediately. we check that the dongle is open here, and if we have enough permission,
 * we list whatever files we find.
 */
void JSApiWorker::run()
{
	if (!isRunning.exchange(true)) {
		debug("JSApiWorker::run() launching main thread");
		myThread = std::thread([this]() { runner(); });
	}
}

/*!
 * stop the worker thread and wait until it completes. then clean up the dongle
 */
void JSApiWorker::stop()
{
	if (isRunning.exchange(false)) {
		workq.disableWait();
		if (myThread.joinable()) myThread.join();
	}
}

/*!
 * main body of the work queue processor
 */
void JSApiWorker::runner()
{
	workq.enableWait();
	while (isRunning) {
		auto optWork = workq.front();
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
				if (res.first == jsapi::work_t::work_status::WORK_SCREWED) {
					debug("JSApiWorker({}) fails with error message {}", currentResultId, r["error"].get<std::string>());
				}
				results.insert(currentResultId, jsapi::result_t(currentResultId, r));
			} catch (const std::exception& e) {
				error("JSApiWorker() gets exception: {}", e.what());
			}
			// we've either added a calculation to the results or had an exception and added an error to the resuls,
			// so now take the item from the queue. as we push to either end of the queue, our front element from before
			// is not necessarily still the front element, but the optWork ref will still be valid
			workq.remove(optWork.first);
		} else {
			if (isRunning && !workq.waitEnabled()) std::this_thread::sleep_for(10us);
		}
	}
}
