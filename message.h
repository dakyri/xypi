#pragma once

#include "xypi_midi.h"
#include "xycfg.h"

#pragma once

// TODO: ASAP find a better solution than this
#include "locked/queue.h"

#include <memory>

namespace xymsg {

/*!
 * base class for messages and commands submitted to the various midi/osc/hardware workers
 * these are messages or commands that are sent to and from the duino via SPI, and broadcast via OSC, and possibly sent to locally connected midi devices
 */
enum class typ : uint8_t {
	none = 0,
	midi = 1,
	midi_list = 2,
	config_button = 3,
	config_pedal = 4,
	config_xlrm8r = 5,
	tempo = 6,
	duino_cmd = 7
};

struct msg_t {
	msg_t(typ _type = typ::none): type(_type) {}
	virtual ~msg_t() = default;

	const typ type;
};

using q_t = locked::queue<std::shared_ptr<msg_t>>;
using midi_t = xymidi::msg;

class MidiMsg : public msg_t {
public:
	MidiMsg() : msg_t(typ::midi) {}
	midi_t midi;
};


class MidiListMsg : public msg_t {
public:
	MidiListMsg() : msg_t(typ::midi_list) {}
	std::vector<midi_t> midi;
};


class ConfigButtonMsg : public msg_t {
public:
	ConfigButtonMsg() : msg_t(typ::config_button), which(0) {}
	uint8_t which;
	config::button cfg;
};


class ConfigPedalMsg : public msg_t {
public:
	ConfigPedalMsg() : msg_t(typ::config_pedal), which(0) {}
	uint8_t which;
	config::pedal cfg;
};


class ConfigXlm8rMsg : public msg_t {
public:
	ConfigXlm8rMsg() : msg_t(typ::config_xlrm8r), which(0) {}
	uint8_t which;
	config::xlrm8r cfg;
};


class TempoMsg : public msg_t {
public:
	TempoMsg(const float _tempo=120) : msg_t(typ::tempo), tempo(_tempo) {}
	float tempo; // a 32 bit float!!
};



class CmdMsg : public msg_t {
public:
	CmdMsg(uint8_t _cmd) : msg_t(typ::tempo), cmd(0) {}
	uint8_t cmd;
};


};
