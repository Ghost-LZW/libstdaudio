# libstdaudio
# Copyright (c) 2018 - Timur Doumler
# Copyright (c) 2023 - Lan Zongwei
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.12)
project(libstdaudio
  VERSION 0.1.0
  LANGUAGES CXX
)

include(CheckIncludeFileCXX)

set(CMAKE_CXX_STANDARD 23)

option(AUDIO_ENABLE_TESTS "Enable tests." ON)
option(AUDIO_ENABLE_EXAMPLES "Build examples." ON)
option(AUDIO_WITH_SDL3 "Enable SDL backend." ON)
option(AUDIO_STATIC "Use static libraries" OFF)

find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init --recursive\
 failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
endif()

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

find_package(PkgConfig)

if (AUDIO_WITH_SDL3)
  pkg_search_module(SDL3 sdl3)

  add_compile_definitions(AUDIO_USE_SDL3 DEBUG_AUDIO)

  if (SDL3_FOUND)
    message(STATUS "System SDL3 found.")
  else()
    message(STATUS "Use internal SDL3.")

    # Disable all subsystems except Audio, Events, Threads, and loadso(for shared library)
    set(SDL_SUBSYSTEMS
        Atomic Video Render Joystick Haptic Hidapi Power Timers
        File CPUinfo Filesystem Sensor Locale Misc)
    foreach(_SUB ${SDL_SUBSYSTEMS})
      string(TOUPPER ${_SUB} _OPT)
      set(SDL_${_OPT} OFF CACHE BOOL "Enable the ${_SUB} subsystem" FORCE)
    endforeach()

    if (AUDIO_STATIC)
      set(SDL_STATIC ON CACHE BOOL "Build a static version of SDL" FORCE)
      set(SDL_LOADSO OFF CACHE BOOL "Enable the LOADSO subsystem" FORCE)

      foreach(AUDIO_LIB ALSA JACK PIPEWIRE PULSEAUDIO SNDIO)
        set(SDL_${AUDIO_LIB}_SHARED OFF CACHE BOOL "Dynamically load ${AUDIO_LIB} audio support" FORCE)
      endforeach()
    endif()

    set(SDL_DISABLE_INSTALL OFF CACHE BOOL "Disable installation of SDL3" FORCE)
    add_subdirectory(third_party/SDL)
  endif()
endif()

add_library(audio INTERFACE)
add_library(std::audio ALIAS audio)
target_include_directories(audio INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(audio INTERFACE std::mdspan)

if (AUDIO_WITH_SDL3)
  if (AUDIO_STATIC)
    target_link_libraries(audio INTERFACE SDL3::SDL3-static)
  else()
    target_link_libraries(audio INTERFACE SDL3::SDL3)
  endif()
endif()

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
# Test and Examples
###################################################

if (AUDIO_ENABLE_EXAMPLES)
  add_subdirectory(examples)
endif()

if (AUDIO_ENABLE_TESTS)
  add_subdirectory(test)
endif()
