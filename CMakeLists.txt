cmake_minimum_required (VERSION 3.5)

set( PROJECT_NAME wiight )
project( ${PROJECT_NAME} )

find_package( Eigen3 3.3 REQUIRED NO_MODULE )

## Set optional features.  This will show up as a preprocessor variable
option( USE_GLIBCXX_DEBUG 
  "Use the _GLIBCXX_DEBUG macro for nicer debugging and error checking of the c++ standard library.  Warning, it breaks ABI compatibility so don't pass container instantiations between translation units that don't have the same debug mode."
  ON )
## USE_MY_LIBRARY in source.
#option (USE_MY_LIBRARY
#        "Use the provided library" ON)

## Support for Clang's CompilationDatabase system
set (CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_subdirectory( third_party/spdlog )
add_subdirectory( third_party/fmt )
add_subdirectory( third_party/pystring )
add_subdirectory( third_party/backward-cpp )
add_subdirectory( third_party/libevhtp.git )

## Compile and create a library.  STATIC is default unless BUILD_SHARED_LIBS
## is on.
#add_library (LibraryName src/sources.cc)

#if (USE_MY_LIBRARY)
  ## Search for include files here as well
  #include_directories ("{PROJECT_SOURCE_DIR}/some_sub_path")

  ## Run Cmake also in this dir
  #add_subdirectory (some_sub_path)

  #set (EXTRA_LIBS ${EXTRA_LIBS} LibraryName)
#endif (USE_MY_LIBRARY)

include_directories( include )
include_directories( third_party/sqlite_modern_cpp/hdr )
include_directories( third_party/argh.git )
include_directories( third_party/json )

set (LIBS 
  backward-cpp
  fmt
  pthread
  pystring
  spdlog
  sqlite3
  xwiimote
  Eigen3::Eigen
  evhtp
  stdc++fs
  )

add_executable( ${PROJECT_NAME} 
  src/logging.cc
  src/main.cc
  src/stacktrace.cc
  )
target_link_libraries( ${PROJECT_NAME} 
  ${EXTRA_LIBS} 
  ${LIBS}
  )

# Compile flags
SET (CMAKE_C_FLAGS                "-Wall -std=c11 -Wextra -Werror")
SET (CMAKE_C_FLAGS_DEBUG          "${CMAKE_CFLAGS} -ggdb3")
SET (CMAKE_C_FLAGS_MINSIZEREL     "${CMAKE_CFLAGS} -Os -DNDEBUG")
SET (CMAKE_C_FLAGS_RELEASE        "${CMAKE_CFLAGS} -O3 -DNDEBUG")
SET (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_CFLAGS} -O2 -g")

SET (CMAKE_CXX_FLAGS                "-Wall -std=c++14 -Wextra -Werror")
SET (CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXXFLAGS} -ggdb3")
SET (CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXXFLAGS} -Os -DNDEBUG")
SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXXFLAGS} -O2 -g")
SET (CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXXFLAGS} -O3 -DNDEBUG")

if (USE_GLIBCXX_DEBUG)
  # Prefer using G++ for debug mode because the GLIBCXX_DEBUG flag for pretty
  # printing collections in GDB doesn't seem to work with clang.
  string(TOUPPER ${CMAKE_BUILD_TYPE} DEBUG_MODE)
  if (DEBUG_MODE MATCHES "DEBUG")
    set (CMAKE_CXX_COMPILER "g++")
    set (CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXXFLAGS} -g -D_GLIBCXX_DEBUG")
  elseif (DEBUG_MODE MATCHES "RELWITHDEBINFO")
    set (CMAKE_CXX_COMPILER "g++")
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXXFLAGS} -O2 -g -D_GLIBCXX_DEBUG")
  endif (DEBUG_MODE MATCHES "DEBUG")
endif (USE_GLIBCXX_DEBUG)


