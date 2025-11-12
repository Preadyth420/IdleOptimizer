cmake_minimum_required(VERSION 3.20)
project(IdleOptimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(IdleOptimizer
  src/main.cpp
  src/config_loader.hpp
)
target_include_directories(IdleOptimizer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

add_custom_command(TARGET IdleOptimizer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:IdleOptimizer>/third_party"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../third_party" "$<TARGET_FILE_DIR:IdleOptimizer>/third_party"
)

if (MSVC)
  target_compile_options(IdleOptimizer PRIVATE /W3 /EHsc)
else()
  target_compile_options(IdleOptimizer PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()