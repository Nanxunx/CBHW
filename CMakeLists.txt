cmake_minimum_required(VERSION 3.16)

project(Turtle335ConverterCore LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


enable_testing()


include(cmake/Sources.cmake)


target_include_directories(turtle335_core PUBLIC include)


target_compile_options(turtle335_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)


include(cmake/Tools.cmake)

include(cmake/Tests.cmake)
add_executable(
    turtle335_validate_m2
    tools_validate_m2.cpp
)

target_link_libraries(
    turtle335_validate_m2
    PRIVATE turtle335_core
)

