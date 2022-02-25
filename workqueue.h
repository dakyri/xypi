#pragma once

// TODO: ASAP find a better solution than this
#include "locked/map.h"
#include "locked/queue.h"

#include <cstdint>
#include <memory>

#include <nlohmann/json.hpp>

class Dongle;

using jobid_t = uint32_t; //!< internal id for the queued work
using workid_t = uint64_t; //!< type for the chimera work id that a job corresponds to

/*!
 * base class for items submitted to the worker
 * work is done via the inherited 'process' virtual
 */
struct work_t {
	enum class work_status: uint32_t {
		WORK_QUEUED, WORK_IMMEDIATE, WORK_SCREWED
	};
	work_t(const std::string& c, jobid_t i, workid_t wi) : cmd(c), id(i), workId(wi) {}
	virtual ~work_t() = default;
	/*! \return - the details of the command that created this work item as json */
	virtual nlohmann::json toJson() = 0;
	virtual std::pair<work_status, nlohmann::json> process(Dongle* dongle) = 0;

	std::string cmd; /*!< the command of the api request that made this work */
	jobid_t id = 0; /*!< the job id allocated by the api handler and returned by the initial request */
	uint64_t workId = 0; /*!< the work id passed in with the command */
};

/*!
 * base class for work results
 */
struct result_t {
	result_t(jobid_t i, const nlohmann::json r) : id(i), result(std::move(r)) {}
	result_t() : id(0) {}

	jobid_t id = 0;
	nlohmann::json result;
};

using workq_t = locked::queue<std::shared_ptr<work_t>>;
using results_t = locked::map<uint32_t, result_t>;
