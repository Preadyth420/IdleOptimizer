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
# --- Stage GUI + scripts next to the built EXE, and ensure config.json exists ---
set(RUNTIME_DIR "$<TARGET_FILE_DIR:IdleOptimizer>")

# Copy GUI + example config + runner BAT to the EXE folder
set(CONFIG_EXAMPLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/config.14days.example.json")
if (NOT EXISTS "${CONFIG_EXAMPLE_SOURCE}")
  set(CONFIG_EXAMPLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/config.example.json")
endif()

add_custom_command(TARGET IdleOptimizer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo "Staging GUI + config next to EXE..."
  # If you want the whole assets folder, use copy_directory; otherwise copy the one file:
  # COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${RUNTIME_DIR}/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/optimizer_gui.html" "${RUNTIME_DIR}/optimizer_gui.html"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CONFIG_EXAMPLE_SOURCE}" "${RUNTIME_DIR}/config.example.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/RunWithLog.bat" "${RUNTIME_DIR}/RunWithLog.bat"

  # On Windows, if config.json is missing, create it from the example (no weird output)
  COMMAND ${CMAKE_COMMAND} -E chdir "${RUNTIME_DIR}" cmd /c "if not exist config.json copy /Y config.example.json config.json >nul 2>&1"
)

target_include_directories(IdleOptimizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(IdleOptimizer PRIVATE nlohmann_json::nlohmann_json)

if (MSVC)
  target_compile_options(IdleOptimizer PRIVATE /W3 /EHsc)
else()
  target_compile_options(IdleOptimizer PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# -------- Copy runtime assets into the build output next to the .exe --------
set(GUI_HTML         ${CMAKE_CURRENT_SOURCE_DIR}/assets/optimizer_gui.html)
set(OPEN_GUI_BAT     ${CMAKE_CURRENT_SOURCE_DIR}/OpenGUI.bat)

add_custom_command(TARGET IdleOptimizer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:IdleOptimizer>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GUI_HTML}"       "$<TARGET_FILE_DIR:IdleOptimizer>/assets/optimizer_gui.html"
  # Don’t overwrite a user’s config.json — ship an example next to the exe
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CONFIG_EXAMPLE_SOURCE}" "$<TARGET_FILE_DIR:IdleOptimizer>/config.example.json"
  # (Optional) drop the helper script next to the exe
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPEN_GUI_BAT}"   "$<TARGET_FILE_DIR:IdleOptimizer>/OpenGUI.bat"
)
