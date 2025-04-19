#pragma once

#include "osc_cmdq.h"
#include "xypi_midi.h"

namespace oscapi {
	using midi_t = xymidi::msg;

	class MidiMsg : public cmd {
	public:
		MidiMsg() : cmd(cmd_t::midi) {}
		uint8_t port;
		midi_t midi;
	};
};
