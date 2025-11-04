cmake_minimum_required(VERSION 3.20)
project(IdleOptimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(IdleOptimizer
  src/main.cpp
  src/config_loader.hpp
)
target_include_directories(IdleOptimizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(IdleOptimizer PRIVATE nlohmann_json::nlohmann_json)

if (MSVC)
  target_compile_options(IdleOptimizer PRIVATE /W3 /EHsc)
else()
  target_compile_options(IdleOptimizer PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()