#pragma once

#include "workqueue.h"

#include <atomic>

/*!
 * \brief does the main processing of json commands and returns json
 */
class JSONHandler
{
public:
	JSONHandler(workq_t& workq, results_t& results);

	nlohmann::json process(const nlohmann::json& request);

	nlohmann::json getCmd(nlohmann::json request);
	nlohmann::json listCmd(nlohmann::json request);

	void debugDump();

private:
	workq_t& m_workq;
	results_t& m_results;
	static std::atomic<jobid_t> jobid;
};
