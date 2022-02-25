#pragma once

#include "workqueue.h"

class Dongle;

class PingWork : public work_t
{
public:
	static std::shared_ptr<work_t> create(const std::string& cmd, jobid_t id, const nlohmann::json& req);
	PingWork(const std::string& cmd, jobid_t id);

	virtual nlohmann::json toJson() override;
	virtual std::pair<uint32_t, nlohmann::json> process(Dongle* dongle) override;
};
