# ModelPort Build Environment

## Project

Repository:

Nanxunx/CBHW

Branch:

feature/v47-effect-validator

Version:

v47.0-effect-validator-framework


# Compiler Environment

IDE:

Visual Studio 2022

MSVC:

17.14

Windows SDK:

10.0.26100

Platform:

Windows x64


# Build

Configure:

cmake -S . -B build-v46


Build Debug:

cmake --build build-v46 --config Debug


# Test

Run all tests:

ctest --test-dir build-v46 -C Debug


Run EffectValidator test:

ctest --test-dir build-v46 -C Debug -R effect_validator


Expected:

100% tests passed


# V47.0 Validation Record

Commit:

ea2d29b

Tag:

v47.0-effect-validator-framework


Completed:

- EffectValidator framework
- CMake integration
- EffectValidator unit test


# Design Decision

Effect validation is separated from conversion.

Current converters remain unchanged.

Reason:

Avoid breaking validated V46 Golden results.


# Development Notes

Do not commit:

- build-v46/
- validation-v46/
- MPQ files
- M2 files
- BLP files
- DBC files


Keep generated artifacts outside Git repository.