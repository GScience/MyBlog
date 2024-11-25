cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ENV{VCPKG_ROOT})
	message(FATAL_ERROR "VCPKG is not enabled. Please set VCPKG_ROOT.")
endif()

project(dx12_learning)
include("cmake/functions.cmake")

add_note_code(Note1 "Note1.cpp")