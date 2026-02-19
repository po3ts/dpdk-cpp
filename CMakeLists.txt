cmake_minimum_required(VERSION 3.20)
project(dpdkpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# ---------------------------------------------------------------------------
# Shared library
# ---------------------------------------------------------------------------
add_library(dpdkpp SHARED)

target_sources(dpdkpp
    PRIVATE
        src/device.cc
        src/device_manager.cc
)

target_include_directories(dpdkpp
    PUBLIC
        include
        ${DPDK_INCLUDE_DIRS}
)

target_link_libraries(dpdkpp PRIVATE ${DPDK_LIBRARIES})
target_compile_options(dpdkpp PRIVATE ${DPDK_CFLAGS_OTHER})

# ---------------------------------------------------------------------------
# Example: l2fwd
# ---------------------------------------------------------------------------
add_executable(l2fwd)

target_sources(l2fwd
    PRIVATE
        examples/l2fwd.cc
)

target_link_libraries(l2fwd PRIVATE dpdkpp ${DPDK_LIBRARIES})
target_compile_options(l2fwd PRIVATE ${DPDK_CFLAGS_OTHER})
