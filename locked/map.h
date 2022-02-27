#pragma once

#include <functional>
#include <mutex>
#include <utility>
#include <unordered_map>

namespace locked {
/*!
 * quick and dirty placeholder for a thread safe unordered map
 * better solutions exist but with tradeoffs: libcds, Honeycomb, facebook folly, intel tbb
 */
template<typename K, typename V>
class map
{
public:
	/*!
	 * safely inserts the element
	 */
	void insert(const K& key, V&& value)
	{
		const std::unique_lock<std::mutex> lock(m_mutex);
		m_map[key] = std::move(value);
	}

	/*!
	 * grabs the element from the map matching 'key', and removing it if found.
	 *  \return std::optional<V>	the value, or nullopt if not available
	 */
	std::pair<V, bool> fetch(const K& key)
	{
		const std::unique_lock<std::mutex> lock(m_mutex);
		auto it = m_map.find(key);
		if (it == m_map.end()) return { V(), false };
		auto v = it->second;
		m_map.erase(it);
		return{ v, true };
	}

	/*!
	 * runs the given function over every element in the map
	 */
	void foreach(std::function<void(const K&, const V&)> f)
	{
		const std::unique_lock<std::mutex> lock(m_mutex);
		for (auto const& it : m_map) { f(it.first, it.second); }
	}

private:
	std::unordered_map<K, V> m_map;
	std::mutex m_mutex;
};

}
