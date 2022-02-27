#pragma once

#include "jsapi_workq.h"

#include <atomic>
#include <thread>

class JSApiWorker
{
public:
	JSApiWorker(jsapi::workq_t& workq, jsapi::results_t& results);
	~JSApiWorker();

	void run();
	void stop();

private:
	void runner();

private:
	std::atomic<bool> isRunning;
	std::thread myThread;

	jsapi::workq_t& workq;
	jsapi::results_t& results;
};