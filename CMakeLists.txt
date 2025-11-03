cmake_minimum_required(VERSION 3.20)
project(IdleOptimizerVS LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
  DOWNLOAD_NO_EXTRACT TRUE
  DOWNLOAD_NAME json.hpp
)
FetchContent_Populate(nlohmann_json)

add_executable(IdleOptimizer
    src/main.cpp
    src/config_loader.hpp
)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${nlohmann_json_SOURCE_DIR})
target_link_libraries(IdleOptimizer PRIVATE nlohmann_json)

set(ASSET_FILES
    ${CMAKE_SOURCE_DIR}/assets/config.json
    ${CMAKE_SOURCE_DIR}/assets/optimizer_gui.html
    ${CMAKE_SOURCE_DIR}/assets/OpenGUI.bat
    ${CMAKE_SOURCE_DIR}/assets/RunWithLog.bat
)
add_custom_command(TARGET IdleOptimizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSET_FILES} $<TARGET_FILE_DIR:IdleOptimizer>
    COMMENT "Copying assets to output directory"
)
