cmake_minimum_required(VERSION 3.21)
project(vulkan_tutorial LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Find Vulkan normally
find_package(Vulkan REQUIRED)

include(FetchContent)

# nlohmann_json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# glm
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# SDL3
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.2.x
)
FetchContent_MakeAvailable(SDL3)

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")

add_executable(vulkan_tutorial "${MY_SOURCES}")

# SDL3 provides a proper CMake target called SDL3::SDL3

target_sources(vulkan_tutorial PRIVATE "${MY_SOURCES}")
target_include_directories(vulkan_tutorial PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source/")
target_link_libraries(vulkan_tutorial
  PRIVATE
    Vulkan::Vulkan
    nlohmann_json::nlohmann_json
    glm::glm
    SDL3::SDL3
)
