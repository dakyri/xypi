#pragma once

#include "xypi_midi.h"

#pragma once

// TODO: ASAP find a better solution than this
#include "locked/queue.h"

#include <memory>

namespace oscapi {

/*!
	* base class for items submitted to the osc worker
	*/
enum class cmd_type : uint8_t {
	none = 0,
	midi = 1,
	port = 2,
	button = 3, // buttons on the front plate handled by arduino
	device = 4, // some raw i2c from a device without specific handling in the arduino
	control = 5, // arduino analog ins
	xlmtr = 6 // xy data from an accelerometer. or perhaps touch screen
};

struct cmd_t {
	cmd_t(cmd_type _type = cmd_type::none): type(_type) {}
	virtual ~cmd_t() = default;

	const cmd_type type;
};

using cmdq_t = locked::queue<std::shared_ptr<cmd_t>>;
using midi_t = xymidi::msg;

class MidiMsg : public cmd_t {
public:
	MidiMsg() : cmd_t(cmd_type::midi) {}
	uint8_t port;
	midi_t midi;
};

};
