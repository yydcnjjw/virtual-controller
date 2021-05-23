cmake_minimum_required(VERSION 3.11)
project(input-test CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(evdev libevdev)

find_package(Threads REQUIRED)

add_executable(virtual-controller
  main.cc
  )

target_link_libraries(virtual-controller
  Threads::Threads)
