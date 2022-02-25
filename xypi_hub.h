#pragma once

#include "workqueue.h"

#include <memory>

#include <boost/asio/io_service.hpp>

class OSCServer;
class Worker;
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
	std::unique_ptr<Worker> oscWorker;

	workq_t m_workQ;
	results_t m_results;

	uint16_t threadCount;
};