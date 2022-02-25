#pragma once

#include "workqueue.h"

#include <atomic>

/*!
 * \brief main OSC processor
 */
class OSCHandler
{
public:
	OSCHandler(workq_t& workq, results_t& results);

	void debugDump();
private:
	workq_t& m_workq;
	results_t& m_results;
	static std::atomic<jobid_t> jobid;
};
