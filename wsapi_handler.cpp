#include "wsapi_handler.h"

#include "jsonutil.h"
#include "wsapi_cmd.h"

#include <functional>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

std::atomic<wsapi::cmd_id> WSApiHandler::cmdid = 0;

//! essential data for describing an api command
struct api_t {
	std::function<json(WSApiHandler*, const json&)> immediateProcessor;
	std::function<std::shared_ptr<wsapi::cmd_t>(const std::string&, wsapi::cmd_id, const json&)> workQueueFactory;
	bool urgent;
};

// clang-format off
//! map between api commands and properties
std::unordered_map<std::string, api_t> api {
//	{"ping",		{nullptr,				&PingWork::create,				true}},
	{"get",			{&WSApiHandler::getCmd,	nullptr,						false}},
	{"list",		{&WSApiHandler::listCmd,	nullptr,						false}}
};
// clang-format on

/*!
 */
WSApiHandler::WSApiHandler(xymsg::q_t &_spiInQ, xymsg::q_t &_oscInQ, wsapi::cmdq_t& _cmdq, wsapi::results_t& _results)
	: spiInQ(_spiInQ), oscInQ(_oscInQ), cmdq(_cmdq), results(_results) {}

/*!
 * main processing hook:
 * takes json in, puts json out, and sets up the command queue in between.
 */
std::pair<bool, json> WSApiHandler::process(const json& request)
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
			auto id = ++cmdid;
			auto work = api_inf.workQueueFactory(cmd, id, request);
			auto result = work->process();
			if (result.first == wsapi::cmd_t::status::CMD_SCHEDULED) {
				if (urgent || api_inf.urgent) {
					cmdq.push_front(std::move(work));
				} else {
					cmdq.push(std::move(work));
				}
				debug("queueing command {} with id {}", cmd, id);
			} else {
				results.insert(id, wsapi::result_t(id, result));
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
json WSApiHandler::getCmd(json request)
{
	const wsapi::cmd_id id = std::stoul(jutil::need_s(request, "id"));
	debug("getCmd({})", id);
	json response;
	if (id == 0) return jutil::errorJSON("Bad request id 0");
	auto result = results.fetch(id);
	if (result.second) {
		response["state"] = "done";
		response["resp"] = result.first.result;
	} else {
		auto qorder = cmdq.find_qorder([id](const std::shared_ptr<wsapi::cmd_t>& v) -> bool { return v->id == id; });
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
json WSApiHandler::listCmd(json request)
{
	json response;
	json workList;
	json resultList;

	cmdq.foreach([&workList](const std::shared_ptr<wsapi::cmd_t>& v) { workList[std::to_string(v->id)] = v->toJson(); });
	results.foreach([&resultList](const wsapi::cmd_id& k, const wsapi::result_t& v) { resultList[std::to_string(k)] = v.result; });

	response["requests"] = workList;
	response["responses"] = resultList;
	return response;
}

void WSApiHandler::debugDump()
{
	debug("api handler, current job id {}", (int)cmdid);
	results.foreach([](uint32_t k, wsapi::result_t v) { debug("> result id {}", k); });
	cmdq.foreach([](const std::shared_ptr<wsapi::cmd_t>& v) { debug("> work id {}", v->id); });
}
