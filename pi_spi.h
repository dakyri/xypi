#pragma once

#include <thread>
#include "osc_cmd.h"


enum spi_io_state_t: uint8_t {
	command_byte = 0,	//!< processing a command byte
	midi_data = 1,		//!< processing midi bytes
	midi_data_1 = 2,		//!< processing midi bytes
	midi_data_2 = 3,		//!< processing midi bytes
	tempo_data = 4,
	tempo_data_1 = 5,
	tempo_data_2 = 6,
	tempo_data_3 = 7,
	diag_message_length = 8,
	diag_message_data = 9,
	filler = 10,
};


class PiSpi {
public:
	PiSpi(oscapi::msgq_t& _inQ, oscapi::msgq_t& _outQ);
	~PiSpi();

	bool start();
	void stop();

protected:
	std::thread spiThread;
	std::atomic<bool> isRunning;
	bool isSpiOpen;

	spi_io_state_t spi_in_state = command_byte;
	float incoming_tempo;
	uint8_t n_midi_cmd_incoming;
	bool tempo_requested;
	bool set_tempo;
	uint8_t cmd_in = 0;
	uint8_t val1_in = 0;
	uint8_t val2_in = 0;



	oscapi::msgq_t& inQ;
	oscapi::msgq_t& outQ;

	void spiRunner();
	void processNextSpiByte(const uint8_t bytIn);
};