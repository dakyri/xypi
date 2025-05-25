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
		std::unique_lock<std::mutex> lock(m_mutex);
		m_enabled = false;
		m_ready.notify_all();
	}

	/*!
	 * enable a wait for data on the front() method
	 */
	void enableWait() { m_enabled = true; }

	/*!
	 * check that blocking mode is enabled
	 */
	bool waitEnabled() { return m_enabled; }

	/*!
	 * push r-value to the back of the queue
	 */
	void push(T&& value)
	{
		const std::unique_lock<std::mutex> lock(m_mutex);
		m_queue.push_back(std::move(value));
		m_ready.notify_all();
	}

	/*!
	 * push r-value to the front of the queue
	 */
	void push_front(T&& value)
	{
		const std::unique_lock<std::mutex> lock(m_mutex);
		m_queue.push_front(std::move(value));
		m_ready.notify_all();
	}

	/*!
	 * returns with the head element of the queue if it's not empty. if waiting is enabled, this call will block, waiting for
	 * data to arrive
	 */
	std::pair<T, bool> front()
	{
		std::unique_lock<std::mutex> conditionLock(m_mutex);
		m_ready.wait(conditionLock, [&]() { return !m_enabled || !m_queue.empty(); });
		if (m_queue.empty()) return { T(), false };

		return{ m_queue.front(), true };
	}

	std::pair<T, bool> front(const chrono::duration timeout)
	{
		std::unique_lock<std::mutex> conditionLock(m_mutex);
		m_ready.wait_for(conditionLock, timeout, [&]() { return !m_enabled || !m_queue.empty(); });
		if (m_queue.empty()) return { T(), false };

		return{ m_queue.front(), true };
	}

	/*!
	 * removes the v element from the queue
	 */
	void remove(T& v)
	{
		const std::unique_lock<std::mutex> lock(m_mutex);
		m_queue.remove(v);
	}

	/*!
	 * finds the index in the queue of the first element matching the given predicate
	 */
	int find_qorder(std::function<bool(const T&)> pred)
	{
		const std::unique_lock<std::mutex> lock(m_mutex);
		int qo = 0;
		for (auto const& it : m_queue) {
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
		const std::unique_lock<std::mutex> lock(m_mutex);
		for (auto const& it : m_queue) { f(it); }
	}

	/*!
	 * runs the given function over every queued element
	 */
	bool empty()
	{
		const std::unique_lock<std::mutex> lock(m_mutex);
		return m_queue.empty();
	}

private:
	std::list<T> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_ready;
	bool m_enabled = true;
};

}
