cmake_minimum_required(VERSION 3.23)
project(reliable_over_udp)

set(CMAKE_CXX_STANDARD 20)

add_executable(reliable_over_udp
        main.cpp
        unreliable.cpp
        reliable_GBN.cpp
        reliable_SR.cpp
        reliable_RENO.cpp
        packet.cpp
        )

target_link_libraries(reliable_over_udp ws2_32)