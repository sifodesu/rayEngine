cmake_minimum_required(VERSION 3.15)
project(rayEngine)

set(CMAKE_CXX_STANDARD 20)

include_directories("libs/Quadtree-master/include")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../src/")
set(PROJECT_BINARY_DIR "src/")

include_directories("src/")
include_directories("src/Managers")
include_directories("src/Objects")
include_directories("src/Components")

file(GLOB_RECURSE CPPS "src/*.cpp")
file(GLOB_RECURSE CPPS2 "src/*/*.cpp")
file(GLOB_RECURSE CPPS3 "src/*/*/*.cpp")


add_executable(rayEngine ${CPPS} ${CPPS2} ${CPPS3} ${LIBS})

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(libs/json-3.10.2)
# include(FetchContent)

# FetchContent_Declare(json
#   GIT_REPOSITORY https://github.com/nlohmann/json.git
#   GIT_TAG v3.9.1)

# FetchContent_GetProperties(json)
# if(NOT json_POPULATED)
#   FetchContent_Populate(json)
#   add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
# endif()

target_link_libraries(rayEngine nlohmann_json::nlohmann_json)


find_package(raylib 3.0 QUIET) # Let CMake search for a raylib-config.cmake

# You could change the QUIET above to REQUIRED and remove this if() clause
# This part downloads raylib and builds it if it's not installed on your system
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
  include(FetchContent)

  FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/master.tar.gz
  )

  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_Populate(raylib)

    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples

    # build raylib
    add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})

  endif()

endif()

target_link_libraries(rayEngine raylib)
