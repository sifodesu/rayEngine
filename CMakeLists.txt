cmake_minimum_required(VERSION 3.15)
project(rayEngine)

set(CMAKE_CXX_STANDARD 20)

find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(raylib 3.0 REQUIRED) # Requires at least version 3.0

include_directories("libs/Quadtree-master/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../src/")
set(PROJECT_BINARY_DIR "src/")

include_directories("src/")
include_directories("src/Managers")
include_directories("src/Object")
include_directories("src/Object/Components")

file(GLOB_RECURSE CPPS "src/*.cpp")
file(GLOB_RECURSE CPPS2 "src/*/*.cpp")
file(GLOB_RECURSE CPPS3 "src/*/*/*.cpp")


add_executable(rayEngine ${CPPS} ${CPPS2} ${CPPS3} ${LIBS})

target_link_libraries(rayEngine raylib)
target_link_libraries(rayEngine nlohmann_json::nlohmann_json)
