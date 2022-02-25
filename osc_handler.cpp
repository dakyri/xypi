#include "osc_handler.h"

#include "work.h"

#include <functional>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

std::atomic<jobid_t> OSCHandler::jobid = 0;


//! essential data for describing an api command
struct api_t {
	std::function<json(OSCHandler*, const json&)> immediateProcessor;
	std::function<std::shared_ptr<work_t>(const std::string&, jobid_t, const json&)> workQueueFactory;
	bool urgent;
};

//! map between api commands and properties
std::unordered_map<std::string, api_t> api {
	{"ping",		{nullptr,				&PingWork::create,				true}},
};

/*!
 */
OSCHandler::OSCHandler(workq_t& workq, results_t& results) : m_workq(workq), m_results(results) {}

void OSCHandler::debugDump()
{
	debug("api handler, current job id {}", jobid);
	m_results.foreach([](uint32_t k, result_t v) { debug("> result id {}", k); });
	m_workq.foreach([](const std::shared_ptr<work_t>& v) { debug("> work id {}", v->id); });
}
