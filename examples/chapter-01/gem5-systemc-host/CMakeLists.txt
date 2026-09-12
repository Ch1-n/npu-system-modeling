cmake_minimum_required(VERSION 3.20)
project(gem5_systemc_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SYSTEMC_HOME "$ENV{SYSTEMC_HOME}" CACHE PATH "SystemC installation prefix")
set(GEM5_ROOT "" CACHE PATH "gem5 source directory")
set(GEM5_BUILD_DIR "${GEM5_ROOT}/build/RISCV_LIB" CACHE PATH "libgem5 build directory")

if(NOT SYSTEMC_HOME)
    message(FATAL_ERROR "Set SYSTEMC_HOME to the SystemC installation prefix")
endif()
if(NOT EXISTS "${GEM5_ROOT}/SConstruct")
    message(FATAL_ERROR "Set GEM5_ROOT to a gem5 source tree")
endif()

if(APPLE)
    set(GEM5_LIBRARY "${GEM5_BUILD_DIR}/libgem5_opt.dylib")
elseif(UNIX)
    set(GEM5_LIBRARY "${GEM5_BUILD_DIR}/libgem5_opt.so")
else()
    message(FATAL_ERROR "Chapter 1 supports macOS and Linux hosts")
endif()

if(NOT EXISTS "${GEM5_LIBRARY}")
    message(FATAL_ERROR "Missing ${GEM5_LIBRARY}; run scripts/build_libgem5.sh")
endif()

find_library(SYSTEMC_LIBRARY systemc PATHS "${SYSTEMC_HOME}/lib" NO_DEFAULT_PATH)
if(NOT SYSTEMC_LIBRARY)
    file(GLOB SYSTEMC_VERSIONED_LIBRARIES
        "${SYSTEMC_HOME}/lib/libsystemc*.dylib"
        "${SYSTEMC_HOME}/lib/libsystemc*.so")
    list(LENGTH SYSTEMC_VERSIONED_LIBRARIES SYSTEMC_LIBRARY_COUNT)
    if(SYSTEMC_LIBRARY_COUNT EQUAL 0)
        message(FATAL_ERROR "No SystemC library found under ${SYSTEMC_HOME}/lib")
    endif()
    list(GET SYSTEMC_VERSIONED_LIBRARIES 0 SYSTEMC_LIBRARY)
endif()
set(GEM5_SYSTEMC_UTIL "${GEM5_ROOT}/util/systemc/gem5_within_systemc")

add_executable(gem5_systemc_host
    src/main.cpp
    "${GEM5_SYSTEMC_UTIL}/sc_module.cc"
    "${GEM5_SYSTEMC_UTIL}/sc_logger.cc"
    "${GEM5_SYSTEMC_UTIL}/stats.cc"
)

target_compile_definitions(gem5_systemc_host PRIVATE TRACING_ON=1)
target_include_directories(gem5_systemc_host PRIVATE
    "${SYSTEMC_HOME}/include"
    "${GEM5_BUILD_DIR}"
    "${GEM5_ROOT}/src"
    "${GEM5_ROOT}/ext"
    "${GEM5_SYSTEMC_UTIL}"
)
target_link_libraries(gem5_systemc_host PRIVATE
    "${SYSTEMC_LIBRARY}"
    "${GEM5_LIBRARY}"
)
set_target_properties(gem5_systemc_host PROPERTIES
    BUILD_RPATH "${GEM5_BUILD_DIR};${SYSTEMC_HOME}/lib"
)

if(APPLE)
    file(RELATIVE_PATH GEM5_INSTALL_NAME "${GEM5_ROOT}" "${GEM5_LIBRARY}")
    add_custom_command(TARGET gem5_systemc_host POST_BUILD
        COMMAND "${CMAKE_INSTALL_NAME_TOOL}"
            -change "${GEM5_INSTALL_NAME}" "${GEM5_LIBRARY}"
            "$<TARGET_FILE:gem5_systemc_host>"
        COMMENT "Fixing libgem5 install name for macOS"
    )
endif()
