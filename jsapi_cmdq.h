#pragma once

// TODO: ASAP find a better solution than this
#include "locked/map.h"
#include "locked/queue.h"

#include <cstdint>
#include <memory>

#include <nlohmann/json.hpp>

namespace jsapi {
	using cmd_id = uint32_t; //!< id for the command
	/*!
	 * base class for commands sent to the hub via the jaon api
	 * some of these might be immediate, and some scheduled
	 */
	struct cmd {
		enum class status : uint32_t {
			CMD_SCHEDULED, CMD_IMMEDIATE, CMD_ERROR
		};
		work_t(const std::string& c, jobid_t i, workid_t wi) : cmd(c), id(i), workId(wi) {}
		virtual ~work_t() = default;
		/*! \return - the details of the command that created this work item as json */
		virtual nlohmann::json toJson() = 0;
		virtual std::pair<status, nlohmann::json> process() = 0;

		std::string cmd; /*!< the command of the api request that made this work */
		cmdid_t id = 0; /*!< the job id allocated by the api handler and returned by the initial request */
	};

	/*!
	 * base class for cmd results
	 */
	struct result_t {
		result_t(cmd_id i, const nlohmann::json r) : id(i), result(std::move(r)) {}
		result_t() : id(0) {}

		jobid_t id = 0;
		nlohmann::json result;
	};

	using cmd_q = locked::queue<std::shared_ptr<cmd_t>>;
};