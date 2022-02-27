#pragma once

#include "osc_workq.h"

#include <atomic>

/*!
 * \brief main OSC processor
 */
class OSCHandler
{
public:
	OSCHandler(oscapi::workq_t& workq);

	void debugDump();
private:
	oscapi::workq_t& m_workq;
};
