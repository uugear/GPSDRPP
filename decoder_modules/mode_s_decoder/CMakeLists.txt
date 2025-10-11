cmake_minimum_required(VERSION 3.13)
project(mode_s_decoder)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(mode_s_decoder PRIVATE "src/")
target_include_directories(mode_s_decoder PRIVATE "../../source_modules/rtl_sdr_source/src")

get_target_property(CURRENT_FLAGS mode_s_decoder COMPILE_OPTIONS)
if(CURRENT_FLAGS)
    string(REPLACE "-std=c++17" "" CURRENT_FLAGS "${CURRENT_FLAGS}")
    set_target_properties(mode_s_decoder PROPERTIES COMPILE_OPTIONS "${CURRENT_FLAGS}")
endif()

set_target_properties(mode_s_decoder PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
