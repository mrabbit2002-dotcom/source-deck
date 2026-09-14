cmake_minimum_required(VERSION 3.28...3.30)
project(source-deck VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(${PROJECT_NAME} MODULE
  src/plugin-main.cpp
  src/source-deck-dock.cpp
  src/source-deck-dock.hpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Widgets)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

if(COMMAND setup_plugin_target)
  setup_plugin_target(${PROJECT_NAME})
endif()
