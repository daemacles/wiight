cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library( backward-cpp 
  STATIC
  backward.cpp
  )

target_link_libraries( backward-cpp
  bfd
  )


target_include_directories( backward-cpp
    INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
    "$<INSTALL_INTERFACE:include>"
)

