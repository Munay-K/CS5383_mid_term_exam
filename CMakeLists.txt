cmake_minimum_required(VERSION 3.15)
project(biblioteca_ej1 CXX)

# --- C++20 ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Librería fuente (lógica) ---
add_library(biblioteca_lib
  src/library.cpp
)

# 👉 Expone la carpeta de headers a quien enlace con la lib
target_include_directories(biblioteca_lib
  PUBLIC ${CMAKE_SOURCE_DIR}/include
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(biblioteca_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Ejecutable demo/CLI ---
add_executable(biblioteca
  src/main.cpp
)
target_link_libraries(biblioteca PRIVATE biblioteca_lib)

# (no es estrictamente necesario por el PUBLIC de arriba, pero ayuda)
target_include_directories(biblioteca PRIVATE ${CMAKE_SOURCE_DIR}/include)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(biblioteca PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tests con Catch2 v3 (FetchContent) ---
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests
  tests/test_lib.cpp
)
target_link_libraries(tests PRIVATE biblioteca_lib Catch2::Catch2WithMain)
# Asegura que tests vea los headers
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
add_test(NAME biblioteca_tests COMMAND tests)

