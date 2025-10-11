cmake_minimum_required(VERSION 3.13)
project(gpsdrpp_core)

if (USE_INTERNAL_LIBCORRECT)
    add_subdirectory("libcorrect/")
endif (USE_INTERNAL_LIBCORRECT)

# Main code
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

add_definitions(-DSDRPP_IS_CORE)

# Configure backend sources
if (OPT_BACKEND_GLFW)
    file(GLOB_RECURSE BACKEND_SRC "backends/glfw/*.cpp" "backends/glfw/*.c")
endif (OPT_BACKEND_GLFW)

# Add code to dyn lib
add_library(gpsdrpp_core SHARED ${SRC} ${BACKEND_SRC})

# Set compiler options
target_compile_options(gpsdrpp_core PRIVATE ${SDRPP_COMPILER_FLAGS})

# Set the install prefix
target_compile_definitions(gpsdrpp_core PUBLIC INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")

# Include core headers
target_include_directories(gpsdrpp_core PUBLIC "src/")
target_include_directories(gpsdrpp_core PUBLIC "src/imgui")
target_include_directories(gpsdrpp_core PRIVATE "../decoder_modules/radio/src")
target_include_directories(gpsdrpp_core PRIVATE "../decoder_modules/mode_s_decoder/src")
target_include_directories(gpsdrpp_core PRIVATE "../source_modules/rtl_sdr_source/src")

# Configure backend includes and libraries
if (OPT_BACKEND_GLFW)
    target_include_directories(gpsdrpp_core PUBLIC "backends/glfw")
    target_include_directories(gpsdrpp_core PUBLIC "backends/glfw/imgui")

    find_package(PkgConfig)
    pkg_check_modules(GLFW3 REQUIRED glfw3)

    target_include_directories(gpsdrpp_core PUBLIC ${GLFW3_INCLUDE_DIRS})
    target_link_directories(gpsdrpp_core PUBLIC ${GLFW3_LIBRARY_DIRS})
    target_link_libraries(gpsdrpp_core PUBLIC ${GLFW3_LIBRARIES})
endif (OPT_BACKEND_GLFW)

# Link to libcorrect
if (USE_INTERNAL_LIBCORRECT)
    target_include_directories(gpsdrpp_core PUBLIC "libcorrect/include")
    target_link_libraries(gpsdrpp_core PUBLIC correct_static)
endif (USE_INTERNAL_LIBCORRECT)

# Link to libcurl
find_package(CURL REQUIRED)
target_link_libraries(gpsdrpp_core PUBLIC CURL::libcurl)

find_package(PkgConfig)
find_package(OpenGL REQUIRED)

pkg_check_modules(FFTW3 REQUIRED fftw3f)
pkg_check_modules(VOLK REQUIRED volk)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(LIBZSTD REQUIRED libzstd)

target_include_directories(gpsdrpp_core PUBLIC
    ${OPENGL_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${GLFW3_INCLUDE_DIRS}
    ${VOLK_INCLUDE_DIRS}
    ${LIBZSTD_INCLUDE_DIRS}
)

target_link_directories(gpsdrpp_core PUBLIC
    ${OPENGL_LIBRARY_DIRS}
    ${FFTW3_LIBRARY_DIRS}
    ${GLFW3_LIBRARY_DIRS}
    ${VOLK_LIBRARY_DIRS}
    ${LIBZSTD_LIBRARY_DIRS}
)

target_link_libraries(gpsdrpp_core PUBLIC
    ${OPENGL_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${GLFW3_LIBRARIES}
    ${VOLK_LIBRARIES}
    ${LIBZSTD_LIBRARIES}
)

if (NOT USE_INTERNAL_LIBCORRECT)
    pkg_check_modules(CORRECT REQUIRED libcorrect)
    target_include_directories(gpsdrpp_core PUBLIC ${CORRECT_INCLUDE_DIRS})
    target_link_directories(gpsdrpp_core PUBLIC ${CORRECT_LIBRARY_DIRS})
    target_link_libraries(gpsdrpp_core PUBLIC ${CORRECT_LIBRARIES})
endif (NOT USE_INTERNAL_LIBCORRECT)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_libraries(gpsdrpp_core PUBLIC stdc++fs)
endif ()

set(CORE_FILES ${RUNTIME_OUTPUT_DIRECTORY} PARENT_SCOPE)

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"

# Install directives
install(TARGETS gpsdrpp_core DESTINATION lib)