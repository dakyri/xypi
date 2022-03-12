#pragma once

// TODO: ASAP find a better solution than this
#include "locked/queue.h"

#include <memory>

namespace oscapi {
	/*!
	 * base class for items submitted to the osc worker
	 */
	enum class work_type : uint8_t {
		none = 0,
		midi = 1,
		port = 2,
		button = 3, // buttons on the front plate handled by arduino
		device = 4, // some raw i2c from a device without specific handling in the arduino
		control = 5, // arduino analog ins
		xlmtr = 6 // xy data from an accelerometer. or perhaps touch screen
	};
	struct work_t {
		work_t(work_type _type = work_type::none): type(_type) {}
		virtual ~work_t() = default;

		const work_type type;
	};

	using workq_t = locked::queue<std::shared_ptr<work_t>>;
};