cmake_minimum_required(VERSION 3.13)
project(pld_de10_tests CXX)
set(CMAKE_CXX_STANDARD 17)
enable_testing()

# portable sources only
add_library(fw_core STATIC ../src/core/packet.cpp)
target_include_directories(fw_core PUBLIC ../src/core)

# doctest's main(), compiled once
add_library(test_main OBJECT test_main.cpp)
target_include_directories(test_main PUBLIC doctest generated)

file(GLOB TESTS CONFIGURE_DEPENDS test_*.cpp)
list(REMOVE_ITEM TESTS ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp)
foreach(src ${TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} $<TARGET_OBJECTS:test_main>)
  target_include_directories(${name} PRIVATE doctest generated)
  target_link_libraries(${name} fw_core)
  target_compile_options(${name} PRIVATE -Wall -Wextra -fsanitize=address,undefined)
  target_link_options(${name} PRIVATE -fsanitize=address,undefined)
  add_test(NAME ${name} COMMAND ${name})
endforeach()