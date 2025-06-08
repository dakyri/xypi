#pragma once

// TODO: ASAP find a better solution than this
#include "locked/map.h"
#include "locked/queue.h"

#include <cstdint>
#include <memory>

#include <nlohmann/json.hpp>

namespace wsapi {

using cmd_id = uint32_t; //!< id for the command
/*!
 * base class for commands sent to the hub via the json api
 * some of these might be immediate, and some scheduled
 *	- send config information to the duino via spi
 *	- send commands to supercollider locally
 *	- command to other running audio programs
 *	- ping, pong and general diagnostic stuff
 *	- diagnostics, stop/start/restart on the osc server
 */
struct cmd_t {
	static std::shared_ptr<cmd_t> create(const std::string& cmd, wsapi::cmd_id id, const nlohmann::json& req);

	enum class status : uint32_t {
		CMD_SCHEDULED, CMD_IMMEDIATE, CMD_ERROR
	};
	cmd_t(const std::string& c, cmd_id i) : cmd(c), id(i) {}
	virtual ~cmd_t() = default;
	/*! \return - the details of the command that created this work item as json */
	virtual nlohmann::json toJson();
	virtual std::pair<status, nlohmann::json> process();

	std::string cmd; /*!< the command of the api request that made this work */
	cmd_id id = 0; /*!< the job id allocated by the api handler and returned by the initial request */
};

/*!
 * base class for cmd results
 */
struct result_t {
	result_t(cmd_id i, const nlohmann::json r) : id(i), result(std::move(r)) {}
	result_t() : id(0) {}

	cmd_id id = 0;
	nlohmann::json result;
};

using cmdq_t = locked::queue<std::shared_ptr<cmd_t>>;
using results_t = locked::map<uint32_t, result_t>;

};