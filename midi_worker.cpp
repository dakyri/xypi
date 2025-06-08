#include "midi_worker.h"

#include "rtmidi/RtMidi.h"
#include <spdlog/spdlog.h>

#include <chrono>

using namespace std::chrono_literals;

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;


MidiWorker::MidiWorker(xymsg::q_t &_spiInQ, xymsg::q_t &_oscInQ, xymsg::q_t &_midiInQ)
	: spiInQ(_spiInQ), oscInQ(_oscInQ), midiOutQ(_midiInQ)
{
 	try {
 		midiIn = std::make_unique<RtMidiIn>(RtMidi::Api::UNSPECIFIED, "xypi midi in");
	} catch ( RtMidiError &e ) {
		error(e.getMessage());
	}
 	try {
		midiOut = std::make_unique<RtMidiOut>(RtMidi::Api::UNSPECIFIED, "xypi midi out");
	} catch ( RtMidiError &e ) {
		error(e.getMessage());
	}
	scanPorts();
	openPorts();
}

MidiWorker::~MidiWorker()
{
	stop();
}

/*!
 * let's see what we can talk to
 */
void MidiWorker::scanPorts()
{
	midiInPorts.clear();
	midiOutPorts.clear();
	if (midiIn) {
		uint32_t nInPorts = midiIn->getPortCount();
		std::string portName;
		for (uint32_t i=0; i<nInPorts; i++) {
			try {
				portName = midiIn->getPortName(i);
				midiInPorts.push_back(portName);
			} catch (const RtMidiError &error ) {
				error.printMessage();
				break;
			}
			info(">> Input Port #{}: {}", i+1 , portName);
		}
 	}
	if (midiOut) {
		uint32_t nOutPorts = midiIn->getPortCount();
		std::string portName;
		for (uint32_t i=0; i<nOutPorts; i++) {
			try {
				portName = midiIn->getPortName(i);
				midiOutPorts.push_back(portName);
			} catch (const RtMidiError &error ) {
				error.printMessage();
				break;
			}
			info(">> Output Port #{}: {}", i+1 , portName);
		}
	}
}

void MidiWorker::openPorts()
{
	auto hvp = hasVirtualPorts();
	if (midiInPorts.size() > 0) {
		midiIn->setCallback([](double deltaTime, std::vector<unsigned char> *imsg, void *data) {
			const auto *worker = static_cast<MidiWorker*>(data);
			const auto msglen = imsg->size();
			if (msglen <= 3) {
				const auto omsgp = std::make_shared<xymsg::MidiMsg>();
				auto &omdi = omsgp->midi;
				omdi.cmd = imsg->at(0);
				if (msglen > 1) {
					omdi.val1 = imsg->at(1);
					if (msglen > 2) {
						omdi.val2 = imsg->at(2);
					}
				}
				worker->oscInQ.push(omsgp);
				worker->spiInQ.push(omsgp);
			} else { // for the moment assume this is just not going to happen except for sysx
				warn("unexpected midi length for {}: {}", imsg->at(0), imsg->size());
			}
		}, this);
		midiIn->ignoreTypes(false, false, false);
		if (hvp) {
			midiIn->openPort(0, "Xypi midi in");
		} else {
			midiIn->openVirtualPort("Xypi midi in");
		}
	}
	if (midiOutPorts.size() > 0) {
		if (hvp) {
			midiOut->openPort(0, "Xypi midi out");
		} else {
			midiOut->openVirtualPort("Xypi midi out");
		}
	}
}

bool MidiWorker::hasVirtualPorts()
{
	std::vector<RtMidi::Api> apis;
	midiIn->getCompiledApi(apis);
	return std::find(apis.begin(), apis.end(), RtMidi::WINDOWS_MM) == apis.end();
}

/*!
 * launch the OSCWorker and return immediately. we check that the dongle is open here, and if we have enough permission,
 * we list whatever files we find.
 */
void MidiWorker::run()
{
	if (!isRunning.exchange(true)) {
		debug("OSCWorker::run() launching main thread");
		myThread = std::thread([this]() { runner(); });
	}
}


/*!
 * stop the worker thread and wait until it completes. then clean up the dongle
 */
void MidiWorker::stop()
{
	if (isRunning.exchange(false)) {
		midiOutQ.disableWait();
		midiOutQ.enable(false);
		if (myThread.joinable()) myThread.join();
	}
}

/*!
 *
 */
inline void MidiWorker::sendMIDI(xymsg::midi_t m)
{
	const auto chan_cmd = m.cmd & 0xf0;
	std::vector<unsigned char> msg;
	if (chan_cmd == 0xf0) {
		switch (m.cmd) {
		case (uint8_t)xymidi::cmd::songPos:
			msg.assign({m.cmd, m.val1, m.val2});
			break;
		case (uint8_t)xymidi::cmd::timeCode:
		case (uint8_t)xymidi::cmd::songSel:
			msg.assign({m.cmd, m.val1});
			break;
		case (uint8_t)xymidi::cmd::clock:
		case (uint8_t)xymidi::cmd::start:
		case (uint8_t)xymidi::cmd::cont:
		case (uint8_t)xymidi::cmd::stop:
		case (uint8_t)xymidi::cmd::sensing:
		case (uint8_t)xymidi::cmd::tuneReq:
			msg.assign({m.cmd});
			break;

		case (uint8_t)xymidi::cmd::sysxStart: /*! shouldn't see anyway. handle separately */
		case (uint8_t)xymidi::cmd::sysxEnd:
		default:
			return;
		}
	} else {
		switch (chan_cmd) {
		case (uint8_t)xymidi::cmd::noteOn:
		case (uint8_t)xymidi::cmd::noteOff:
		case (uint8_t)xymidi::cmd::keyPress:
		case (uint8_t)xymidi::cmd::ctrl:
		case (uint8_t)xymidi::cmd::bend:
			msg.assign({m.cmd, m.val1, m.val2});
			break;
		case (uint8_t)xymidi::cmd::prog:
		case (uint8_t)xymidi::cmd::chanPress:
			msg.assign({m.cmd, m.val1});
			break;
		default:
			return;
		}
	}
	midiOut->sendMessage(&msg);
}

/*!
 * main body of the work queue processor
 */
void MidiWorker::runner()
{
	midiOutQ.enable();
	midiOutQ.enableWait();
	while (isRunning) {
		auto optMsg = midiOutQ.front();
		if (optMsg.second) {
			// specifically make a new reference to the shared_ptr to work with, so we can leave the work at the top of q
			// workRef should still be valid even if it is no longer front
			const auto &msg = optMsg.first;
			try {
				if (msg->type == xymsg::typ::midi) {
					const auto &mmsg = std::reinterpret_pointer_cast<xymsg::MidiMsg>(msg)->midi;
					sendMIDI(mmsg);
				} else if (msg->type == xymsg::typ::midi) {
					const auto &mlmsg = std::reinterpret_pointer_cast<xymsg::MidiListMsg>(msg)->midi;
					for (const auto &m: mlmsg) {
						sendMIDI(m);
					}
				}
			} catch (const std::exception& e) {
				error("MidiWorker() gets exception: {}", e.what());
			}
			midiOutQ.remove(optMsg.first); // now it's safe to remove!
		} else {
			if (isRunning && !midiOutQ.waitEnabled()) std::this_thread::sleep_for(10us);
		}
	}
}
