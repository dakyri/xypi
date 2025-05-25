#include "jsapi_cmd.h"

#include "jsonutil.h"

#include <boost/algorithm/hex.hpp>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using spdlog::info;
using spdlog::debug;

namespace jsapi {

std::shared_ptr<cmd_t> cmd_t::create(const std::string& cmd, cmd_id id, const nlohmann::json& request)
{
	return std::make_shared<cmd_t>(cmd, id);
}

nlohmann::json cmd_t::toJson()
{
	nlohmann::json jsonval;
	jsonval["cmd"] = cmd;
	return jsonval;
}

std::pair<jsapi::cmd_t::status, json> cmd_t::process()
{
	json result;
	debug("jsappi_cmd process {}", id);
//	result["usb"] = dongle->ping();
	return {cmd_t::status::CMD_IMMEDIATE, result};
}

}