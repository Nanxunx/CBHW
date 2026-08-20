# Turtle WMO Shader Render Analysis

状态：**real-client binary correlation**

目标：确定 Turtle WoW 1.18.1 / Vanilla-derived WMO renderer 实际支持哪些 MOMT shader 数字及 Texture0/Texture1 路径，用于约束 3.3.5a shader downgrade。

---

## 1. Material record baseline

Turtle root WMO material table pointer:

```text
CMapObj + 0x1D8
```

MOBA material index is expanded with:

```text
materialIndex << 6
```

therefore:

```text
MOMT stride = 0x40 = 64 bytes
```

Resolved runtime texture handles are stored in the material record at:

```text
MOMT + 0x38 = Texture0 handle
MOMT + 0x3C = Texture1 handle
```

---

## 2. Single-texture render path

Function around:

```text
0x6B4F10
```

At approximately:

```text
0x6B4F82
```

it obtains the MOMT record and binds:

```text
[MOMT + 0x38]
```

This path clearly uses Texture0.

It also reads:

```text
MOMT + 0x00 flags
MOMT + 0x04 shader
MOMT + 0x08 blend mode / related state
```

and contains a special runtime branch for:

```text
shader == 1
```

around `0x6B5077`.

---

## 3. Dual-texture render path

Function starts approximately:

```text
0x6B5190
```

It iterates MOBA records and resolves the material in the same way.

At:

```text
0x6B5259
```

it loads/binds Texture0 using:

```text
MOMT + 0x38
```

Then at:

```text
0x6B527B
```

it reads:

```text
MOMT + 0x3C
```

and if non-zero resolves/binds Texture1 as well.

Therefore:

> **Turtle/Vanilla-derived client has a real two-texture WMO material rendering path.**

This is not an inference from file format documentation; it is visible in the uploaded Turtle client machine code.

---

## 4. Shader-specific branches in Turtle

Inside the dual-texture render function, the client reads:

```text
MOMT + 0x04 = shader
```

and contains explicit branches for multiple legacy shader IDs.

### 4.1 Shader 3 and 5

Around:

```text
0x6B54E1
```

logic effectively distinguishes:

```text
shader == 3
shader == 5
other
```

with different render-state setup.

Thus shader 3 and shader 5 are genuinely recognized by the Turtle renderer and participate in the legacy dual-texture path.

### 4.2 Shader 1 and 2

Later around:

```text
0x6B55AB
```

logic distinguishes:

```text
shader == 1
shader == 2
other
```

and selects different graphics state constants.

Combined with the single-texture path's shader-1 branch, this proves the client does not treat MOMT.shader as an ignored value.

---

## 5. Comparison with 3.3.5a

3.3.5a build 12340 material loader has already been binary-confirmed to classify:

```text
single texture: 0,1,2,4
dual texture:   3,5,6
```

The Turtle renderer independently contains explicit numeric handling for:

```text
1,2,3,5
```

and a true Texture0 + Texture1 render path.

This creates strong binary correlation that the pre-WotLK shader family `0..5` was retained across the two client generations.

### Current confidence

```text
WotLK 0 -> Turtle 0 : strong inference / likely identity
WotLK 1 -> Turtle 1 : strong binary correlation
WotLK 2 -> Turtle 2 : strong binary correlation
WotLK 3 -> Turtle 3 : strong binary correlation; dual-texture path exists
WotLK 4 -> Turtle 4 : strong inference; also WotLK native fallback target
WotLK 5 -> Turtle 5 : strong binary correlation; dual-texture path exists
WotLK 6 -> no equivalent legacy extended-vertex path confirmed
```

Important: this does **not yet prove the exact fixed-function lighting equation for every ID is byte-for-byte identical**. Final identity mapping must still be regression-tested in Turtle.

---

## 6. Shader 6 boundary

3.3.5a shader 6 has already been confirmed to trigger:

```text
second MOCV + second MOTV
```

and 0x30-byte runtime group vertices instead of the ordinary 0x24-byte path.

The Turtle WMO loader/render paths inspected so far do not have the corresponding WotLK second-set group feature path.

Therefore shader 6 is the first hard compatibility boundary.

Recommended target policy:

```text
shader 0..5 -> provisional same-ID legacy mapping + validation
shader 6    -> fold/bake/downgrade
```

---

## 7. Why shader 4 is the preferred shader-6 fallback after bake

The 3.3.5a material loader itself performs this fallback:

```text
shader 3/5/6
+ missing Texture1
        ->
shader = 4
```

Therefore, after an offline bake reduces shader 6 to one final texture, `shader 4` is the most evidence-backed first fallback candidate.

Recommended first-pass rule:

```cpp
if (sourceShader == 6)
{
    if (CanLosslesslyCollapseSecondSet(material, group))
    {
        // collapse and select appropriate legacy material
    }
    else
    {
        BakeShader6ToSingleTexture();
        targetShader = 4;
        diagnostics.materialWasBaked = true;
    }
}
```

Confidence of `6 -> baked 4`: **strong inference supported by 3.3.5a native fallback behavior; requires Turtle visual regression tests.**

---

## 8. First-pass mapper

Do not implement as a blind numeric cast without diagnostics. Use:

```cpp
enum class WmoShaderConversionKind
{
    IdentityCandidate,
    RequiresBake,
    Unsupported
};

struct WmoShaderMapping
{
    uint32_t sourceShader;
    uint32_t targetShader;
    WmoShaderConversionKind kind;
    bool requiresTexture1;
    bool requiresSecondUv;
    bool requiresSecondVertexColor;
};
```

Initial table:

```text
0 -> 0   IdentityCandidate   Texture0
1 -> 1   IdentityCandidate   Texture0
2 -> 2   IdentityCandidate   Texture0
3 -> 3   IdentityCandidate   Texture0 + Texture1
4 -> 4   IdentityCandidate   Texture0
5 -> 5   IdentityCandidate   Texture0 + Texture1
6 -> 4   RequiresBake        Texture0 + Texture1 + MOTV2/MOCV2
```

The table must carry confidence/diagnostic metadata so future binary or runtime findings can change a single rule without rewriting the parser.

---

## 9. Validation plan

For shaders 0..5, create or obtain one representative WMO material for each ID and compare:

1. 3.3.5a source render screenshot/reference.
2. converted WMO in Turtle 1.18.1.
3. texture-stage behavior.
4. alpha/blend behavior.
5. fog/unlit/two-sided flags.
6. vertex color response.

For shader 6, additionally compare baked output against the WotLK source using a pixel-difference or visual reference test.

Until that corpus exists, same-ID mapping for 0..5 is classified as **provisional but strongly supported**, not mathematically proven.
