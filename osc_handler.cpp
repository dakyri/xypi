#include "osc_handler.h"

#include "osc_work.h"

#include <functional>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

//! essential data for describing an api command
struct api_t {
	std::function<json(OSCHandler*, const json&)> immediateProcessor;
	std::function<std::shared_ptr<oscapi::work_t>(const std::string&, const json&)> workQueueFactory;
	bool urgent;
};

//! map between api commands and properties
std::unordered_map<std::string, api_t> api {
//	{"ping",		{nullptr,				&PingWork::create,				true}},
};

/*!
 */
OSCHandler::OSCHandler(oscapi::workq_t& workq) : m_workq(workq) {}

void OSCHandler::debugDump()
{
	debug("api handler, current job id {}");
//	m_workq.foreach([](const std::shared_ptr<oscapi::work_t>& v) { debug("> work id {}", v->id); });
}
