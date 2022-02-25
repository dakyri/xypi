#include "jsonutil.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace jutil {

nlohmann::json errorJSON(const std::string& msg)
{
	spdlog::debug("returning error from server: {}", msg);
	nlohmann::json response;
	response["error"] = msg;
	return response;
}

uint64_t opt_ull(const nlohmann::json& obj, const std::string& field, uint64_t dflt)
{
	return obj.contains(field) ? std::stoull(obj[field].get<std::string>()) : dflt;
}

std::string opt_s(const nlohmann::json& obj, const std::string& field, const std::string& dflt)
{
	return obj.contains(field) ? obj[field].get<std::string>() : dflt;
}

std::string need_s(const nlohmann::json& obj, const std::string& field)
{
	auto it = obj.find(field);
	if (it == obj.end()) throw std::invalid_argument(fmt::format("Expected '{}' field.", field));
	return *it;
}


}