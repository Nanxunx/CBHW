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
    src/adt/WotlkAdtProbe.cpp
    src/adt/WotlkMapProbe.cpp
    src/adt/WotlkTerrainNormalizer.cpp
    src/adt/WotlkMcshNormalizer.cpp
    src/adt/WotlkAdtNormalizer.cpp
    src/adt/AdtWriter.cpp
    src/adt/AdtValidator.cpp
    src/adt/WdtWriter.cpp
    src/adt/WotlkWdtReader.cpp
    src/dbc/LiquidTypeDbc.cpp
    src/m2/WotlkM2Reader.cpp
    src/m2/AnimationMetadata.cpp
    src/m2/LegacyTrack.cpp
    src/m2/ClassicM2Header.cpp
    src/m2/RibbonWriter.cpp
    src/m2/ParticleWriter.cpp
    src/m2/SkinViewWriter.cpp
    src/m2/ClassicM2Validator.cpp
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

add_executable(turtle335_convert_adt tools/convert_wotlk_adt.cpp)
target_link_libraries(turtle335_convert_adt PRIVATE turtle335_core)

add_executable(turtle335_probe_adt tools/probe_wotlk_adt.cpp)
target_link_libraries(turtle335_probe_adt PRIVATE turtle335_core)

add_executable(turtle335_probe_map tools/probe_wotlk_map.cpp)
target_link_libraries(turtle335_probe_map PRIVATE turtle335_core)

add_executable(turtle335_probe_m2 tools/probe_wotlk_m2.cpp)
target_link_libraries(turtle335_probe_m2 PRIVATE turtle335_core)

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

add_executable(turtle335_wotlk_adt_probe_tests tests/test_wotlk_adt_probe.cpp)
target_link_libraries(turtle335_wotlk_adt_probe_tests PRIVATE turtle335_core)

add_executable(turtle335_wotlk_map_probe_tests tests/test_wotlk_map_probe.cpp)
target_link_libraries(turtle335_wotlk_map_probe_tests PRIVATE turtle335_core)

add_executable(turtle335_wotlk_terrain_normalizer_tests tests/test_wotlk_terrain_normalizer.cpp)
target_link_libraries(turtle335_wotlk_terrain_normalizer_tests PRIVATE turtle335_core)

add_executable(turtle335_wotlk_mcsh_normalizer_tests tests/test_wotlk_mcsh_normalizer.cpp)
target_link_libraries(turtle335_wotlk_mcsh_normalizer_tests PRIVATE turtle335_core)

add_executable(turtle335_wotlk_adt_normalizer_tests tests/test_wotlk_adt_normalizer.cpp)
target_link_libraries(turtle335_wotlk_adt_normalizer_tests PRIVATE turtle335_core)

add_executable(turtle335_liquid_type_dbc_tests tests/test_liquid_type_dbc.cpp)
target_link_libraries(turtle335_liquid_type_dbc_tests PRIVATE turtle335_core)

add_executable(turtle335_wotlk_wdt_reader_tests tests/test_wotlk_wdt_reader.cpp)
target_link_libraries(turtle335_wotlk_wdt_reader_tests PRIVATE turtle335_core)

add_executable(turtle335_raw_wotlk_pipeline_tests tests/test_raw_wotlk_pipeline.cpp)
target_link_libraries(turtle335_raw_wotlk_pipeline_tests PRIVATE turtle335_core)

add_executable(turtle335_m2_core_tests tests/test_m2_core.cpp)
target_link_libraries(turtle335_m2_core_tests PRIVATE turtle335_core)

add_executable(turtle335_classic_m2_header_tests tests/test_classic_m2_header.cpp)
target_link_libraries(turtle335_classic_m2_header_tests PRIVATE turtle335_core)

add_executable(turtle335_ribbon_writer_tests tests/test_ribbon_writer.cpp)
target_link_libraries(turtle335_ribbon_writer_tests PRIVATE turtle335_core)

add_executable(turtle335_particle_writer_tests tests/test_particle_writer.cpp)
target_link_libraries(turtle335_particle_writer_tests PRIVATE turtle335_core)

add_executable(turtle335_skin_view_writer_tests tests/test_skin_view_writer.cpp)
target_link_libraries(turtle335_skin_view_writer_tests PRIVATE turtle335_core)

add_executable(turtle335_classic_m2_validator_tests tests/test_classic_m2_validator.cpp)
target_link_libraries(turtle335_classic_m2_validator_tests PRIVATE turtle335_core)

enable_testing()
add_test(NAME turtle335_core_tests COMMAND turtle335_core_tests)
add_test(NAME turtle335_mcnk_tests COMMAND turtle335_mcnk_tests)
add_test(NAME turtle335_full_adt_tests COMMAND turtle335_full_adt_tests)
add_test(NAME turtle335_terrain_tests COMMAND turtle335_terrain_tests)
add_test(NAME turtle335_fixture_a_tests COMMAND turtle335_fixture_a_tests)
add_test(NAME turtle335_wdt_tests COMMAND turtle335_wdt_tests)
add_test(NAME turtle335_normalized_adt_tests COMMAND turtle335_normalized_adt_tests)
add_test(NAME turtle335_wotlk_adt_reader_tests COMMAND turtle335_wotlk_adt_reader_tests)
add_test(NAME turtle335_wotlk_adt_probe_tests COMMAND turtle335_wotlk_adt_probe_tests)
add_test(NAME turtle335_wotlk_map_probe_tests COMMAND turtle335_wotlk_map_probe_tests)
add_test(NAME turtle335_wotlk_terrain_normalizer_tests COMMAND turtle335_wotlk_terrain_normalizer_tests)
add_test(NAME turtle335_wotlk_mcsh_normalizer_tests COMMAND turtle335_wotlk_mcsh_normalizer_tests)
add_test(NAME turtle335_wotlk_adt_normalizer_tests COMMAND turtle335_wotlk_adt_normalizer_tests)
add_test(NAME turtle335_liquid_type_dbc_tests COMMAND turtle335_liquid_type_dbc_tests)
add_test(NAME turtle335_wotlk_wdt_reader_tests COMMAND turtle335_wotlk_wdt_reader_tests)
add_test(NAME turtle335_raw_wotlk_pipeline_tests COMMAND turtle335_raw_wotlk_pipeline_tests)
add_test(NAME turtle335_m2_core_tests COMMAND turtle335_m2_core_tests)
add_test(NAME turtle335_classic_m2_header_tests COMMAND turtle335_classic_m2_header_tests)
add_test(NAME turtle335_ribbon_writer_tests COMMAND turtle335_ribbon_writer_tests)
add_test(NAME turtle335_particle_writer_tests COMMAND turtle335_particle_writer_tests)
add_test(NAME turtle335_skin_view_writer_tests COMMAND turtle335_skin_view_writer_tests)
add_test(NAME turtle335_classic_m2_validator_tests COMMAND turtle335_classic_m2_validator_tests)
