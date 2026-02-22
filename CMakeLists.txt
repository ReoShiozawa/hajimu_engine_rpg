cmake_minimum_required(VERSION 3.20)
project(engine_rpg C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(HAJIMU_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../../jp/include")
message(STATUS "HAJIMU_INCLUDE_DIR = ${HAJIMU_INCLUDE_DIR}")

add_library(engine_rpg SHARED
    src/eng_db.c
    src/eng_battle.c
    src/eng_dialog.c
    src/eng_save.c
    src/plugin.c
)

target_include_directories(engine_rpg PRIVATE
    ${HAJIMU_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

if(UNIX AND NOT APPLE)
    target_link_libraries(engine_rpg PRIVATE m)
endif()

set_target_properties(engine_rpg PROPERTIES
    OUTPUT_NAME "engine_rpg"
    SUFFIX ".hjp"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
)
