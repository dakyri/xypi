#pragma once

#include "osc_workq.h"
#include "xyspi.h"

namespace oscapi {
	using midi_t = xyspi::midi_t;

	class MidiWork : public work_t {
	public:
		midi_t midi;
	};
};
