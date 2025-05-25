cmake_minimum_required(VERSION 3.9)
set(CMAKE_CXX_STANDARD 17)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "/usr/local")
endif()
project(xypi)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.74 COMPONENTS program_options coroutine thread)
find_package(spdlog 1.15.2 REQUIRED)
find_package(nlohmann_json 3.6.0 REQUIRED)

include_directories(include)

set(EXTRA_SOURCES "")

if (WIN32)
	list(APPEND EXTRA_SOURCES win_midi.cpp)
endif()

add_executable(${PROJECT_NAME}
	xypi.cpp
	xypi_hub.cpp
	osc_server.cpp
	osc_worker.cpp
	osc_handler.cpp
	ws_server.cpp
	ws_session_handler.cpp
	jsapi_cmd.cpp
	jsapi_worker.cpp
	json_handler.cpp
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

option(SINGLE_THREADED_IO "Build single threaded server" OFF)
if (SINGLE_THREADED_IO)
	target_compile_definitions(${PROJECT_NAME} PUBLIC SINGLE_THREADED_IO)
endif()

option(XYPI_TESTS "Build server unit tests" OFF)
if (XYPI_TESTS)
	add_subdirectory("tests")
endif()

