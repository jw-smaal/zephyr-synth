# Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(midi-synth)

set(GEN_LUTS_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_luts.py)
set(LUTS_HEADER ${CMAKE_CURRENT_BINARY_DIR}/include/synth_luts_generated.h)

add_custom_command(
    OUTPUT ${LUTS_HEADER}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include
    COMMAND python3 ${GEN_LUTS_SCRIPT} ${CONFIG_AUDIO_SAMPLE_RATE} ${LUTS_HEADER}
    DEPENDS ${GEN_LUTS_SCRIPT}
)

target_sources(app PRIVATE 
    src/main.c 
    src/audio.c 
    src/synth_engine.c
    src/midi_handler.c
    src/adsr.c 
)

target_include_directories(app PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/include)
set_source_files_properties(src/synth_engine.c PROPERTIES OBJECT_DEPENDS ${LUTS_HEADER})
