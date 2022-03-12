#pragma once

#include "osc_workq.h"
#include "xyspi.h"

namespace oscapi {
	using midi_t = xyspi::midi_t;

	class MidiMsg : public work_t {
	public:
		MidiMsg() : work_t(work_type::midi) {}
		uint8_t port;
		midi_t midi;
	};
};
