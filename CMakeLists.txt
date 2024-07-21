cmake_minimum_required(VERSION 3.15)
project(rayEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/..)

file(GLOB_RECURSE headers LIST_DIRECTORIES true "*.h")
foreach(header ${headers})
    get_filename_component(dir ${header} DIRECTORY)
    include_directories(${dir})
endforeach()

file(GLOB_RECURSE CPPS "src/*.cpp")

set(RAYLIB_VERSION 5.0)
find_package(raylib ${RAYLIB_VERSION} QUIET) # QUIET or REQUIRED
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
include(FetchContent)
FetchContent_Declare(
  raylib
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
  set(FETCHCONTENT_QUIET NO)
  FetchContent_Populate(raylib)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
  add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
  endif()
  endif()

  
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

add_executable(rayEngine ${CPPS})

target_link_libraries(rayEngine PRIVATE nlohmann_json::nlohmann_json raylib)