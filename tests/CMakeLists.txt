add_executable(dummy-t dummy-t.c)
add_test(NAME dummy COMMAND dummy-t)

find_package(CMocka REQUIRED)

#xdg basedir tests
add_executable(xdg-dirs-t xdg-dirs-t.c ${CMAKE_SOURCE_DIR}/src/xdg_basedirs.c ${CMAKE_SOURCE_DIR}/src/string_array.c)
target_link_libraries(xdg-dirs-t ${CMOCKA_LIBRARY})
add_test(NAME xdg-dirs COMMAND xdg-dirs-t)

#qth file operation tests
configure_file(test_data/flyby.qth.in test_data/flyby/flyby.qth COPYONLY)
add_executable(qth-t qth-t.c ${CMAKE_SOURCE_DIR}/src/qth_config.c ${CMAKE_SOURCE_DIR}/src/string_array.c)
target_link_libraries(qth-t ${CMOCKA_LIBRARY} predict)
add_test(NAME qth COMMAND qth-t)

#string array tests
add_executable(string-array-t string-array-t.c ${CMAKE_SOURCE_DIR}/src/string_array.c)
target_link_libraries(string-array-t ${CMOCKA_LIBRARY})
add_test(NAME string-array COMMAND string-array-t)

#TLE test files
configure_file(test_data/old_tles/part1.tle.in test_data/old_tles/part1.tle COPYONLY)
configure_file(test_data/old_tles/part2.tle.in test_data/old_tles/part2.tle COPYONLY)
configure_file(test_data/newer_tles/amateur.txt.in test_data/newer_tles/amateur.txt COPYONLY)
configure_file(test_data/flyby.whitelist.in test_data/flyby/flyby.whitelist COPYONLY)

#TLEs in paths compatible with the XDG_DATA_HOME setup
configure_file(test_data/old_tles/part1.tle.in test_data/old_tles/flyby/tles/part1.tle COPYONLY)
configure_file(test_data/old_tles/part2.tle.in test_data/old_tles/flyby/tles/part2.tle COPYONLY)
configure_file(test_data/newer_tles/amateur.txt.in test_data/newer_tles/flyby/tles/amateur.txt COPYONLY)
configure_file(test_data/old_tles/part1.tle.in test_data/mixture/flyby/tles/part1.tle COPYONLY)
configure_file(test_data/old_tles/part2.tle.in test_data/mixture/flyby/tles/part2.tle COPYONLY)
configure_file(test_data/newer_tles/amateur.txt.in test_data/mixture/flyby/tles/amateur.txt COPYONLY)

#TLE db tests
add_executable(tle-db-t tle-db-t.c ${CMAKE_SOURCE_DIR}/src/tle_db.c ${CMAKE_SOURCE_DIR}/src/string_array.c)
target_link_libraries(tle-db-t ${CMOCKA_LIBRARY} predict)
add_test(NAME tle-db COMMAND tle-db-t)

#transponder db test file
configure_file(test_data/flyby.db.in test_data/flyby/flyby.db)

#transponder db tests
add_executable(transponder-db-t transponder-db-t.c ${CMAKE_SOURCE_DIR}/src/transponder_db.c ${CMAKE_SOURCE_DIR}/src/string_array.c ${CMAKE_SOURCE_DIR}/src/tle_db.c)
target_link_libraries(transponder-db-t ${CMOCKA_LIBRARY} predict)
add_test(NAME transponder-db COMMAND transponder-db-t)

#locator test
add_executable(locator-conversion-t locator-conversion-t.c ${CMAKE_SOURCE_DIR}/src/locator.c)
target_link_libraries(locator-conversion-t ${CMOCKA_LIBRARY} m)
add_test(NAME locator-conversion COMMAND locator-conversion-t)
