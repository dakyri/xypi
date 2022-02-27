#pragma once

#include "osc_workq.h"
#include "osc_server.h"

#include <atomic>
#include <thread>

class OSCWorker
{
public:
	OSCWorker(OSCServer &_oscurver, oscapi::workq_t& workq);
	~OSCWorker();

	void run();
	void stop();

private:
	void runner();

private:
	std::atomic<bool> isRunning;
	std::thread myThread;

	OSCServer &oscurver;
	oscapi::workq_t& workq;
};