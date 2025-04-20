#include "osc_api.h"
#include "osc_cmd.h"

#include <functional>
#include <boost/regex.hpp>
#include <spdlog/spdlog.h>
#include <oscpp/server.hpp>
#include <oscpp/client.hpp>
#include <oscpp/print.hpp>

#include <sstream>
#include <iomanip>

std::string hexStr(uint8_t *data, int len)
{
	std::stringstream ss;
	ss << std::hex;

	for (int i(0); i < len; ++i)
		ss << std::setw(2) << std::setfill('0') << (int)data[i];

	return ss.str();
}

using spdlog::info;
using spdlog::error;
using spdlog::debug;
using spdlog::warn;
using regex = boost::regex;
using midi_cmd = xymidi::cmd;

namespace oscapi {
	regex osc_re("/(?<MDI>midi(?<PRT>[0-9])?/(?<CMD>"
		"(non)|(nof)|(key)|(ctl)|(prg)|(chn)|(bnd)" "|"			// numbered submatches [4..10]
		"(sex)|(tcd)|(pos)|(sel)|(tun)|(clk)|(stt)|(cnt)|(stp)" "))"	// numbered submatches [11..19]
	);

	/*!
	 * \class oscapi::Parser
	 * main unit handling translation to and from packed OSC data and internal structures for MIDI and other items of interest
	 */
	Processor::Processor(oscapi::cmdq_t& _cmdq) : cmdq(cmdq) {}

	/*!
	 * main wrapper decoding an OSC encoded buffer
	 */
	void Processor::parse(uint8_t* data, std::size_t size)
	{
		debug("got bytes {}", hexStr(data, size));
		handlePacket(OSCPP::Server::Packet(data, size));
	}

	/*!
	 * pack one of our recognized midi/whatever messages as OSC
	 */
	bool Processor::pack(uint8_t* buffer, std::size_t& size, const std::shared_ptr<cmd_t> msg)
	{
		try {
			if (msg->type == cmd_type::midi) {
				auto mcp = std::static_pointer_cast<MidiMsg>(msg);
				if (!mcp) {
					return false;
				}
				OSCPP::Client::Packet packet(buffer, size);
				/*
				packet       // Open a bundle with a timetag
					.openBundle(1234ULL)
				*/
				std::string base("/midi");
				if (mcp->port > 0) {
					base += ('0' + mcp->port);
				}
				auto val = mcp->midi.val;
				if (!xymidi::isSysCmd(mcp->midi.cmd)) {
					auto chan = xymidi::getCCChan(mcp->midi.cmd);
					switch (xymidi::getCCCmd(mcp->midi.cmd)) {
					case (int)midi_cmd::noteOn: {
						packet.openMessage((base + "/non").c_str(), 3).int32(chan).int32(val.note.pitch).int32(val.note.vel).closeMessage();
						break;
					}
					case (int)midi_cmd::noteOff: {
						packet.openMessage((base + "/nof").c_str(), 3).int32(chan).int32(val.note.pitch).int32(val.note.vel).closeMessage();
						break;
					}
					case (int)midi_cmd::keyPress: {
						packet.openMessage((base + "/key").c_str(), 3).int32(chan).int32(val.note.pitch).int32(val.note.vel).closeMessage();
						break;
					}
					case (int)midi_cmd::ctrl: {
						packet.openMessage((base + "/ctl").c_str(), 3).int32(chan).int32(val.ctrl.tgt).int32(val.ctrl.amt).closeMessage();
						break;
					}
					case (int)midi_cmd::prog: {
						packet.openMessage((base + "/prg").c_str(), 2).int32(chan).int32(val.prog).closeMessage();
						break;
					}
					case (int)midi_cmd::chanPress: {
						packet.openMessage((base + "/chn").c_str(), 2).int32(chan).int32(val.press).closeMessage();
						break;
					}
					case (int)midi_cmd::bend: {
						packet.openMessage((base + "/bnd").c_str(), 2).int32(chan).int32(val.bend).closeMessage();
						break;
					}
					}
				} else {
					switch (mcp->midi.cmd) {
					case (int)midi_cmd::timeCode: {
						packet.openMessage((base + "/tcd").c_str(), 2).int32(val.time_code.type).int32(val.time_code.val).closeMessage();
						break;
					}
					case (int)midi_cmd::songPos: {
						packet.openMessage((base + "/pos").c_str(), 1).int32(val.song_pos).closeMessage();
						break;
					}
					case (int)midi_cmd::songSel: {
						packet.openMessage((base + "/sel").c_str(), 1).int32(val.song_sel).closeMessage();
						break;
					}
					case (int)midi_cmd::tuneReq: {
						packet.openMessage((base + "/tun").c_str(), 0).closeMessage();
						break;
					}
					case (int)midi_cmd::clock: {
						packet.openMessage((base + "/clk").c_str(), 0).closeMessage();
						break;
					}
					case (int)midi_cmd::start: {
						packet.openMessage((base + "/stt").c_str(), 0).closeMessage();
						break;
					}
					case (int)midi_cmd::cont: {
						packet.openMessage((base + "/cnt").c_str(), 0).closeMessage();
						break;
					}
					case (int)midi_cmd::stop: {
						packet.openMessage((base + "/stp").c_str(), 0).closeMessage();
						break;
					}

					}
				}
				/*
				packet.closeBundle();
				*/
				size = packet.size();
			}
		} catch (const std::exception& e) {
			debug("Processor::pack throws {}", e.what());
			return false;
		}
		return true;
	}

