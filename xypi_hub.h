#pragma once

#include "osc_workq.h"

#include <memory>

#include <boost/asio/io_service.hpp>

class OSCServer;
class OSCWorker;
class OSCHandler;

class XypiHub
{
public:
	XypiHub(uint16_t port, uint16_t threadCount = 1);
	~XypiHub();

	void run();
	void stop();

private:
	boost::asio::io_service oscService;

	std::shared_ptr<OSCHandler> oscHandler; //!<< we should be able to get away with sharing the one
	std::unique_ptr<OSCServer> oscServer;
	std::unique_ptr<OSCWorker> oscWorker;

	oscapi::workq_t oscOutQ;

	uint16_t threadCount;
};