# Turtle335Converter

Convert WoW 3.3.5a assets to Vanilla 1.12.x / Turtle WoW 1.18.1 compatible resources.

## Research checkpoint

The current reverse-engineering and conversion design is documented under `docs/research/`:

- [Research Checkpoint](docs/research/RESEARCH_CHECKPOINT.md) — project-wide conclusions, evidence levels, architecture and current status.
- [M2 Retroport Spec](docs/research/M2_RETROPORT_SPEC.md) — MD20/M2 structure conversion, skins, animations, particles and validation.
- [WMO Retroport Spec](docs/research/WMO_RETROPORT_SPEC.md) — root/group WMO conversion, MOMT, MOPY, second UV/color sets and MLIQ.
- [ADT Retroport Spec](docs/research/ADT_RETROPORT_SPEC.md) — ADT/MCNK conversion, MH2O -> MCLQ, resource tables and placements.

## Target pipeline

```text
WoW 3.3.5a assets
        |
        v
Turtle335Converter
        |
        v
Vanilla/Turtle-compatible M2 / WMO / ADT / BLP
        |
        v
Tortoise extractor
        |
        v
maps / vmaps / mmaps
        |
        v
Turtle WoW 1.18.1
```

The target client loader and target Tortoise extractor are treated as the final compatibility authority. Community documentation is used as supporting evidence, not as a substitute for loader-level verification.
