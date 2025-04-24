#pragma once

#include <stdint.h>

namespace xymidi {
enum class bus_cmd: uint8_t {
	greets = 'h',
	sendMidiAny = '0', // or every
	sendMidiPort1 = '1', // specific ports. we probably won't have more than 1 or 2 extras
	sendMidiPort2 = '2',
	sendMidiPort3 = '3',
	sendMidiPort4 = '4',
	sendMidiPort5 = '5',
	sendMidiPort6 = '6',
	sendMidiPort7 = '7',
	sendMidiPort8 = '8',
	sendMidiPort9 = '9',
	setMidiPortCmd = '>',
	sendButton = 'b',
	sendDevice = 'i', // output from a generic i2c device that we haven't got a specific command for
	ok = 'k'
};

enum class midi_port_cmd : uint8_t { // 4 bit bit field
	enableThru = 1,
	disableThru = 2
};

inline bool isSysCmd(uint8_t cmd) { return (cmd & 0xf0) != 0; }
inline bool isCmdByte(uint8_t _byte) { return (_byte & 0x80) != 0; }
inline uint8_t getCCCmd(uint8_t _byte) { return (_byte & 0xf0); }
inline uint8_t getCCChan(uint8_t _byte) { return (_byte & 0x0f); }

enum class cmd : uint8_t {
	nul			= 0x00,
	noteOff		= 0x80,
	noteOn		= 0x90,
	keyPress	= 0xa0,
	ctrl		= 0xb0,
	prog		= 0xc0,
	chanPress	= 0xd0,
	bend		= 0xe0,

	sysxStart	= 0xf0,
	timeCode	= 0xf1,
	songPos		= 0xf2,
	songSel		= 0xf3,
	cableMsg	= 0xf5,
	tuneReq		= 0xf6,
	sysxEnd		= 0xf7,
	clock		= 0xf8,
	start		= 0xfa,
	cont		= 0xfb,
	stop		= 0xfc,
	sensing		= 0xfe,
	sysReset	= 0xff
};

inline uint8_t operator | (cmd op, uint8_t t) { return static_cast<uint8_t>(op) | t; }
inline uint8_t operator & (cmd op, uint8_t t) { return static_cast<uint8_t>(op) & t; }
inline uint8_t operator ^ (cmd op, uint8_t t) { return static_cast<uint8_t>(op) ^ t; }
inline uint8_t ot(cmd op) { return static_cast<uint8_t>(op); }
inline cmd ot(uint8_t op) { return static_cast<cmd>(op); }

enum class midi_ctrl : uint8_t {
	modulation		= 0x01,
	breathController = 0x02,
	footController	= 0x04,
	portamentoTime	= 0x05,
	dataEntry		= 0x06,
	mainVolume		= 0x07,
	midiBalance		= 0x08,
	pan				= 0x0a,
	expressionCtrl	= 0x0b,
	general_1		= 0x10,
	general_2		= 0x11,
	general_3		= 0x12,
	general_4		= 0x13,
	sustainPedal	= 0x40,
	portamento		= 0x41,
	sostenuto		= 0x42,
	softPedal		= 0x43,
	hold_2			= 0x45,
	general_5		= 0x50,
	general_6		= 0x51,
	general_7		= 0x52,
	general_8		= 0x53,
	effects_depth	= 0x5b,
	tremolo_depth	= 0x5c,
	chorus_depth	= 0x5d,
	celeste_depth	= 0x5e,
	phaser_depth	= 0x5f,
	data_increment	= 0x60,
	data_decrement	= 0x61,
	reset_all		= 0x79,
	local_control	= 0x7a,
	all_notes_off	= 0x7b,
	omni_mode_off	= 0x7c,
	omni_mode_on	= 0x7d,
	mono_mode_on	= 0x7e,
	poly_mode_on	= 0x7f,

	tempo_change	= 0x51
};

#pragma pack(push, 1)

struct msg {
	msg(uint8_t _cmd=0, uint8_t _val1=0, uint8_t _val2=0, uint8_t _port=0) : cmd(_cmd), val1(_val1), val2(_val2), port(_port) { }

	static inline msg noteon(uint8_t chan, uint8_t note, uint8_t vel) { return { cmd::noteOn | (chan & 0xf), note, vel }; }
	static inline msg noteon(uint8_t args[3]) { return { cmd::noteOn | (args[0] & 0xf), args[1], args[2] }; }
	static inline msg noteoff(uint8_t chan, uint8_t note, uint8_t vel) { return { cmd::noteOff | (chan & 0xf), note, vel }; }
	static inline msg keypress(uint8_t chan, uint8_t note, uint8_t vel) { return { cmd::keyPress | (chan & 0xf), note, vel }; }
	static inline msg control(uint8_t chan, uint8_t tgt, uint8_t amt) { return { cmd::ctrl | (chan & 0xf), tgt, amt }; }
	static inline msg prog(uint8_t chan, uint8_t prog) { return { cmd::prog | (chan & 0xf), prog }; }
	static inline msg chanpress(uint8_t chan, uint8_t press) { return { cmd::chanPress | (chan & 0xf), press }; }
	static inline msg bend(uint8_t chan, uint16_t bend) { return { cmd::bend | (chan & 0xf), (uint8_t) ((bend >> 7) & 0x7f), (uint8_t) (bend & 0x7f) }; }
	static inline msg timecode(uint8_t typ, uint8_t v) { return { (uint8_t)cmd::timeCode, typ, v }; }
	static inline msg songpos(uint16_t pos) { return { (uint8_t)cmd::songPos, (uint8_t) ((pos >> 7) & 0x7f), (uint8_t) (pos & 0x7f) }; }
	static inline msg songsel(uint8_t sel) { return { (uint8_t)cmd::songSel, sel }; }
	static inline msg tune() { return {(uint8_t)cmd::tuneReq}; }
	static inline msg clock() { return {(uint8_t)cmd::clock}; }
	static inline msg start() { return {(uint8_t)cmd::start}; }
	static inline msg cont() { return {(uint8_t)cmd::cont}; }
	static inline msg stop() { return {(uint8_t)cmd::stop}; }

	inline uint8_t channel() { return cmd & 0xf; }

	uint8_t port;
	uint8_t cmd;
	uint8_t val1;
	uint8_t val2;
};

struct midi_port_cmd_t {
	uint8_t port : 4;
	uint8_t cmd : 4;
};

#pragma pack(pop)

};