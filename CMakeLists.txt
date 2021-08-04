cmake_minimum_required(VERSION 3.0.0)
project(rayEngine)
set(CMAKE_CXX_STANDARD 20)

find_package(nlohmann_json 3.2.0 REQUIRED)

include_directories("libs/Quadtree-master/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../src/")
set(PROJECT_BINARY_DIR "src/")
# include_directories("src/Managers")
# include_directories("src/Components")
include_directories("src/")

file(GLOB_RECURSE CPPS "src/*.cpp")
file(GLOB_RECURSE CPPS2 "src/*/*.cpp")

add_executable(rayEngine ${CPPS} ${CPPS2} ${LIBS})

target_link_libraries(rayEngine nlohmann_json::nlohmann_json)
