#pragma once

#include "dongle.h"
#include "hubctl.h"
#include "workqueue.h"

#include <atomic>
#include <thread>

class Worker
{
public:
	Worker(Dongle::permission permission, const std::string& passwd, workq_t& workq, results_t& results);
	~Worker();

	void run();
	void stop();

protected:
	bool openDongle();
	void closeDongle();

private:
	void runner();

protected:
	Dongle::permission m_donglePermission;
	std::string m_donglePasswd;
	Dongle m_dongle;
	HubCtl m_hub;

private:
	std::atomic<bool> m_isRunning;
	std::thread m_thread;

	workq_t& m_workq;
	results_t& m_results;
};