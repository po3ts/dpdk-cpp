cmake_minimum_required(VERSION 3.20)
project(dpdk_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# ---------------------------------------------------------------------------
# Shared library
# ---------------------------------------------------------------------------
add_library(dpdk_cpp SHARED)

target_sources(dpdk_cpp
    PRIVATE
        src/device.cc
        src/device_manager.cc
)

target_include_directories(dpdk_cpp
    PUBLIC
        include
        ${DPDK_INCLUDE_DIRS}
)

target_link_libraries(dpdk_cpp PRIVATE ${DPDK_LIBRARIES})
target_compile_options(dpdk_cpp PRIVATE ${DPDK_CFLAGS_OTHER})

# ---------------------------------------------------------------------------
# Example: l2fwd
# ---------------------------------------------------------------------------
add_executable(l2fwd)

target_sources(l2fwd
    PRIVATE
        examples/l2fwd.cc
)

target_link_libraries(l2fwd PRIVATE dpdk_cpp ${DPDK_LIBRARIES})
target_compile_options(l2fwd PRIVATE ${DPDK_CFLAGS_OTHER})
