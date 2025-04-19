#pragma once

// TODO: ASAP find a better solution than this
#include "locked/queue.h"

#include <memory>

namespace oscapi {
	/*!
	 * base class for items submitted to the osc worker
	 */
	enum class cmd_t : uint8_t {
		none = 0,
		midi = 1,
		port = 2,
		button = 3, // buttons on the front plate handled by arduino
		device = 4, // some raw i2c from a device without specific handling in the arduino
		control = 5, // arduino analog ins
		xlmtr = 6 // xy data from an accelerometer. or perhaps touch screen
	};
	struct cmd {
		cmd(cmd_t _type = cmd_t::none): type(_type) {}
		virtual ~cmd() = default;

		const cmd_t type;
	};

	using cmdq_t = locked::queue<std::shared_ptr<cmd>>;
};