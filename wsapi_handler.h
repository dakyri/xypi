#pragma once

#include "wsapi_cmd.h"
#include "message.h"

#include <atomic>
#include <tuple>

/*!
 * \brief does the processing of web socket commands, json or otherwise, and returns an appropriate response. anything that can be handled without delay is handled directly
 *  and anything that will delay the io thread will be submitted to a worker
 */
class WSApiHandler
{
public:
	WSApiHandler(xymsg::q_t &_spiInQ, xymsg::q_t &_oscInQ, wsapi::cmdq_t& _cmdQ, wsapi::results_t& results);

	std::pair<bool, nlohmann::json> process(const nlohmann::json& request);

	nlohmann::json getCmd(nlohmann::json request);
	nlohmann::json listCmd(nlohmann::json request);

	void debugDump();

private:
	xymsg::q_t& spiInQ;
	xymsg::q_t& oscInQ;
	wsapi::cmdq_t& cmdq;
	wsapi::results_t& results;
	static std::atomic<wsapi::cmd_id> cmdid;
};
