cmake_minimum_required(VERSION 3.16)

project(cpp11
	VERSION 1.0
	LANGUAGES CXX
	)

add_executable(cpp11 
	avr11.cc
	kb11.cc
	kt11.cc
	lp11.cc
	rk11.cc
	disasm.cc
	kl11.cc
	kw11.cc
	pc11.cc
	unibus.cc)

target_include_directories(cpp11 PUBLIC 
	"${PROJECT_SOURCE_DIR}")

target_compile_options(cpp11 PRIVATE -g1 -O2 -W -Wall -Werror -Wextra)

set_property(TARGET cpp11 PROPERTY CXX_STANDARD 17)
