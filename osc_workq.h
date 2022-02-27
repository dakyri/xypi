#pragma once

// TODO: ASAP find a better solution than this
#include "locked/queue.h"

#include <memory>

namespace oscapi {
	/*!
	 * base class for items submitted to the osc worker
	 * work is done via the inherited 'process' virtual
	 */
	struct work_t {
		work_t() {}
		virtual ~work_t() = default;
	};

	using workq_t = locked::queue<std::shared_ptr<work_t>>;
};