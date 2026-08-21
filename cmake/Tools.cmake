add_executable(turtle335_generate_fixture_a
    tools/generate_fixture_a.cpp
)

target_link_libraries(
    turtle335_generate_fixture_a
    PRIVATE turtle335_core
)


add_executable(turtle335_generate_fixture_wdt
    tools/generate_fixture_wdt.cpp
)

target_link_libraries(
    turtle335_generate_fixture_wdt
    PRIVATE turtle335_core
)


add_executable(turtle335_validate_adt
    tools/validate_adt.cpp
)

target_link_libraries(
    turtle335_validate_adt
    PRIVATE turtle335_core
)


add_executable(turtle335_validate_wdt
    tools/validate_wdt.cpp
)

target_link_libraries(
    turtle335_validate_wdt
    PRIVATE turtle335_core
)


add_executable(turtle335_convert_adt
    tools/convert_wotlk_adt.cpp
)

target_link_libraries(
    turtle335_convert_adt
    PRIVATE turtle335_core
)


add_executable(turtle335_probe_adt
    tools/probe_wotlk_adt.cpp
)

target_link_libraries(
    turtle335_probe_adt
    PRIVATE turtle335_core
)


add_executable(turtle335_probe_map
    tools/probe_wotlk_map.cpp
)

target_link_libraries(
    turtle335_probe_map
    PRIVATE turtle335_core
)


add_executable(turtle335_probe_m2
    tools/probe_wotlk_m2.cpp
)

target_link_libraries(
    turtle335_probe_m2
    PRIVATE turtle335_core
)


add_executable(turtle335_convert_m2
    tools/convert_wotlk_m2.cpp
)

target_link_libraries(
    turtle335_convert_m2
    PRIVATE turtle335_core
)