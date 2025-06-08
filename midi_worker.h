#pragma once

#include "message.h"

#include <atomic>
#include <thread>
#include <memory>

class RtMidiIn;
class RtMidiOut;

class MidiWorker
{
public:
	MidiWorker(xymsg::q_t &_spiInQ, xymsg::q_t &_oscInQ, xymsg::q_t& _midiInQ);
	~MidiWorker();

	void run();
	void stop();

	void scanPorts();
	void openPorts();

	bool hasVirtualPorts();
private:
	void runner();
	void sendMIDI(xymsg::midi_t m);
private:
	std::atomic<bool> isRunning;
	std::thread myThread;

	std::unique_ptr<RtMidiIn> midiIn;
	std::unique_ptr<RtMidiOut> midiOut;
	
	std::vector<std::string> midiInPorts;
	std::vector<std::string> midiOutPorts;
	
	xymsg::q_t& spiInQ;
	xymsg::q_t& oscInQ;
	xymsg::q_t& midiOutQ;
};