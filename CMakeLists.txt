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

configure_file(src/config.h.in config.h @ONLY)
add_definitions(-std=gnu99)

find_package(PkgConfig)
pkg_search_module(PREDICT REQUIRED predict)

include_directories(${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src ${PREDICT_INCLUDE_DIRS})
link_directories(${PREDICT_LIBRARY_DIRS})

#main flyby executable
add_executable(flyby src/ui.c src/hamlib.c src/main.c src/string_array.c src/xdg_basedirs.c src/xdg_basedir_extras.c src/tle_db.c src/transponder_db.c src/qth_config.c src/filtered_menu.c src/transponder_editor.c src/multitrack.c src/locator.c src/option_help.c src/singletrack.c src/prediction_schedules.c src/hamlib_status.c src/field_helpers.c src/track_astronomical_bodies.c)
install(TARGETS flyby RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

target_link_libraries(flyby m ncurses menu form ${PREDICT_LIBRARIES})

#transponder database utility
set(TRANSPONDER_UTILITY_NAME "flyby-transponder-dbutil") #name of transponder utility executable
add_executable(transponder_utility src/transponder_utility.c src/tle_db.c src/transponder_db.c src/string_array.c src/xdg_basedirs.c src/xdg_basedir_extras.c src/option_help.c)
target_link_libraries(transponder_utility ${PREDICT_LIBRARIES} m)
install(TARGETS transponder_utility RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set_target_properties(transponder_utility PROPERTIES OUTPUT_NAME "${TRANSPONDER_UTILITY_NAME}")

#scripts
set(SATNOGS_FETCH_EXECUTABLE "flyby-satnogs-fetcher") #name of satnogs db fetcher executable
set(UPDATE_TLE_EXECUTABLE "flyby-update-tles") #name of TLE updater
configure_file(utils/fetch_satnogs_db.py.in ${SATNOGS_FETCH_EXECUTABLE} @ONLY)
configure_file(utils/update-tle.sh.in ${UPDATE_TLE_EXECUTABLE} @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${SATNOGS_FETCH_EXECUTABLE} DESTINATION bin/)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${UPDATE_TLE_EXECUTABLE} DESTINATION bin/)

#manpage
configure_file(docs/man/flyby.man flyby.1 @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/flyby.1 DESTINATION share/man/man1)

if (WITH_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif (WITH_TESTING)
