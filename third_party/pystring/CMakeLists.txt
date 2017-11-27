
cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library( pystring
  STATIC
  pystring.h
  pystring.cpp
  )

target_include_directories(
  pystring
  INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
  )

