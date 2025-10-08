cmake_minimum_required(VERSION 3.15)
project(biblioteca_ej1 CXX)

# --- C++20 ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Función para cobertura SOLO por target (GCC/Clang) ---
function(enable_coverage TARGET)
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${TARGET} PRIVATE -O0 -g --coverage)
    target_link_options(${TARGET}    PRIVATE --coverage)
  endif()
endfunction()

# --- Librería fuente (lógica) ---
add_library(biblioteca_lib
  src/library.cpp
)

# Headers públicos
target_include_directories(biblioteca_lib
  PUBLIC ${CMAKE_SOURCE_DIR}/include
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(biblioteca_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Habilita cobertura SOLO en tu código
enable_coverage(biblioteca_lib)

# --- Ejecutable demo/CLI ---
add_executable(biblioteca
  src/main.cpp
)
target_link_libraries(biblioteca PRIVATE biblioteca_lib)
target_include_directories(biblioteca PRIVATE ${CMAKE_SOURCE_DIR}/include)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(biblioteca PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tests con Catch2 (FetchContent) ---
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
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Cobertura SOLO en tests (no en deps)
enable_coverage(tests)

# --- CTest ---
include(CTest)
add_test(NAME biblioteca_tests COMMAND tests)
