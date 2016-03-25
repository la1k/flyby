add_executable(dummy-t dummy-t.c)
add_test(NAME dummy COMMAND dummy-t)

find_package(CMocka REQUIRED)

add_executable(xdg-dirs-t xdg-dirs-t.c ${CMAKE_SOURCE_DIR}/src/xdg_basedirs.c)
target_link_libraries(xdg-dirs-t ${CMOCKA_LIBRARY})
add_test(NAME xdg-dirs COMMAND xdg-dirs-t)

configure_file(test_data/flyby/flyby.qth.in test_data/flyby/flyby.qth ONLY)
add_executable(qth-t qth-t.c ${CMAKE_SOURCE_DIR}/src/qth_config.c ${CMAKE_SOURCE_DIR}/src/string_array.c)
target_link_libraries(qth-t ${CMOCKA_LIBRARY} predict)
add_test(NAME qth COMMAND qth-t)
