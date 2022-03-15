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
	// 
	boost::cmatch results;
	boost::regex upgrade_re("(?<REQ>GET .+ HTTP.+\r\n)"
		"((Sec-WebSocket-Key: (?<KEY>.*)\r\n)"
		"|(?<UPG>Upgrade: websocket\r\n)"
		"|(.+:.+\r\n))+"
		"(?<HEND>\r\n)");
	/*
	boost::regex upgrade_re(
		"(?<REQ>GET .+ HTTP.+\r\n)"
		"((Sec-WebSocket-Key: (?<KEY>.*)\r\n)"
		"|"
		"(?<UPG>Upgrade: websocket\r\n)"
		"|"
		"(.+:.+\r\n))+"
		"(?<HEND>\r\n)");*/
	std::string req("jhkjh\r\n"
		"GET / HTTP/1.1\r\n"
		"Host: 127.0.0.1:8080\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate\r\n"
		"Sec-WebSocket-Key: vXHVX3RfeMpEJfapdcS+ng==\r\n"
		"Connection: keep-alive, Upgrade\r\n"
		"Upgrade: websocket\r\n"
		"\r\n"
		"as;jk;sldfhjfsklad;"
	);
	if (boost::regex_search(req.c_str(), results, upgrade_re, boost::match_extra|boost::match_not_dot_newline)) {
		if (results["HEND"].matched && results["UPG"].matched) {
			std::cout << "found an upgrade request " << results["REQ"].str() << ", UPGL: " << results["UPG"].str() << ", KEY: " << results["KEY"].str() << "\n";
		}
		else {
			std::cout << "missing upgrade key and/or header end" << "\n";
		}
	}
	else {
		std::cout << "not matching basic http request" << "\n";
	}
	/*

	boost::regex osc_re("/(?<MDI>midi(?<PRT>[0-9])?/((?<CHM>(non)|(nof)|(ctl)|(prg)|(chn)|(key)|(bnd))|(?<SYS>(sex)|(pos)|(stt)|(stp)|(tun)|(tcd)|(sel)|(clk))))");
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
	}*/

#else
	namespace options = boost::program_options;
	options::options_description opts{"Chimera Socket Backend Dog Server"};
	// clang-format off
	opts.add_options()
		("help,h",																		"show help screen")
		("osc_dst_addr,a",	options::value<std::string>()->default_value("127.0.0.1"),	"set osc target address")
		("osc_dst_port,p",	options::value<uint16_t>()->default_value(57120),			"set osc target port")
		("osc_rcv_port,q",	options::value<uint16_t>()->default_value(5505),			"set osc listening port")
		("ws_port,r",		options::value<uint16_t>()->default_value(8080),			"set ws listening port")
		("threads,t",		options::value<uint16_t>()->default_value(1), 				"set io thread count (0 uses the default hardware concurrency)")
		("log-level,l",		options::value<uint16_t>()->default_value(4),				"set log level from 0 (off) to 4 (debug) and 5 (ridiculous)")
		;
	// clang-format on
	options::variables_map vars;
	options::store(options::parse_command_line(argc, argv, opts), vars);
	options::notify(vars);

	if (vars.count("help")) {
		std::cout << opts << std::endl;
		return 0;
	}

	auto oscDstAddr = vars["osc_dst_addr"].as<std::string>();
	auto oscDstPort = vars["osc_dst_port"].as<uint16_t>();
	auto oscRcvPort = vars["osc_rcv_port"].as<uint16_t>();
	auto wsPort = vars["ws_port"].as<uint16_t>();
	auto threadCount = vars["threads"].as<uint16_t>();
	if (threadCount == 0) threadCount = std::thread::hardware_concurrency();
	auto logLevel = vars["log-level"].as<uint16_t>();
	setLogLevel(logLevel);

	info("starting xypi hub {}", std::string("a string"));

	XypiHub xypi(oscDstAddr, oscDstPort, oscRcvPort, wsPort, threadCount);
	xypi.run();
#endif
	return 0;
}