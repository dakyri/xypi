#pragma once

#include "jsapi_cmd.h"

#include <atomic>
#include <thread>

class JSApiWorker
{
public:
	JSApiWorker(jsapi::cmdq_t& _cq, jsapi::results_t& results);
	~JSApiWorker();

	void run();
	void stop();

private:
	void runner();

private:
	std::atomic<bool> isRunning;
	std::thread myThread;

	jsapi::cmdq_t& cmdq;
	jsapi::results_t& results;
};