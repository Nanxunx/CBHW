# Turtle335Converter

Core research and conversion tooling for selected 3.3.5a -> Turtle/Classic 1.12 asset retroport work.

## V4.6 whole-M2 validation status

The current V4.6 whole-M2 branch has completed a real Windows VS2022 x64 Debug build and complete CTest run:

```text
30/30 tests passed
```

The next validation gate is selected real-model whole-output Golden regression.

From the repository root, after the local build has produced `build-v46\Debug\turtle335_convert_m2.exe`, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\modelport\Run_V46_SelectedGolden.ps1
```

Default paired corpus paths used by the current modelport research workflow:

```text
E:\335_FinalExtract_V5
E:\335to112_Converted_FinalExtract_V1
```

The runner reuses the V4.4 targeted selector when needed and packages selected source/sidecars, generated canonical v256 M2s, historical successful 1.12 targets, logs and hashes for semantic comparison.

See `docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-20_0335.md` for the latest exact project checkpoint.
