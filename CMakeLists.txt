cmake_minimum_required(VERSION 3.15)
project(biblioteca_ej1 CXX)

# --- C++20 ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Rutas de include ---
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- Librería fuente (lógica) ---
add_library(biblioteca_lib
  src/library.cpp
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(biblioteca_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Ejecutable demo/CLI ---
add_executable(biblioteca
  src/main.cpp
)
target_link_libraries(biblioteca PRIVATE biblioteca_lib)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(biblioteca PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tests con Catch2 v3 ---
find_package(Catch2 3 REQUIRED)

add_executable(tests
  tests/test_lib.cpp
)
target_link_libraries(tests PRIVATE biblioteca_lib Catch2::Catch2WithMain)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
add_test(NAME biblioteca_tests COMMAND tests)
