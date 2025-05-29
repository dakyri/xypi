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
		const std::unique_lock<std::mutex> lock(mutex);
		map[key] = std::move(value);
	}

	/*!
	 * grabs the element from the map matching 'key', and removing it if found.
	 *  \return std::optional<V>	the value, or nullopt if not available
	 */
	std::pair<V, bool> fetch(const K& key)
	{
		const std::unique_lock<std::mutex> lock(mutex);
		auto it = map.find(key);
		if (it == map.end()) return { V(), false };
		auto v = it->second;
		map.erase(it);
		return{ v, true };
	}

	/*!
	 * runs the given function over every element in the map
	 */
	void foreach(std::function<void(const K&, const V&)> f)
	{
		const std::unique_lock<std::mutex> lock(mutex);
		for (auto const& it : map) { f(it.first, it.second); }
	}

private:
	std::unordered_map<K, V> map;
	std::mutex mutex;
};

}
