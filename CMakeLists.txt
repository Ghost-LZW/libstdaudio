# libstdaudio
# Copyright (c) 2018 - Timur Doumler
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.12)
project(libstdaudio
  VERSION 0.1.0
  LANGUAGES CXX
)

include(CheckIncludeFileCXX)

set(CMAKE_CXX_STANDARD 23)

if (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
else ()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif ()

if (APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "-framework CoreAudio")
endif ()

CHECK_INCLUDE_FILE_CXX(mdspan MDSPAN_EXIST)
if (NOT MDSPAN_EXIST)
  add_subdirectory(third_party/mdspan)
endif()

add_library(audio INTERFACE)
add_library(std::audio ALIAS audio)
target_include_directories(audio INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(audio INTERFACE std::mdspan)

###################################################

install(TARGETS audio EXPORT audioTargets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT audioTargets
    FILE audioTargets.cmake
    NAMESPACE std::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/audio
)

export(TARGETS audio
    NAMESPACE std::
    FILE audioTargets.cmake
)

install(DIRECTORY include/experimental DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/audioConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/audioConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/audio
)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/audioConfigVersion.cmake
  COMPATIBILITY SameMajorVersion
  ARCH_INDEPENDENT
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/audioConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/audioConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/audio
)

###################################################
# Test and Examples, move into their own directory
###################################################

include_directories(include)

add_executable(white_noise examples/white_noise.cpp)
add_executable(print_devices examples/print_devices.cpp)
add_executable(sine_wave examples/sine_wave.cpp)
add_executable(melody examples/melody.cpp)
add_executable(level_meter examples/level_meter.cpp)

add_executable(test
        test/test_main.cpp
        test/audio_buffer_test.cpp
        test/audio_device_test.cpp)
