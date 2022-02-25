#include "xypi_hub.h"

#include <boost/program_options.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <stdlib.h>
//#include <sys/wait.h>

using spdlog::info;
using spdlog::debug;

/*!
 * sets the logging level to a value from 0 (logging completely off) to 4 (full debug nonsense)
 */
void setLogLevel(int l)
{
	spdlog::level::level_enum level = spdlog::level::info;
	switch (l) {
	case 0: level = spdlog::level::off; break;
	case 1: level = spdlog::level::err; break;
	case 2: level = spdlog::level::warn; break;
	case 3: level = spdlog::level::info; break;
	case 4: level = spdlog::level::debug; break;
	case 5: level = spdlog::level::trace; break;
	}
	spdlog::set_level(level);
}

int main(int argc, char** argv)
{
	namespace options = boost::program_options;
	options::options_description opts{"Chimera Socket Backend Dog Server"};
	// clang-format off
	opts.add_options()
		("help,h",														"show help screen")
		("port,p",		options::value<uint16_t>()->default_value(80),	"set listening port")
		("threads,t",	options::value<uint16_t>()->default_value(0), 	"set io thread count (0 uses the default hardware concurrency)")
		("log-level,l",	options::value<uint16_t>()->default_value(3),	"set log level from 0 (off) to 4 (debug) and 5 (ridiculous)")
		;
	// clang-format on
	options::variables_map vars;
	options::store(options::parse_command_line(argc, argv, opts), vars);
	options::notify(vars);

	if (vars.count("help")) {
		std::cout << opts << std::endl;
		return 0;
	}

	auto serverPort = vars["port"].as<uint16_t>();
	auto threadCount = vars["threads"].as<uint16_t>();
	if (threadCount == 0) threadCount = std::thread::hardware_concurrency();
	auto logLevel = vars["log-level"].as<uint16_t>();
	setLogLevel(logLevel);

	info("starting xypi hub {}", std::string("a string"));

	XypiHub xypi(serverPort, threadCount);
	xypi.run();

	return 0;
}