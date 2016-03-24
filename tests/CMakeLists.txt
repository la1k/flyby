add_executable(dummy-t dummy-t.c)
add_test(NAME dummy COMMAND dummy-t)

find_package(CMocka REQUIRED)

add_executable(xdg-dirs-t xdg-dirs-t.c ${CMAKE_SOURCE_DIR}/src/xdg_basedirs.c)
target_link_libraries(xdg-dirs-t ${CMOCKA_LIBRARY})
add_test(NAME xdg-dirs COMMAND xdg-dirs-t)
