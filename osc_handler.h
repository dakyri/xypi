#pragma once

#include <vector>
#include <string>

#include "message.h"

namespace OSCPP { namespace Server { class Packet; } };

namespace oscapi {
	/*!
	 * \brief main OSC processor, parses buffers of incoming OSC message to the internal msg structureand handles formatting of out message
	 * queue structure into OSC, ready for broadcast on the OSC socket
	 * the incoming message are sent to queues them ready to be directed to a local midi connection or SPI connect, 
	 */
	class Processor
	{
	public:
		Processor(xymsg::q_t& _outq);

		void parse(uint8_t *data, std::size_t size);
		bool pack(uint8_t *data, std::size_t &size, const std::shared_ptr<xymsg::msg_t> _msg);
		bool pack(uint8_t *data, std::size_t &size, const std::string& path, const std::vector<int> & params = {});
		void debugDump();

	private:
		void handlePacket(OSCPP::Server::Packet &packet);

		xymsg::q_t& outq;
	};
};
