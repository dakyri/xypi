#pragma once

#include <vector>
#include <string>

#include "osc_cmd.h"

namespace OSCPP { namespace Server { class Packet; } };

namespace oscapi {
	/*!
	 * \brief main OSC processor, parses buffers of incoming messages
	 */
	class Processor
	{
	public:
		Processor(cmdq_t& cmdq);

		void parse(uint8_t *data, std::size_t size);
		bool pack(uint8_t *data, std::size_t &size, const std::shared_ptr<cmd_t> _cmd);
		bool pack(uint8_t *data, std::size_t &size, const std::string& path, const std::vector<int> & params = {});
		void debugDump();

	private:
		void handlePacket(OSCPP::Server::Packet &packet);

		cmdq_t& cmdq;
	};
};
