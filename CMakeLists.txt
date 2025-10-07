# --- Tests con Catch2 v3 (FetchContent) ---
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4   # o la versión estable que prefieras
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests
  tests/test_lib.cpp
)
target_link_libraries(tests PRIVATE biblioteca_lib Catch2::Catch2WithMain)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
add_test(NAME biblioteca_tests COMMAND tests)
