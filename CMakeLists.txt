cmake_minimum_required(VERSION 3.9)
set(CMAKE_CXX_STANDARD 17)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
if (WIN32)
	message(STATUS "Windoes!")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	list(APPEND CMAKE_MODULE_PATH "/usr/local")
endif()
project(xypi)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.74 COMPONENTS program_options coroutine thread)
find_package(Spdlog 1.15.2 REQUIRED)
find_package(nlohmann_json 3.6.0 REQUIRED)

find_path(HAS_WIRING_PI "wiringPiSPI.h")

include_directories(include)

if (HAS_WIRING_PI)
	set(EXTRA_SOURCES "pi_spi.cpp")
endif()

add_executable(${PROJECT_NAME}
	xypi.cpp
	xypi_hub.cpp
	osc_server.cpp
	osc_worker.cpp
	midi_worker.cpp
	osc_handler.cpp
	ws_server.cpp
	ws_session_handler.cpp
	wsapi_cmd.cpp
	wsapi_worker.cpp
	wsapi_handler.cpp
	jsonutil.cpp
	${EXTRA_SOURCES}
)

target_link_libraries(${PROJECT_NAME}
	${CMAKE_THREAD_LIBS_INIT}
	Boost::program_options
	Boost::coroutine
	Boost::thread
	nlohmann_json::nlohmann_json
)

if (WIN32)
	target_include_directories(${PROJECT_NAME}
		PUBLIC "C:/Program Files (x86)/RtMidi/include"
			   "C:/Program Files (x86)/spdlog/include"
			   "C:/Program Files (x86)/oscpp/include"
			   )
	add_definitions(-D_WIN32_WINNT=0x0A00 -DFMT_UNICODE=0) # 0x0A00 = win 10
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
endif()
target_include_directories(${PROJECT_NAME}
	PUBLIC "xypiduino/include")

option(SINGLE_THREADED_IO "Build single threaded server" OFF)
if (SINGLE_THREADED_IO)
	target_compile_definitions(${PROJECT_NAME} PUBLIC SINGLE_THREADED_IO)
endif()

option(XYPI_TESTS "Build server unit tests" OFF)
if (XYPI_TESTS)
	add_subdirectory("tests")
endif()

