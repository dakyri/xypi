#pragma once

#include "osc_cmd.h"
#include "osc_server.h"

#include <atomic>
#include <thread>

class OSCWorker
{
public:
	OSCWorker(OSCServer &_oscurver, oscapi::msgq_t& msgq);
	~OSCWorker();

	void run();
	void stop();

private:
	void runner();

private:
	std::atomic<bool> isRunning;
	std::thread myThread;

	OSCServer &oscurver;
	oscapi::msgq_t& msgq;
};