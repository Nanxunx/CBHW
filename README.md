# Turtle335Converter


Convert World of Warcraft **3.3.5a build12340** assets into **Vanilla 1.12.x / Turtle WoW compatible resources**.


The project focuses on cross-version asset conversion:


- ADT terrain conversion
- M2 model conversion
- WMO conversion
- Particle system conversion
- Binary structure validation
- Semantic compatibility checking


The goal is not simple binary copying, but a validated conversion pipeline between different WoW client generations.


---


# Project Status


## Current Development


### V4.7 Particle Pipeline


Completed:


- ✅ ParticleProbe
- ✅ ParticleReader
- ✅ ParticleData model
- ✅ ParticleValidator
- ✅ GitHub Actions CI integration




Current next steps:


- ParticleWriter
- Classic M2 writer integration
- Particle round-trip validation




---


# Build Status


## Supported Environment


- Windows 10 / Windows 11
- Visual Studio 2022
- MSVC v143
- CMake




## Continuous Integration


GitHub Actions automatically verifies:


- Windows MSVC build
- Debug configuration
- Release configuration
- Automated CTest validation




Local validation:


```powershell
cmake -S . -B build-v46


cmake --build build-v46 --config Debug


ctest --test-dir build-v46 -C Debug --output-on-failure
Architecture Overview
WoW 3.3.5a build12340 assets


        |
        v


Turtle335Converter


        |
        +----------------+
        |                |
        v                v


ADT Pipeline        M2 Pipeline


        |                |


        v                v


Vanilla/Turtle compatible output



The converter uses:

binary readers
normalized intermediate structures
validators
writers
regression tests
ADT Conversion Pipeline

Current ADT pipeline:

WoW 3.3.5a ADT
    |
    + LiquidType.dbc
    + optional WDT
          |
          v


WotLK structural readers


          |
          v


Semantic normalization


          |
          v


Vanilla/Turtle ADT writer


          |
          v


Self validation



Implemented ADT areas:

MVER / MHDR / MCIN / MCNK reconstruction
MTEX
MMDX / MMID
MWMO / MWID
MDDF / MODF
MCRF
MCVT terrain normalization
MCNR normal conversion
MCLY layer conversion
MCAL alpha normalization
MH2O to legacy MCLQ conversion
MCSH shadow normalization
WotLK WDT parsing
LiquidType.dbc resolution

The converter intentionally reports unsupported semantics as:

LOSS
BLOCKER
RISK

instead of silently copying incompatible binary data.

ADT Tools
Single ADT Probe
turtle335_probe_adt <source.adt> [--wdt <source.wdt>]

The probe analyzes:

MCAL encoding
MH2O liquid information
MCSH
MCCV
MCSE
holes
placement risks

Exit codes:

0 = clean


2 = blocker exists


3 = risk/loss exists
Batch Map Probe
turtle335_probe_map <adt-directory> [--recursive]

Features:

scans multiple ADT files
aggregates issue codes
detects conversion risks
identifies clean candidates
M2 Conversion Pipeline

Current M2 development:

WotLK M2


    |


    v


Reader


    |


    v


Normalized Data


    |


    v


Validator


    |


    v


Writer


    |


    v


Classic M2 Output



V4.6 milestone:

Completed:

whole M2 validation framework
writer validation framework
animation related checks
ribbon/effect validation
golden reference workflow

Current gate:

Real model regression testing.

Compilation is no longer the primary limitation.

Particle System

Particle conversion is being developed as part of the M2 pipeline.

Current modules:

include/turtle335/m2/particle/


    ParticleProbe.h
    ParticleReader.h
    ParticleData.h
    ParticleValidator.h




src/m2/particle/


    ParticleProbe.cpp
    ParticleReader.cpp
    ParticleValidator.cpp




tests/m2/particle/


    Particle tests

Design goals:

preserve particle emitter structure
validate binary offsets
detect invalid references
support future writer implementation
Verification

Current verification includes:

CTest


34 / 34 tests passed

CI validates:

Configure


        |


Build


        |


CTest


        |


Result
Research Documentation

Detailed reverse engineering notes are stored in:

docs/research/

Important documents:

Current project memory
M2 Retroport Specification
ADT Retroport Specification
WMO Retroport Specification
Golden Reference analysis
Binary structure analysis

These documents record:

file format research
conversion decisions
compatibility analysis
implementation checkpoints
Target Pipeline
WoW 3.3.5a assets


        |


        v


Turtle335Converter


        |


        v


Vanilla/Turtle compatible:


    M2
    WMO
    ADT
    BLP


        |


        v


Tortoise extractor


        |


        v


maps / vmaps / mmaps


        |


        v


Turtle WoW 1.18.1



The final compatibility authority is the target client loader and runtime behavior.

Development Workflow

Recommended workflow:

Create feature branch


        |


Modify code


        |


Build locally


        |


Run tests


        |


Commit


        |


Push


        |


GitHub Actions validation



Commit style:

feat:
new feature




fix:
bug fix




docs:
documentation




ci:
continuous integration




chore:
maintenance
Repository Layout
Turtle335Converter


├── .github
│   └── workflows
│
├── docs
│   └── research
│
├── include
│   └── turtle335
│
├── src
│
├── tests
│
├── tools
│
├── CMakeLists.txt
└── README.md


Roadmap
V4.7

Particle pipeline

ParticleReader ✅
ParticleValidator ✅
ParticleWriter ⏳
Classic M2 integration ⏳
Future
Complete WotLK → Vanilla conversion pipeline
More binary validators
More automated regression testing
Expanded CI coverage