cmake_minimum_required(VERSION 3.5)

project(ui_http_server LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_executable(ui_http_server
  main.cpp
  HttpServer.cpp
  HttpServer.h
  RecvStream.cpp
  RecvStream.h
  HttpServer.ui
)

target_link_libraries(ui_http_server PRIVATE Qt5::Widgets)

set(CMAKE_CXX_FLAGS -pthread)
