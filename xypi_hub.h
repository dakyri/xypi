#pragma once

#include "osc_workq.h"

#include <memory>

#include <boost/asio/io_service.hpp>

class OSCServer;
class OSCWorker;
class JSONHandler;
class WSServer;

namespace oscapi {
	class Processor;
}

class XypiHub
{
public:
	XypiHub(std::string dst_osc_adr, uint16_t dst_osc_prt, uint16_t rcv_osc_port, uint16_t ws_port, uint16_t threadCount = 1);
	~XypiHub();

	void run();
	void stop();

private:
	boost::asio::io_service ioService;

	std::shared_ptr<oscapi::Processor> oscParser; //!<< we should be able to get away with sharing the one
	std::unique_ptr<OSCServer> oscServer;
	std::unique_ptr<OSCWorker> oscWorker;
	std::shared_ptr<JSONHandler> jsonApi;
	std::unique_ptr<WSServer> wsServer;


	oscapi::workq_t oscOutQ;

	uint16_t threadCount;
};