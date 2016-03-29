cmake_minimum_required (VERSION 3.0)
project (Client)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(../Thirdparty/libuv/include/)
include_directories(inc)
include_directories(../Common/inc)

set(SOURCES
  src/chatsession.cpp
  src/main.cpp
  ../Common/src/msg.cpp
)
add_executable (Client ${SOURCES})
find_library(LIBUV_DEBUG NAMES libuv.a PATHS ../Thirdparty/libuv/Debug/)
find_library(LIBUV_RELEASE NAMES libuv.a PATHS ../Thirdparty/libuv/Release/)

TARGET_LINK_LIBRARIES(Client 
                      debug ${LIBUV_DEBUG}
                      optimized ${LIBUV_RELEASE}
                      pthread)

