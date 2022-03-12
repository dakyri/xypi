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
#include <boost/regex.hpp>

int main(int argc, char** argv)
{
#undef HACK
#ifdef HACK
	boost::regex osc_re("/(?<MDI>midi(?<PRT>[0-9])?/((?<CHM>(non)|(nof)|(ctl)|(prg)|(chn)|(key)|(bnd))|(?<SYS>(sex)|(pos)|(stt)|(stp)|(tun)|(tcd)|(sel)|(clk))))");
	boost::cmatch results;
	if (boost::regex_match("/midi2/non", results, osc_re, boost::match_extra)) {
		if (results["MDI"].matched) {
			std::cout << "matches and recognises midi path" << "\n";
			std::cout << "port: " << results["PRT"] << "\n";
			std::cout << results[0].matched << results[1].matched << results[2].matched << results[3].matched << results[4].matched
				<< results[5].matched << results[11].matched << results[13].matched << results[20].matched << "\n";
			if (results["SYS"].matched) {
				std::cout << "system mode: " << results["SYS"] << "\n";
			}
			else if (results["CHM"].matched) {
				std::cout << "channel mode: " << results["CHM"] << "\n";
			}
		}
	}
	else {
		std::cout << "nope\n";
	}

#else
	namespace options = boost::program_options;
	options::options_description opts{"Chimera Socket Backend Dog Server"};
	// clang-format off
	opts.add_options()
		("help,h",															"show help screen")
		("port,p",		options::value<uint16_t>()->default_value(5505),	"set osc listening port")
		("threads,t",	options::value<uint16_t>()->default_value(1), 		"set io thread count (0 uses the default hardware concurrency)")
		("log-level,l",	options::value<uint16_t>()->default_value(4),		"set log level from 0 (off) to 4 (debug) and 5 (ridiculous)")
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
#endif
	return 0;
}