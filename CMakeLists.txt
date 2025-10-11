cmake_minimum_required(VERSION 3.13)
project(gpsdrpp)

# Backends
option(OPT_BACKEND_GLFW "Use the GLFW backend" ON)

# Sources
option(OPT_BUILD_RTL_SDR_SOURCE "Build RTL-SDR Source Module (Dependencies: librtlsdr)" ON)

# Sinks
option(OPT_BUILD_AUDIO_SINK "Build Audio Sink Module (Dependencies: rtaudio)" ON)
#option(OPT_BUILD_NETWORK_SINK "Build Audio Sink Module (no dependencies required)" OFF)
#option(OPT_BUILD_NEW_PORTAUDIO_SINK "Build the new PortAudio Sink Module (Dependencies: portaudio)" ON)
#option(OPT_BUILD_PORTAUDIO_SINK "Build PortAudio Sink Module (Dependencies: portaudio)" OFF)

# Decoders
#option(OPT_BUILD_ATV_DECODER "Build ATV decoder (no dependencies required)" OFF)
#option(OPT_BUILD_FALCON9_DECODER "Build the falcon9 live decoder (Dependencies: ffplay)" OFF)
#option(OPT_BUILD_KG_SSTV_DECODER "Build the KG SSTV (KG-STV) decoder module (no dependencies required)" OFF)
#option(OPT_BUILD_M17_DECODER "Build the M17 decoder module (Dependencies: codec2)" OFF)
#option(OPT_BUILD_METEOR_DEMODULATOR "Build the meteor demodulator module (no dependencies required)" ON)
#option(OPT_BUILD_PAGER_DECODER "Build the pager decoder module (no dependencies required)" ON)
option(OPT_BUILD_RADIO "Main audio modulation decoder (AM, FM, SSB, etc...)" ON)
#option(OPT_BUILD_RYFI_DECODER "RyFi data link decoder" OFF)
#option(OPT_BUILD_WEATHER_SAT_DECODER "Build the HRPT decoder module (no dependencies required)" OFF)
option(OPT_BUILD_MODE_S "Build Mode-S(ADS-B) decoder" ON)


# Misc
option(OPT_BUILD_FREQUENCY_MANAGER "Build the Frequency Manager module" ON)
#option(OPT_BUILD_IQ_EXPORTER "Build the IQ Exporter module" ON)
option(OPT_BUILD_RECORDER "Audio and baseband recorder" ON)
#option(OPT_BUILD_RIGCTL_CLIENT "Rigctl client to make SDR++ act as a panadapter" OFF)
#option(OPT_BUILD_RIGCTL_SERVER "Rigctl backend for controlling SDR++ with software like gpredict" OFF)
#option(OPT_BUILD_SCANNER "Frequency scanner" ON)
#option(OPT_BUILD_SCHEDULER "Build the scheduler" OFF)

# Other options
option(USE_INTERNAL_LIBCORRECT "Use an internal version of libcorrect" ON)

# Module cmake path
set(SDRPP_MODULE_CMAKE "${CMAKE_SOURCE_DIR}/sdrpp_module.cmake")

# Root source folder
set(SDRPP_CORE_ROOT "${CMAKE_SOURCE_DIR}/core/src/")

# Compiler flags
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    # Debug Flags
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_COMPILER_FLAGS -g -Og -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
    else ()
        set(SDRPP_COMPILER_FLAGS -g -Og -std=c++17)
    endif ()
else()
    # Normal Flags
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_COMPILER_FLAGS -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
    else ()
        set(SDRPP_COMPILER_FLAGS -O3 -std=c++17)
    endif ()
endif()
set(SDRPP_MODULE_COMPILER_FLAGS ${SDRPP_COMPILER_FLAGS})

# Set a default install prefix
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "..." FORCE)
endif()

# Core of SDR++
add_subdirectory("core")

# Source modules
if (OPT_BUILD_RTL_SDR_SOURCE)
add_subdirectory("source_modules/rtl_sdr_source")
endif (OPT_BUILD_RTL_SDR_SOURCE)

# Sink modules
if (OPT_BUILD_AUDIO_SINK)
add_subdirectory("sink_modules/audio_sink")
endif (OPT_BUILD_AUDIO_SINK)

#if (OPT_BUILD_PORTAUDIO_SINK)
#add_subdirectory("sink_modules/portaudio_sink")
#endif (OPT_BUILD_PORTAUDIO_SINK)

#if (OPT_BUILD_NETWORK_SINK)
#add_subdirectory("sink_modules/network_sink")
#endif (OPT_BUILD_NETWORK_SINK)

#if (OPT_BUILD_NEW_PORTAUDIO_SINK)
#add_subdirectory("sink_modules/new_portaudio_sink")
#endif (OPT_BUILD_NEW_PORTAUDIO_SINK)


# Decoders
#if (OPT_BUILD_ATV_DECODER)
#add_subdirectory("decoder_modules/atv_decoder")
#endif (OPT_BUILD_ATV_DECODER)

#if (OPT_BUILD_FALCON9_DECODER)
#add_subdirectory("decoder_modules/falcon9_decoder")
#endif (OPT_BUILD_FALCON9_DECODER)

#if (OPT_BUILD_KG_SSTV_DECODER)
#add_subdirectory("decoder_modules/kg_sstv_decoder")
#endif (OPT_BUILD_KG_SSTV_DECODER)

