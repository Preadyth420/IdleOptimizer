cmake_minimum_required(VERSION 3.25)
project(IdleOptimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/Zc:__cplusplus /std:c++20)
endif()

# Optimization flags per compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -fno-exceptions")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob3 /GL /GS- /Gy /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    string(REPLACE "/profile" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "/PROFILE" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/config_loader.hpp
  third_party/nlohmann/json.hpp
)

install(TARGETS ${PROJECT_NAME} DESTINATION .)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/optimizer_gui.html" DESTINATION .)