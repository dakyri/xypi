#include "json_handler.h"

#include "jsonutil.h"
#include "jsapi_work.h"

#include <functional>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

std::atomic<jsapi::jobid_t> JSONHandler::jobid = 0;

//! essential data for describing an api command
struct api_t {
	std::function<json(JSONHandler*, const json&)> immediateProcessor;
	std::function<std::shared_ptr<jsapi::work_t>(const std::string&, jsapi::jobid_t, const json&)> workQueueFactory;
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
JSONHandler::JSONHandler(jsapi::workq_t& _workq, jsapi::results_t& _results) : workq(_workq), results(_results) {}

/*!
 * main processing hook:
 * takes json in, puts json out, and sets up the work queue in between.
 */
std::pair<bool, json> JSONHandler::process(const json& request)
{
	auto cmd = jutil::need_s(request, "cmd");
	auto urgent = !jutil::opt_s(request, "urgent", "").empty();
	info("JSONHandler::process('{}')", cmd);
	auto ait = api.find(cmd);
	if (ait == api.end()) return {true, jutil::errorJSON(fmt::format("Command '{}' not implemented.", cmd))};

	json response;
	try {
		auto api_inf = ait->second;
		if (api_inf.immediateProcessor) {
			response = api_inf.immediateProcessor(this, request);
		} else {
			auto id = ++jobid;
			auto work = api_inf.workQueueFactory(cmd, id, request);
			auto result = work->process();
			if (result.first == jsapi::work_t::work_status::WORK_QUEUED) {
				if (urgent || api_inf.urgent) {
					workq.push_front(std::move(work));
				} else {
					workq.push(std::move(work));
				}
				debug("queueing command {} with id {}", cmd, id);
			} else {
				results.insert(id, jsapi::result_t(id, result));
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

	return {true, response};
}

/*!
 * handle a 'get' command.
 *  \param json request we expect exactly 1 request parameter, 'id' which corresponds to the id of a previously queued request
 */
json JSONHandler::getCmd(json request)
{
	const jsapi::jobid_t id = std::stoul(jutil::need_s(request, "id"));
	debug("getCmd({})", id);
	json response;
	if (id == 0) return jutil::errorJSON("Bad request id 0");
	auto result = results.fetch(id);
	if (result.second) {
		response["state"] = "done";
		response["resp"] = result.first.result;
	} else {
		auto qorder = workq.find_qorder([id](const std::shared_ptr<jsapi::work_t>& v) -> bool { return v->id == id; });
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

	workq.foreach([&workList](const std::shared_ptr<jsapi::work_t>& v) { workList[std::to_string(v->id)] = v->toJson(); });
	results.foreach([&resultList](const jsapi::jobid_t& k, const jsapi::result_t& v) { resultList[std::to_string(k)] = v.result; });

	response["requests"] = workList;
	response["responses"] = resultList;
	return response;
}

void JSONHandler::debugDump()
{
	debug("api handler, current job id {}", jobid);
	results.foreach([](uint32_t k, jsapi::result_t v) { debug("> result id {}", k); });
	workq.foreach([](const std::shared_ptr<jsapi::work_t>& v) { debug("> work id {}", v->id); });
}
