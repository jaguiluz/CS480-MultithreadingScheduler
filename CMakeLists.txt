cmake_minimum_required(VERSION 4.0)
project(CS480_Assignment4)

set(CMAKE_CXX_STANDARD 20)

add_executable(CS480_Assignment4 main.cpp
        producer.cpp
        producer.h
        log.cpp
        log.h
        seating.h
        consumer.cpp
        consumer.h)
