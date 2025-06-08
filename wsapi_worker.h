#pragma once

#include "wsapi_cmd.h"

#include <atomic>
#include <thread>

class WSApiWorker
{
public:
	WSApiWorker(wsapi::cmdq_t& _cq, wsapi::results_t& results);
	~WSApiWorker();

	void run();
	void stop();

private:
	void runner();

private:
	std::atomic<bool> isRunning;
	std::thread myThread;

	wsapi::cmdq_t& cmdq;
	wsapi::results_t& results;
};