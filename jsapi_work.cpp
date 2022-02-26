#include "jsapi_work.h"

#include "jsonutil.h"

#include <boost/algorithm/hex.hpp>
#include <openssl/aes.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using spdlog::info;
using spdlog::debug;

PingWork::PingWork(const std::string& cmd, id_t id) : work_t(cmd, id, 0) {}

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
	if (!dongle) return {Dongle::error::DONGLE_REQUIRED, result};
	result["usb"] = dongle->ping();
	return {0, result};
}
