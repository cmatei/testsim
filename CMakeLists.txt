cmake_minimum_required(VERSION 3.16)

project(testsim VERSION 0.1 LANGUAGES CXX)

FIND_PACKAGE(PkgConfig REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# RtAudio is required
pkg_check_modules(rtaudio REQUIRED IMPORTED_TARGET rtaudio)

# Main executable - Contest Simulator with WinKeyer3 protocol
add_executable(testsim
	testsim_daemon.cpp
	winkeyer.cpp keyer.cpp station.cpp
	audioprocess.cpp random.cpp qsb.cpp calllist.cpp
	dxstation.cpp dxoper.cpp qrnstation.cpp qrmstation.cpp
	mystation.cpp contest.cpp
)

target_link_libraries(testsim PUBLIC
	PkgConfig::rtaudio
)
