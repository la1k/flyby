cmake_minimum_required(VERSION 2.6)
project(flyby)

add_definitions(-O3 -s -fomit-frame-pointer -pthread)
add_executable(flyby flyby.c)

target_link_libraries(flyby pthread)
target_link_libraries(flyby m)
target_link_libraries(flyby ncurses)
target_link_libraries(flyby menu)

