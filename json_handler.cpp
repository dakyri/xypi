#include "json_handler.h"

#include "jsonutil.h"
#include "work.h"

#include <functional>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

std::atomic<jobid_t> JSONHandler::jobid = 0;

//! essential data for describing an api command
struct api_t {
	std::function<json(JSONHandler*, const json&)> immediateProcessor;
	std::function<std::shared_ptr<work_t>(const std::string&, jobid_t, const json&)> workQueueFactory;
	bool urgent;
};

// clang-format off
//! map between api commands and properties
std::unordered_map<std::string, api_t> api {
	{"ping",		{nullptr,				&PingWork::create,				true}},
	{"get",			{&JSONHandler::getCmd,	nullptr,						false}},
	{"list",		{&JSONHandler::listCmd,	nullptr,						false}}
};
// clang-format on

/*!
 */
JSONHandler::JSONHandler(workq_t& workq, results_t& results) : m_workq(workq), m_results(results) {}

/*!
 * main processing hook:
 * takes json in, puts json out, and sets up the work queue in between.
 */
json JSONHandler::process(const json& request)
{
	auto cmd = jutil::need_s(request, "cmd");
	auto urgent = !jutil::opt_s(request, "urgent", "").empty();
	info("JSONHandler::process('{}')", cmd);
	auto ait = api.find(cmd);
	if (ait == api.end()) return jutil::errorJSON(fmt::format("Command '{}' not implemented.", cmd));

	json response;
	try {
		auto api_inf = ait->second;
		if (api_inf.immediateProcessor) {
			response = api_inf.immediateProcessor(this, request);
		} else {
			jobid_t id = ++jobid;
			auto work = api_inf.workQueueFactory(cmd, id, request);
			auto result = work->process(nullptr);
			if (result.first == work_t::work_status::WORK_QUEUED) {
				if (urgent || api_inf.urgent) {
					m_workq.push_front(std::move(work));
				} else {
					m_workq.push(std::move(work));
				}
				debug("queueing command {} with id {}", cmd, id);
			} else {
				m_results.insert(id, result_t(id, result));
				debug("storing immediate results for command {} with id {}", cmd, id);
			}
			response["id"] = id;
		}
	} catch (const nlohmann::json::type_error& e) {
		// from here, probably a bad conversion
		return jutil::errorJSON(fmt::format("JSON type error, {}", e.what()));
	} catch (const nlohmann::json::out_of_range& e) {
		// probably from json attempt to access a non-existent field
		return jutil::errorJSON(fmt::format("JSON out of range, {}", e.what()));
	}

	return response;
}

/*!
 * handle a 'get' command.
 *  \param json request we expect exactly 1 request parameter, 'id' which corresponds to the id of a previously queued request
 */
json JSONHandler::getCmd(json request)
{
	const id_t id = std::stoul(jutil::need_s(request, "id"));
	debug("getCmd({})", id);
	json response;
	if (id == 0) return jutil::errorJSON("Bad request id 0");
	auto result = m_results.fetch(id);
	if (result.has_value()) {
		response["state"] = "done";
		response["resp"] = result->result;
	} else {
		auto qorder = m_workq.find_qorder([id](const std::shared_ptr<work_t>& v) -> bool { return v->id == id; });
		if (qorder >= 0) {
			response["state"] = "enqueued";
			response["pos"] = qorder;
		} else {
			return jutil::errorJSON(fmt::format("Requested id, {}, is neither queued or completed", id));
		}
	}

	return response;
}

/*!
 * handle 'list' api command.
 * an instant commant that takes no parameters, and dumps the contensts of the current queues and maps
 */
json JSONHandler::listCmd(json request)
{
	json response;
	json workList;
	json resultList;

	m_workq.foreach([&workList](const std::shared_ptr<work_t>& v) { workList[std::to_string(v->id)] = v->toJson(); });
	m_results.foreach([&resultList](const id_t& k, const result_t& v) { resultList[std::to_string(k)] = v.result; });

	response["requests"] = workList;
	response["responses"] = resultList;
	return response;
}

void JSONHandler::debugDump()
{
	debug("api handler, current job id {}", jobid);
	m_results.foreach([](uint32_t k, result_t v) { debug("> result id {}", k); });
	m_workq.foreach([](const std::shared_ptr<work_t>& v) { debug("> work id {}", v->id); });
}
