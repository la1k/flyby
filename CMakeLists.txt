# The minimum required version hasn't been tested. Feel free to adjust
# downwards if necessary.
cmake_minimum_required(VERSION 2.8)

project(flyby C)

include(GNUInstallDirs)

# These variables will be set automatically by newer CMake (at least
# 3.0) if the project command is given a VERSION argument. Set them
# manually for now.
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
#PROJECT_VERSION_PATCH isn't used
#PROJECT_VERSION_TWEAK isn't used
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(FLYBY_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(FLYBY_VERSION_MINOR ${PROJECT_VERSION_MINOR})
#FLYBY_VERSION_PATCH isn't used
#FLYBY_VERSION_TWEAK isn't used
set(FLYBY_VERSION ${PROJECT_VERSION})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

configure_file(config.h.in config.h @ONLY)
include_directories(${CMAKE_BINARY_DIR})
add_executable(flyby src/ui.c src/hamlib.c src/main.c src/string_array.c src/xdg_basedirs.c src/tle_db.c src/transponder_db.c src/qth_config.c src/filtered_menu.c src/transponder_editor.c)
install(TARGETS flyby RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_definitions(-std=gnu99)
target_link_libraries(flyby m)
target_link_libraries(flyby ncurses)
target_link_libraries(flyby menu)
target_link_libraries(flyby form)
target_link_libraries(flyby predict)