#if (OPT_BUILD_M17_DECODER)
#add_subdirectory("decoder_modules/m17_decoder")
#endif (OPT_BUILD_M17_DECODER)

#if (OPT_BUILD_METEOR_DEMODULATOR)
#add_subdirectory("decoder_modules/meteor_demodulator")
#endif (OPT_BUILD_METEOR_DEMODULATOR)

#if (OPT_BUILD_PAGER_DECODER)
#add_subdirectory("decoder_modules/pager_decoder")
#endif (OPT_BUILD_PAGER_DECODER)

if (OPT_BUILD_RADIO)
add_subdirectory("decoder_modules/radio")
endif (OPT_BUILD_RADIO)

#if (OPT_BUILD_RYFI_DECODER)
#add_subdirectory("decoder_modules/ryfi_decoder")
#endif (OPT_BUILD_RYFI_DECODER)

#if (OPT_BUILD_WEATHER_SAT_DECODER)
#add_subdirectory("decoder_modules/weather_sat_decoder")
#endif (OPT_BUILD_WEATHER_SAT_DECODER)

if (OPT_BUILD_MODE_S)
add_subdirectory("decoder_modules/mode_s_decoder")
endif (OPT_BUILD_MODE_S)

# Misc
if (OPT_BUILD_FREQUENCY_MANAGER)
add_subdirectory("misc_modules/frequency_manager")
endif (OPT_BUILD_FREQUENCY_MANAGER)

#if (OPT_BUILD_IQ_EXPORTER)
#add_subdirectory("misc_modules/iq_exporter")
#endif (OPT_BUILD_IQ_EXPORTER)

if (OPT_BUILD_RECORDER)
add_subdirectory("misc_modules/recorder")
endif (OPT_BUILD_RECORDER)

#if (OPT_BUILD_RIGCTL_CLIENT)
#add_subdirectory("misc_modules/rigctl_client")
#endif (OPT_BUILD_RIGCTL_CLIENT)

#if (OPT_BUILD_RIGCTL_SERVER)
#add_subdirectory("misc_modules/rigctl_server")
#endif (OPT_BUILD_RIGCTL_SERVER)

#if (OPT_BUILD_SCANNER)
#add_subdirectory("misc_modules/scanner")
#endif (OPT_BUILD_SCANNER)

#if (OPT_BUILD_SCHEDULER)
#add_subdirectory("misc_modules/scheduler")
#endif (OPT_BUILD_SCHEDULER)

add_executable(gpsdrpp "src/main.cpp")

target_link_libraries(gpsdrpp PRIVATE gpsdrpp_core)

# Link to OpenGL library
find_library(GL_LIBRARY NAMES GL PATHS "/usr/lib/aarch64-linux-gnu/libGL.so")
if (GL_LIBRARY)
    message(STATUS "Using custom libGL: ${GL_LIBRARY}")
    target_link_libraries(gpsdrpp PRIVATE ${GL_LIBRARY})
else()
    message(FATAL_ERROR "libGL.so not found")
endif ()

# Link to gpiod library
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED libgpiod)
include_directories(${GPIOD_INCLUDE_DIRS})
link_directories(${GPIOD_LIBRARY_DIRS})
target_link_libraries(gpsdrpp PRIVATE ${GPIOD_LIBRARIES})

# Compiler arguments
target_compile_options(gpsdrpp PRIVATE ${SDRPP_COMPILER_FLAGS})

if (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:gpsdrpp_core>/libgpsdrpp_core.so\" \"$<TARGET_FILE_DIR:gpsdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    target_link_libraries(sdrpp PUBLIC pthread)
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:gpsdrpp_core>/libgpsdrpp_core.so\" \"$<TARGET_FILE_DIR:gpsdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:gpsdrpp_core>/libgpsdrpp_core.so\" \"$<TARGET_FILE_DIR:gpsdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:gpsdrpp_core>/libgpsdrpp_core.dylib\" \"$<TARGET_FILE_DIR:gpsdrpp>\")
endif ()

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" -DOPT_BUILD_BLADERF_SOURCE=ON -DOPT_BUILD_LIMESDR_SOURCE=ON -DOPT_BUILD_SDRPLAY_SOURCE=ON -DOPT_BUILD_M17_DECODER=ON -DOPT_BUILD_SCANNER=ON -DOPT_BUILD_SCHEDULER=ON -DOPT_BUILD_USRP_SOURCE=ON -DOPT_BUILD_PAGER_DECODER=ON

# Create module cmake file
configure_file(${CMAKE_SOURCE_DIR}/sdrpp_module.cmake ${CMAKE_CURRENT_BINARY_DIR}/sdrpp_module.cmake @ONLY)

# Install directives
install(TARGETS gpsdrpp DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/bandplans DESTINATION share/gpsdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/colormaps DESTINATION share/gpsdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/fonts DESTINATION share/gpsdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/icons DESTINATION share/gpsdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/themes DESTINATION share/gpsdrpp)
configure_file(${CMAKE_SOURCE_DIR}/gpsdrpp.desktop ${CMAKE_CURRENT_BINARY_DIR}/gpsdrpp.desktop @ONLY)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gpsdrpp.desktop DESTINATION share/applications)
endif ()

# Create uninstall target
configure_file(${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake @ONLY)
add_custom_target(uninstall ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# Create headers target
