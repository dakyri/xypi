#pragma once

#include "message.h"
#include "osc_server.h"

#include <atomic>
#include <thread>

class OSCWorker
{
public:
	OSCWorker(OSCServer &_oscurver, xymsg::q_t& msgq);
	~OSCWorker();

	void run();
	void stop();

private:
	void runner();

private:
	std::atomic<bool> isRunning;
	std::thread myThread;

	OSCServer &oscurver;
	xymsg::q_t& msgq;
};