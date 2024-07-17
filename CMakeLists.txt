cmake_minimum_required(VERSION 3.10)

project(LechLed VERSION 1.0)

add_library(
    serial
    SimpleSerial.cpp
    SimpleSerial.h
)

add_executable(led main.cpp SimpleSerial.cpp)

target_link_libraries(led PRIVATE serial)