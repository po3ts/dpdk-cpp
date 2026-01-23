cmake_minimum_required(VERSION 3.20)
project(l2fwd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        l2fwd.cc
        device_manager.cc
        device.cc
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

target_include_directories(${PROJECT_NAME} PRIVATE ${DPDK_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${DPDK_LIBRARIES})
