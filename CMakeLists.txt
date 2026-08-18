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
    src/adt/PlacementWriter.cpp
    src/adt/TerrainWriter.cpp
    src/adt/NormalizedAdt.cpp
    src/adt/WotlkAdtReader.cpp
    src/adt/AdtWriter.cpp
    src/adt/AdtValidator.cpp
    src/adt/WdtWriter.cpp
)

target_include_directories(turtle335_core PUBLIC include)

target_compile_options(turtle335_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

add_executable(turtle335_generate_fixture_a tools/generate_fixture_a.cpp)
target_link_libraries(turtle335_generate_fixture_a PRIVATE turtle335_core)

add_executable(turtle335_generate_fixture_wdt tools/generate_fixture_wdt.cpp)
target_link_libraries(turtle335_generate_fixture_wdt PRIVATE turtle335_core)

add_executable(turtle335_validate_adt tools/validate_adt.cpp)
target_link_libraries(turtle335_validate_adt PRIVATE turtle335_core)

add_executable(turtle335_validate_wdt tools/validate_wdt.cpp)
target_link_libraries(turtle335_validate_wdt PRIVATE turtle335_core)

add_executable(turtle335_core_tests tests/test_adt_core.cpp)
target_link_libraries(turtle335_core_tests PRIVATE turtle335_core)

add_executable(turtle335_mcnk_tests tests/test_mcnk_writer.cpp)
target_link_libraries(turtle335_mcnk_tests PRIVATE turtle335_core)

add_executable(turtle335_full_adt_tests tests/test_full_adt_writer.cpp)
target_link_libraries(turtle335_full_adt_tests PRIVATE turtle335_core)

add_executable(turtle335_terrain_tests tests/test_terrain_writer.cpp)
target_link_libraries(turtle335_terrain_tests PRIVATE turtle335_core)

add_executable(turtle335_fixture_a_tests tests/test_fixture_a.cpp)
target_link_libraries(turtle335_fixture_a_tests PRIVATE turtle335_core)

add_executable(turtle335_wdt_tests tests/test_wdt_writer.cpp)
target_link_libraries(turtle335_wdt_tests PRIVATE turtle335_core)

add_executable(turtle335_normalized_adt_tests tests/test_normalized_adt.cpp)
target_link_libraries(turtle335_normalized_adt_tests PRIVATE turtle335_core)

add_executable(turtle335_wotlk_adt_reader_tests tests/test_wotlk_adt_reader.cpp)
target_link_libraries(turtle335_wotlk_adt_reader_tests PRIVATE turtle335_core)

enable_testing()
add_test(NAME turtle335_core_tests COMMAND turtle335_core_tests)
add_test(NAME turtle335_mcnk_tests COMMAND turtle335_mcnk_tests)
add_test(NAME turtle335_full_adt_tests COMMAND turtle335_full_adt_tests)
add_test(NAME turtle335_terrain_tests COMMAND turtle335_terrain_tests)
add_test(NAME turtle335_fixture_a_tests COMMAND turtle335_fixture_a_tests)
add_test(NAME turtle335_wdt_tests COMMAND turtle335_wdt_tests)
add_test(NAME turtle335_normalized_adt_tests COMMAND turtle335_normalized_adt_tests)
add_test(NAME turtle335_wotlk_adt_reader_tests COMMAND turtle335_wotlk_adt_reader_tests)
