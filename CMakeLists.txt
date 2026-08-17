cmake_minimum_required(VERSION 3.16)
project(Turtle335ConverterCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(turtle335_core
    src/adt/Mcal.cpp
    src/adt/LegacyLiquid.cpp
    src/adt/MclqWriter.cpp
    src/adt/Mh2oReader.cpp
    src/adt/McnkWriter.cpp
)

target_include_directories(turtle335_core PUBLIC include)

target_compile_options(turtle335_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

add_executable(turtle335_core_tests tests/test_adt_core.cpp)
target_link_libraries(turtle335_core_tests PRIVATE turtle335_core)

add_executable(turtle335_mcnk_tests tests/test_mcnk_writer.cpp)
target_link_libraries(turtle335_mcnk_tests PRIVATE turtle335_core)

enable_testing()
add_test(NAME turtle335_core_tests COMMAND turtle335_core_tests)
add_test(NAME turtle335_mcnk_tests COMMAND turtle335_mcnk_tests)
