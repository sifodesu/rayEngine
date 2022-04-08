cmake_minimum_required(VERSION 3.15)
project(rayEngine)

set(CMAKE_CXX_STANDARD_REQUIRED 20)
set(CMAKE_CXX_FLAGS "-std=c++20")

include_directories("libs/Quadtree-master/include")
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "./src/")
# set(PROJECT_BINARY_DIR "src/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "..")
set(PROJECT_BINARY_DIR "..")

include_directories("src/")
include_directories("src/Managers")
include_directories("src/Objects")
include_directories("src/Objects/Bullet")
include_directories("src/Components")

file(GLOB_RECURSE CPPS "src/*.cpp")
file(GLOB_RECURSE CPPS2 "src/*/*.cpp")
file(GLOB_RECURSE CPPS3 "src/*/*/*.cpp")


add_executable(rayEngine ${CPPS} ${CPPS2} ${CPPS3} ${LIBS})

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(libs/json-3.10.2)
target_link_libraries(rayEngine nlohmann_json::nlohmann_json)

add_subdirectory(libs/raylib-4.0.0)
target_link_libraries(rayEngine raylib)
