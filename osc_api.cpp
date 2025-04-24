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
	regex osc_re("/(?<MDI>midi(?<PRT>[0-9])?)");

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
				if (mcp->midi.port > 0) {
					base += ('0' + mcp->midi.port);
				}
				packet.openMessage(base.c_str(), 1).midi({mcp->midi.cmd, mcp->midi.val1, mcp->midi.val2, mcp->midi.port}).closeMessage();
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
					try {
						const auto m = args.midi();
						xymidi::msg mbuf(m.status, m.data1, m.data2, m.port);
					}
					catch (const OSCPP::Error &e) {
						debug("Oscpp error processing {}: {}", results["CMD"].str(), e.what());
					}
				}
			}
		}
	}

};
