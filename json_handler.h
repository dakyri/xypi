#pragma once

#include "jsapi_cmdq.h"

#include <atomic>
#include <tuple>

/*!
 * \brief does the main processing of json commands and returns json
 */
class JSONHandler
{
public:
	JSONHandler(jsapi::workq_t& workq, jsapi::results_t& results);

	std::pair<bool, nlohmann::json> process(const nlohmann::json& request);

	nlohmann::json getCmd(nlohmann::json request);
	nlohmann::json listCmd(nlohmann::json request);

	void debugDump();

private:
	jsapi::workq_t& workq;
	jsapi::results_t& results;
	static std::atomic<jsapi::jobid_t> jobid;
};
