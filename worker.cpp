#include "worker.h"
#include "jsonutil.h"

#include <spdlog/spdlog.h>

#include <chrono>

using namespace std::chrono_literals;

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

/*!
 * \class Worker
 *   the bit that does stuff. we run in our own thread, outside of the asio threadpool, and inspect the
 * shared queue for new arrivals, do them and pop results into the results structure. the queue has a condition variable
 * and the main work thread blocks waiting for queue items to process. we also hold the interface to the
 * dongle and the hub here. specific details of the work are handled in the work structures (implemented in work.cpp).
 * TODO:
 *   - perhaps we could run inside the asio thread pool
 *   - perhaps we could wrap this into an asio::..::service either as a dongle server or queue listener. this is
 *		probably just an elegant wrapper around our thread and blocking queue architecture
 */

Worker::Worker(Dongle::permission permission, const std::string& passwd, workq_t& workq, results_t& results)
	: m_donglePermission(permission), m_donglePasswd(passwd), m_isRunning(false), m_workq(workq), m_results(results)
{}

Worker::~Worker() { stop(); }

/*!
 * launch the worker and return immediately. we check that the dongle is open here, and if we have enough permission,
 * we list whatever files we find.
 */
void Worker::run()
{
	if (openDongle()) {
		if (m_donglePermission == Dongle::permission::admin) {
			std::vector<DATA_FILE_LIST> dataFiles;
			auto ret = m_dongle.listFiles(FILE_DATA, dataFiles);
			if (ret == DONGLE_SUCCESS) {
				for (const auto& it : dataFiles) { info("=> data file {:#x}", it.m_FILEID); }
			} else if (ret != DONGLE_FILE_NOT_FOUND) {
				error("Worker::run() error listing data files: {}", Dongle::errorStr(ret));
			}
			std::vector<PRIKEY_FILE_LIST> privateKeyFiles;
			ret = m_dongle.listFiles(FILE_PRIKEY_RSA, privateKeyFiles);
			if (ret == DONGLE_SUCCESS) {
				for (const auto& it : privateKeyFiles) { info("=> rsa private key file {:#x}", it.m_FILEID); }
			} else if (ret != DONGLE_FILE_NOT_FOUND) {
				error("Worker::run() error listing rsa key files: {}", Dongle::errorStr(ret));
			}
			ret = m_dongle.listFiles(FILE_PRIKEY_ECCSM2, privateKeyFiles);
			if (ret == DONGLE_SUCCESS) {
				for (const auto& it : privateKeyFiles) { info("=> eccsm2 private key file {:#x}", it.m_FILEID); }
			} else if (ret != DONGLE_FILE_NOT_FOUND) {
				error("Worker::run() error listing eccsm2 key files: {}", Dongle::errorStr(ret));
			}
			std::vector<KEY_FILE_LIST> keyFiles;
			ret = m_dongle.listFiles(FILE_KEY, keyFiles);
			if (ret == DONGLE_SUCCESS) {
				for (const auto& it : keyFiles) { info("=> key file {:#x}", it.m_FILEID); }
			} else if (ret != DONGLE_FILE_NOT_FOUND) {
				error("Worker::run() error listing key files: {}", Dongle::errorStr(ret));
			}
		}
	}
	if (!m_isRunning.exchange(true)) {
		debug("Worker::run() launching main thread");
		m_thread = std::thread([this]() { runner(); });
	}
}

/*!
 * stop the worker thread and wait until it completes. then clean up the dongle
 */
void Worker::stop()
{
	if (m_isRunning.exchange(false)) {
		m_workq.disableWait();
		if (m_thread.joinable()) m_thread.join();
	}
	m_dongle.close();
}

/*!
 * opens the dongle. of course. returns true if it succeeds
 */
bool Worker::openDongle()
{
	m_dongle.close();
	debug("Enumerating dongles...");
	auto [dongles, uret] = Dongle::enumerate();
	if (dongles.size() == 0) {
		error("No dongles found. oops: {}", Dongle::errorStr(uret));
		return false;
	}
	debug("Opening the dongle of our dreams...");
	uret = m_dongle.open(0, m_donglePermission, m_donglePasswd);
	if (uret != DONGLE_SUCCESS) {
		error("Dongle open error: {}", Dongle::errorStr(uret));
		return false;
	}
	return true;
}

/*!
 * pretty much what it says
 */
void Worker::closeDongle() { m_dongle.close(); }

/*!
 * main body of the work queue processor
 */
void Worker::runner()
{
	m_workq.enableWait();
	while (m_isRunning) {
		auto optWork = m_workq.front();
		if (optWork.has_value()) {
			// specifically make a new reference to the shared_ptr to work with, so we can leave the work at the top of q
			// workRef should still be valid even if it is no longer front ("ping" is pushed to front, not end of q)
			// work in process should still be marked as in queue in case we get polled while in progress (it would be highly
			// embarassing for a "get" to not be able to find an item just because we were working on it)
			auto& work = *optWork;
			auto currentResultId = work->id;
			debug("Worker::runner() processing result id {} work id {}", currentResultId, work->workId);
			try {
				auto [res, r] = work->process(&m_dongle);
				debug("processed!");
				if (res != 0) {
					if (m_dongle.shouldReboot()) {
						info("Worker({}): possibly recoverable error. Attempting hub reboot.", currentResultId);
						if (m_hub.reboot() != 0) {
							debug("Worker({}): hub reboot fails: {}", currentResultId, m_hub.lastError());
							m_dongle.close();
						} else if (!openDongle()) {
							debug("Worker({}): dongle reopenning fails!", currentResultId);
						} else {
							std::tie(res, r) = work->process(&m_dongle);
							if (res != 0) {
								debug("Worker({}) fails after attempted dongle reboot with error code {:#x}, message {}.", currentResultId,
									  res, r["error"].get<std::string>());
							} else {
								debug("Worker({}) succeeds after attempted reboot.", currentResultId);
							}
						}
					} else {
						debug("Worker({}) fails with error code {:#x}, message {}", currentResultId, res, r["error"].get<std::string>());
					}
				}
				m_results.insert(currentResultId, result_t(currentResultId, r));
			} catch (const std::exception& e) {
				error("Worker({}) gets exception: {}", currentResultId, e.what());
				m_results.insert(currentResultId,
								 result_t(currentResultId,
										  jutil::errorJSON(fmt::format("Internal error on job with result id {}, work id {}: {}",
																	   currentResultId, work->workId, e.what()))));
			}
			// we've either added a calculation to the results or had an exception and added an error to the resuls,
			// so now take the item from the queue. as we push to either end of the queue, our front element from before
			// is not necessarily still the front element, but the optWork ref will still be valid
			m_workq.remove(*optWork);
		} else {
			if (m_isRunning && !m_workq.waitEnabled()) std::this_thread::sleep_for(10us);
		}
	}
}
