#include <iostream>
#include <array>
#include <chrono>
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;

#include <wiringPiSPI.h>

#include "pi_spi.h"
#include "xypiduino/include/xyspi.h"


constexpr int SPIchannel = 0;
constexpr int SPIclockSpeed = 1000000;

PiSpi::PiSpi(xymsg::q_t& _inQ, xymsg::q_t& _outQ)
	: inQ(_inQ), outQ(_outQ), isSpiOpen(false), isRunning(false)
{
	// For the SPI clock speed we use 1 MHz. With WiringPi and Raspberry Pi you can choose a clock speed between 500 kHz and 32 MHz.
	// different SPI modes: 0, 1, 2 and 3 –Here we are using mode 0, which is the default mode on Arduino.
	int fd = wiringPiSPISetupMode(SPIchannel, SPIclockSpeed, 0);
	if (fd >= 0) {
		isSpiOpen = true;
	} else {
		std::cerr << "Failed to init SPI communication.\n";	
	}
}

bool PiSpi::start() {
	if (!isSpiOpen) {
		return false;
	}
	if (!isRunning.exchange(true)) {
		spiThread = std::thread(spiRunner, this);
	}
	return isRunning;
}

/*!
 * stop the worker thread and wait until it completes. then clean up the dongle
 */
void PiSpi::stop()
{
	if (isRunning.exchange(false)) {
		inQ.disableWait();
		inQ.enable(false);
		if (spiThread.joinable()) spiThread.join();
	}
}


void PiSpi::spiRunner()
{
	std::array<uint8_t, xyspi::maxCmdLen> buf;

	static auto processNextSpiByte = [this] (const uint8_t bytIn) -> bool {
		switch (spi_in_state) {
			case command_byte:
				if (bytIn & xyspi::midi) {
					spi_in_state = midi_data;
					n_midi_cmd_incoming = bytIn & 0x7f;
				} else {
					switch (bytIn) {
						case xyspi::null:
							break;
						case xyspi::pong:
							return true;
						case xyspi::ping:
							break;
						case xyspi::send_tempo:
							tempo_requested = true;
							break;
						case xyspi::tempo:
							spi_in_state = tempo_data;
							set_tempo = false;
							break;
						case xyspi::diag_message:
							break;
					}			
				}
				break;
			case midi_data:
				cmd_in = bytIn;
				spi_in_state = midi_data_1;
				break;
			case midi_data_1:
				val1_in = bytIn;
				spi_in_state = midi_data_2;
				break;
			case midi_data_2:
				val2_in = bytIn;
				if (!deviceMidiOut.addToBuf(cmd_in, val1_in, val2_in)) {
					dropped_midi_in = true;
				}
				spi_in_state = (--n_midi_cmd_incoming > 0)? midi_data: command_byte;
				break;
			case tempo_data:
				spi_in_state = tempo_data_1;
				((uint8_t*)&incoming_tempo)[0] = bytIn;
				break;
			case tempo_data_1:
				spi_in_state = tempo_data_2;
				((uint8_t*)&incoming_tempo)[1] = bytIn;
				break;
			case tempo_data_2:
				spi_in_state = tempo_data_3;
				((uint8_t*)&incoming_tempo)[2] = bytIn;
				break;
			case tempo_data_3:
				spi_in_state = command_byte;
				((uint8_t*)&incoming_tempo)[3] = bytIn;
				set_tempo = true;
				break;
			default:
				spi_in_state = command_byte;
				break;
	
		}
		return false;
	};
	

	inQ.enable();
	inQ.enableWait();
	
	while (isRunning) {
		auto optMsg = inQ.front(timeout);
		int msgLen = 0;
		if (optMsg.second) {
			auto msg = optMsg.first;
			switch (msg->type) {
				case xymsg::typ::midi: {
					const auto &mmsg = std::static_pointer_cast<xymsg::MidiMsg>(msg);
					buf[0] = xyspi::cmd_t::midi | 1;
					buf[1] = mmsg->midi.cmd;
					buf[2] = mmsg->midi.val1;
					buf[3] = mmsg->midi.val2;
					msgLen = 4;
					break;
				}
				case xymsg::typ::midi_list: {
					const auto &mmsg = std::static_pointer_cast<xymsg::MidiListMsg>(msg);
					auto l = mmsg->midi.size();
					if (l > 127) {
						error("midi list too long. skipping.");
						break;
					}
					buf[0] = xyspi::cmd_t::midi | mmsg->midi.size();
					uint8_t *p = &buf[0];
					for (const auto &m: mmsg->midi) {
						buf[++msgLen] = m.cmd;
						buf[++msgLen] = m.val1;
						buf[++msgLen] = m.val2;
					}
					++msgLen;
					break;
				}
				case xymsg::typ::config_button: {
					const auto &mmsg = std::static_pointer_cast<xymsg::ConfigButtonMsg>(msg);
					buf[0] = xyspi::cmd_t::cfg_button;
					buf[1] = mmsg->which;
					buf[2] = sizeof(config::button);
					std::memcpy(&buf[3], &mmsg->cfg, sizeof(config::button));
					msgLen = sizeof(config::button) + 3;
					break;
				}
				case xymsg::typ::config_pedal: {
					const auto &mmsg = std::static_pointer_cast<xymsg::ConfigPedalMsg>(msg);
					buf[0] = xyspi::cmd_t::cfg_pedal;
					buf[1] = mmsg->which;
					buf[2] = sizeof(config::pedal);
					std::memcpy(&buf[3], &mmsg->cfg, sizeof(config::pedal));
					msgLen = sizeof(config::pedal) + 3;
					break;
				}
				case xymsg::typ::config_xlrm8r: {
					const auto &mmsg = std::static_pointer_cast<xymsg::ConfigXlm8rMsg>(msg);
					buf[0] = xyspi::cmd_t::cfg_xlrm8;
					buf[1] = mmsg->which;
					buf[2] = sizeof(config::xlrm8r);
					std::memcpy(&buf[3], &mmsg->cfg, sizeof(config::xlrm8r));
					msgLen = sizeof(config::xlrm8r) + 3;
					break;
				}
				case xymsg::typ::tempo: {
					const auto &mmsg = std::static_pointer_cast<xymsg::TempoMsg>(msg);
					buf[0] = xyspi::cmd_t::tempo;
					*(reinterpret_cast<float*>(&buf[1])) = mmsg->tempo;
					msgLen = 5;
					break;
				}
				case xymsg::typ::duino_cmd: {
					buf[0] = std::static_pointer_cast<xymsg::CmdMsg>(msg)->cmd;
					msgLen = 1;
					break;
				}
				case xymsg::typ::none:
				default:
					buf[0] = xyspi::cmd_t::null;
					msgLen = 1;
					break;
			}
		} else {
			buf[0] = xyspi::cmd_t::ping;
			msgLen = 1;
		}

		bool wasPonged = false;
		if (msgLen > 0) {
			wiringPiSPIDataRW(SPIchannel, buf, msgLen);
			for (auto i=0; i<msgLen; i++) {
				wasPonged = processNextSpiByte(buf[i]);
			}
		}

		if (isRunning) {
			if (!wasPonged || !inQ.empty()) {
				/* not really a sleep. but we'll yield otherwise, we wait for
				 incoming with wait on a condition variable with a timeout inside front()
				 at the start of the loop */
				std::this_thread::sleep_for(10us);
			}
		}
	}

}
