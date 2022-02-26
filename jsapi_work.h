#pragma once

#include "jsapi_workq.h"

class PingWork : public jsapi::work_t
{
public:
	static std::shared_ptr<work_t> create(const std::string& cmd, jsapi::jobid_t id, const nlohmann::json& req);
	PingWork(const std::string& cmd, jsapi::jobid_t id);

	virtual nlohmann::json toJson() override;
	virtual std::pair<jsapi::work_t::work_status, nlohmann::json> process() override;
};
