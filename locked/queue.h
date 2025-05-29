#pragma once

#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <utility>

namespace locked {
/*!
 * quick and dirty thread safe queue. better solutions exist but with tradeoffs (libcds, Honeycomb, folly, tbb)
 * a condition variable can be enabled to force blocking waits for a non empty queue
 */
template<typename T>
class queue
{
public:
	/*!
	 * clear any threads waiting on this queue. the queue will still function safely if not 'enabled' but will not block the front()
	 * method while waiting for data.
	 */
	void disableWait()
	{
		std::unique_lock<std::mutex> lock(mutex);
		isBlocking = false;
		ready.notify_all();
	}

	/*!
	 * enable a wait for data on the front() method
	 */
	void enable(bool _en=true) { isRunning = _en; }

	/*!
	 * enable a wait for data on the front() method
	 */
	void enableWait() { isBlocking = true; }

	/*!
	 * check that blocking mode is enabled
	 */
	bool waitEnabled() { return isBlocking; }

	/*!
	 * push r-value to the back of the queue
	 */
	void push(T&& value)
	{
		const std::unique_lock<std::mutex> lock(mutex);
		if (!isRunning) return;
		queue.push_back(std::move(value));
		ready.notify_all(); //! TODO: or notify one???
	}

	/*!
	 * push r-value to the front of the queue
	 */
	void push_front(T&& value)
	{
		const std::unique_lock<std::mutex> lock(mutex);
		if (!isRunning) return;
		queue.push_front(std::move(value));
		ready.notify_all(); //! TODO: or notify one???
	}

	/*!
	 * returns with the head element of the queue if it's not empty. if waiting is enabled, this call will block, waiting for
	 * data to arrive
	 */
	std::pair<T, bool> front()
	{
		std::unique_lock<std::mutex> conditionLock(mutex);
		ready.wait(conditionLock, [&]() { return !isBlocking || !queue.empty(); });
		if (queue.empty()) return { T(), false };

		return{ queue.front(), true };
	}

	std::pair<T, bool> front(const chrono::duration timeout)
	{
		std::unique_lock<std::mutex> conditionLock(mutex);
		ready.wait_for(conditionLock, timeout, [&]() { return !isBlocking || !queue.empty(); });
		if (queue.empty()) return { T(), false };

		return{ queue.front(), true };
	}

	/*!
	 * removes the v element from the queue
	 */
	void remove(T& v)
	{
		const std::unique_lock<std::mutex> lock(mutex);
		queue.remove(v);
	}

	/*!
	 * finds the index in the queue of the first element matching the given predicate
	 */
	int find_qorder(std::function<bool(const T&)> pred)
	{
		const std::unique_lock<std::mutex> lock(mutex);
		int qo = 0;
		for (auto const& it : queue) {
			if (pred(it)) return qo;
			++qo;
		}
		return -1;
	}

	/*!
	 * runs the given function over every queued element
	 */
	void foreach(std::function<void(const T&)> f)
	{
		const std::unique_lock<std::mutex> lock(mutex);
		for (auto const& it : queue) { f(it); }
	}

	/*!
	 * runs the given function over every queued element
	 */
	bool empty()
	{
		const std::unique_lock<std::mutex> lock(mutex);
		return queue.empty();
	}

private:
	std::list<T> queue;
	std::mutex mutex;
	std::condition_variable ready;
	bool isBlocking = true;
	bool isRunning = false;	//!< we have a running worker to remove things from the queue
};

}
