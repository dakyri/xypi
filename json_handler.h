#pragma once

#include "jsapi_cmd.h"
#include "message.h"

#include <atomic>
#include <tuple>

/*!
 * \brief does the processing of json commands and returns json. anything that can be handled without delay is handled directly
 *  and anything that will delay the io thread will be submitted to a worker
 */
class JSONHandler
{
public:
	JSONHandler(xymsg::q_t &_spiInQ, xymsg::q_t &_oscInQ, jsapi::cmdq_t& _cmdQ, jsapi::results_t& results);

	std::pair<bool, nlohmann::json> process(const nlohmann::json& request);

	nlohmann::json getCmd(nlohmann::json request);
	nlohmann::json listCmd(nlohmann::json request);

	void debugDump();

private:
	xymsg::q_t& spiInQ;
	xymsg::q_t& oscInQ;
	jsapi::cmdq_t& cmdq;
	jsapi::results_t& results;
	static std::atomic<jsapi::cmd_id> cmdid;
};
