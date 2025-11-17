cmake_minimum_required(VERSION 3.10)

project(twamp_light VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
add_executable(
    twamp_light 
    src/main.cpp 
    src/twamp_light_sender.cpp 
    src/twamp_light_reflector.cpp
    src/twamp_light_packet.cpp
    )
