#pragma once

#include <cstddef>
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace jutil {

nlohmann::json errorJSON(const std::string& msg);
std::string need_s(const nlohmann::json& obj, const std::string& field);
uint64_t opt_ull(const nlohmann::json& obj, const std::string& field, uint64_t dflt);
std::string opt_s(const nlohmann::json& obj, const std::string& field, const std::string& dflt);

}
