cmake_minimum_required(VERSION 3.20)
project(systemc_hello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SYSTEMC_HOME "$ENV{SYSTEMC_HOME}" CACHE PATH "SystemC installation prefix")
if(NOT SYSTEMC_HOME)
    message(FATAL_ERROR "Set SYSTEMC_HOME to the SystemC installation prefix")
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

add_executable(systemc_hello main.cpp)
target_include_directories(systemc_hello PRIVATE "${SYSTEMC_HOME}/include")
target_link_libraries(systemc_hello PRIVATE "${SYSTEMC_LIBRARY}")
set_target_properties(systemc_hello PROPERTIES BUILD_RPATH "${SYSTEMC_HOME}/lib")

enable_testing()
add_test(NAME systemc_hello COMMAND systemc_hello)
