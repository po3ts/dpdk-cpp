cmake_minimum_required(VERSION 3.20)
project(dpdkpp)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        ${DPDK_INCLUDE_DIRS}
)

target_link_libraries(dpdkpp PUBLIC ${DPDK_LIBRARIES})
target_compile_options(dpdkpp PUBLIC ${DPDK_CFLAGS_OTHER})
target_compile_features(dpdkpp PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
# Example: l2fwd
# ---------------------------------------------------------------------------
add_executable(l2fwd)

target_sources(l2fwd
    PRIVATE
        examples/l2fwd/l2fwd.cc
)

target_link_libraries(l2fwd PRIVATE dpdkpp)
target_compile_features(l2fwd PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS dpdkpp
    EXPORT dpdkppTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT dpdkppTargets
    FILE dpdkppTargets.cmake
    NAMESPACE dpdkpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dpdkpp
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/dpdkppConfigVersion.cmake"
    VERSION 0.1.0
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dpdkppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dpdkppConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dpdkpp
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/dpdkppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/dpdkppConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dpdkpp
)