	/*!
	 * pack an arbitrary message with a path and possible int params as OSC
	 */
	bool Processor::pack(uint8_t * buffer, std::size_t & size, const std::string & path, const std::vector<int>& params)
	{
		try {
			OSCPP::Client::Packet packet(buffer, size);
			packet.openMessage(path.c_str(), params.size());
			for (const auto pi : params) {
				packet.int32(pi);
			}
			packet.closeMessage();
			size = packet.size();
		}
		catch (const std::exception& e) {
			debug("Processor::pack throws {}", e.what());
			return false;
		}
		return true;
	}

	void Processor::debugDump()
	{
		debug("Processor::debugDump()");
		//	m_workq.foreach([](const std::shared_ptr<oscapi::work_t>& v) { debug("> work id {}", v->id); });
	}

	void Processor::handlePacket(OSCPP::Server::Packet & packet)
	{
		if (packet.isBundle()) {
			// Convert to a PacketStream, iterate over all packets, and call handlePacket recursively.
			// TODO: check recursion level. shouldn't go deep
			OSCPP::Server::Bundle bundle(packet);
			OSCPP::Server::PacketStream packets(bundle.packets());
			debug("bundle {}", bundle.time());
			while (!packets.atEnd()) {
				handlePacket(packets.next());
			}
		} else {
			// Convert to message, stream arguments
			OSCPP::Server::Message msg(packet);
			OSCPP::Server::ArgStream args(msg.args());
			boost::cmatch results;
			if (boost::regex_match(msg.address(), results, osc_re, boost::match_extra)) {
				if (results["MDI"].matched) {
					debug("matches and recognises midi path '{}', cmd '{}'", results["MDI"].str(), results["CMD"].str());
					uint8_t port = results["PRT"].matched ? results["PRT"].str()[0] - '0' : 0;
					uint8_t cmd_ind = 0;
					for (int i = 4; i <= 18; ++i) {
						if (results[i].matched) {
							cmd_ind = i;
							break;
						}
					}
					xymidi::msg mbuf;
					try {
						switch (cmd_ind) {
						case 4: {
							auto chan = args.int32(), pitch = args.int32(), vel = args.int32();
							mbuf.noteon(chan, pitch, vel);
							debug("note on {} {} {}", mbuf.channel(), mbuf.val.note.pitch, mbuf.val.note.vel);
							break;
						}
						case 5: {
							auto chan = args.int32(), pitch = args.int32(), vel = args.int32();
							mbuf.noteoff(chan, pitch, vel);
							debug("note off {} {} {}", mbuf.channel(), mbuf.val.note.pitch, mbuf.val.note.vel);
							break;
						}
						case 6: {
							auto chan = args.int32(), pitch = args.int32(), vel = args.int32();
							mbuf.keypress(chan, pitch, vel);
							debug("keypress {} {} {}", mbuf.channel(), mbuf.val.note.pitch, mbuf.val.note.vel);
							break;
						}
						case 7: {
							auto chan = args.int32(), tgt = args.int32(), amt = args.int32();
							mbuf.control(chan, tgt, amt);
							debug("control {} {} {}", mbuf.channel(), mbuf.val.ctrl.tgt, mbuf.val.ctrl.amt);
							break;
						}
						case 8: {
							auto chan = args.int32(), prog = args.int32();
							mbuf.prog(chan, prog);
							debug("prog {} {}", mbuf.channel(), mbuf.val.prog);
							break;
						}
						case 9: {
							auto chan = args.int32(), press = args.int32();
							mbuf.chanpress(chan, press);
							debug("chanpress {} {}", mbuf.channel(), mbuf.val.press);
							break;
						}
						case 10: {
							auto chan = args.int32(), bend = args.int32();
							mbuf.bend(chan, bend);
							debug("bend {} {}", mbuf.channel(), mbuf.val.bend);
							break;
						}
						case 11:
//							mbuf.sysx();
							throw new OSCPP::Error("unimplemented sysx");
							break;
						case 12: {
							auto typ = args.int32(), val = args.int32();
							mbuf.timecode(typ, val);
							debug("timecode {} {}", mbuf.val.time_code.type, mbuf.val.time_code.val);
							break;
						}
						case 13: 
							mbuf.songpos(args.int32());
							debug("songpos {}", mbuf.val.song_pos);
							break;
						case 14: {
							mbuf.songsel(args.int32());
							debug("songsel {}", mbuf.val.song_sel);
							break;
						}
						case 15:
							mbuf.tune();
							debug("tune");
							break;
						case 16:
							mbuf.clock();
							debug("clock");
							break;
						case 17:
							mbuf.start();
							debug("start");
							break;
						case 18:
							mbuf.cont();
							debug("cont");
							break;
						case 19:
							mbuf.stop();
							debug("stop");
							break;
						}

					}
					catch (const OSCPP::Error &e) {
						debug("Oscpp error processing {}: {}", results["CMD"].str(), e.what());
					}
				}
			}
		}
	}

};
