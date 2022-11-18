cmake_minimum_required(VERSION 3.23)
project(Server)

set(CMAKE_CXX_STANDARD 17)

# essential to link to the library!!! and be careful about the order of this command
link_libraries(ws2_32)

add_executable(Server main.cpp Server.cpp Server.h PacketManager.cpp PacketManager.h Util.cpp Util.h)


