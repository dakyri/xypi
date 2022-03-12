#include "jsapi_work.h"

#include "jsonutil.h"

#include <boost/algorithm/hex.hpp>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using spdlog::info;
using spdlog::debug;


std::shared_ptr<jsapi::work_t> PingWork::create(const std::string& cmd, jsapi::jobid_t id, const nlohmann::json& request)
{
	return std::make_shared<PingWork>(cmd, id);
}

PingWork::PingWork(const std::string& cmd, jsapi::jobid_t id) : work_t(cmd, id, 0) {}

nlohmann::json PingWork::toJson()
{
	nlohmann::json jsonval;
	jsonval["cmd"] = cmd;
	return jsonval;
}

std::pair<jsapi::work_t::work_status, json> PingWork::process()
{
	json result;
	debug("PingWork::process(result {}))", id);
//	result["usb"] = dongle->ping();
	return {jsapi::work_t::work_status::WORK_IMMEDIATE, result};
}
