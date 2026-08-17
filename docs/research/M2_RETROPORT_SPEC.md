# M2 Retroport Spec — 3.3.5a -> Vanilla/Turtle 1.18.1

状态：**Checkpoint-M2/WMO**

目标：把 WoW 3.3.5a 的 M2/MD20 模型结构转换成 Vanilla 1.12.x / Turtle WoW 1.18.1 可加载的旧式 MD20 结构。

---

## 1. 设计原则

M2 转换器不能是“改一个 version integer”的工具。

正确路线：

```text
3.3.5a M2
   -> parse all arrays
   -> normalize semantics
   -> convert version-divergent records
   -> rebuild offsets/counts
   -> rebuild skin/profile data
   -> write target MD20
   -> validate
```

尤其禁止：

- 只改 header version
- 直接截断文件尾部
- 删除不认识的 particle/ribbon 数据但保留旧 offset
- 复制 source offsets 到 target writer

---

# 2. 当前客户端证据

Turtle WoW 1.18.1 的 `WoW.exe` 已确认：

- 基于 Vanilla 1.12.1 build 5875 客户端路线
- 保留 `MD20`
- 仍使用旧式 M2 runtime loader
- 不是 1.14 Classic/CASC 模型系统

旧 Vanilla 客户端逆向资料中已经恢复出 M2 loader 对多种记录的 file/runtime stride，这些结果与当前项目逆向方向一致。

当前项目工作基线是：

```text
3.3.5a MD20/M2
      ->
Vanilla/Turtle-compatible MD20
```

精确目标版本号与所有接受范围仍以实际 loader 检查为准；不要仅根据社区 struct 名称推断。

---

# 3. Normalized M2

建议首先建立版本无关的语义模型：

```cpp
struct NormalizedM2
{
    std::string name;

    std::vector<uint32_t> globalSequences;
    std::vector<AnimationSequence> sequences;
    std::vector<int16_t> animationLookup;

    std::vector<Bone> bones;
    std::vector<int16_t> keyBoneLookup;

    std::vector<Vertex> vertices;

    std::vector<ColorTrack> colors;
    std::vector<Texture> textures;
    std::vector<TransparencyTrack> transparencies;
    std::vector<TextureTransform> textureTransforms;

    std::vector<RenderFlag> renderFlags;
    std::vector<uint16_t> boneLookup;
    std::vector<uint16_t> textureLookup;
    std::vector<uint16_t> textureUnitLookup;
    std::vector<uint16_t> transparencyLookup;
    std::vector<uint16_t> textureTransformLookup;

    std::vector<BoundingTriangle> boundingTriangles;
    std::vector<Vec3> boundingVertices;
    std::vector<Vec3> boundingNormals;

    std::vector<Attachment> attachments;
    std::vector<Event> events;
    std::vector<Light> lights;
    std::vector<Camera> cameras;
    std::vector<RibbonEmitter> ribbons;
    std::vector<ParticleEmitter> particles;

    std::vector<SkinProfile> skins;
};
```

目标 writer 只接受 `NormalizedM2`，不直接接触 source raw offsets。

---

# 4. 已恢复的旧客户端 loader 特征

Vanilla 客户端逆向中已经恢复出若干关键记录布局。当前把这些当作目标 writer 的重要约束。

## 4.1 Runtime sizing evidence

已知旧 loader 会根据 MD20 header 的 count，为不同 element 分配固定 runtime block，例如：

```text
global sequences
sequences
bones
views/geosets
colors
textures
texture transforms
attachments
events
lights
cameras
ribbons
particles
```

这意味着：

> 只要某个 target record stride 写错，即使 header 能通过，后续 runtime copy loop 仍可能错位、越界或崩溃。

---

# 5. Bone

Bone 转换至少需要保留：

```text
key bone id / index
flags
parent
submesh id / unknown legacy fields
translation track
rotation track
scale track
pivot
```

转换要求：

- parent index 必须落在目标 bone 数组范围
- track 内部 timestamp/value array 全部重建 offset
- interpolation / global sequence 语义必须映射
- quaternion 不能简单 memcpy，需确认源/目标压缩表示

Validator：

```text
parent == -1 OR parent < boneCount
all track arrays in range
globalSequence index valid
```

---

# 6. Animation tracks

M2 的大量结构都嵌套 `M2Track`/array-of-arrays。

转换器需要一个统一 track 转换层：

```cpp
template<class TValue>
struct NormalizedTrack
{
    uint16_t interpolation;
    int16_t globalSequence;
    std::vector<std::vector<uint32_t>> timestamps;
    std::vector<std::vector<TValue>> values;
};
```

Writer 根据 target format 把二维 array 重新布局。

必须检查：

- timestamp group count 与 value group count
- 每个 sub-array offset
- target element stride
- empty animation blocks

---

# 7. Vertices / skin profiles

这是 3.3.5 -> Vanilla M2 的关键难点。

模型主体与 `.skin`/view 数据必须作为一个整体转换。

Normalized vertex 至少保留：

```text
position
bone weights
bone indices
normal
UV
```

Normalized skin 至少保留：

```text
vertex lookup
triangle/index lookup
bone indices
submeshes
texture units / batches
```

### 核心原则

如果 target 客户端使用旧式 view/skin representation，不能只复制 3.3.5 外部 `.skin` 文件并改 M2 header。

必须建立：

```text
source M2 + source skin profiles
        ->
Normalized geometry/render graph
        ->
Vanilla-compatible view/skin representation
```

---

# 8. Texture / material

Texture record 的语义应归一化为：

```cpp
struct Texture
{
    uint32_t type;
    uint32_t flags;
    std::string filename;
};
```

转换器需要收集资源依赖：

```text
M2
  -> BLP texture paths
```

并统一处理：

- path case
- slash direction
- extension
- target MPQ path

Render flags / texture units 需要与 skin batch 一起转换。

---

# 9. Texture transform / transparency / color

这些结构都包含 animation tracks。

目标 writer 不能只复制外层 record；必须递归重写内部 track arrays。

推荐统一：

```text
ColorTrack
TransparencyTrack
TextureTransform
      ->
TrackSerializer<T>
```

---

# 10. Attachments / events / cameras / lights

## Attachment

检查：

- id
- bone
- position
- animate track

## Event

检查：

- event identifier
- data
- bone
- position
- timestamps

## Camera

旧客户端对 camera file layout 有固定读取逻辑。

转换要保留：

- type
- FOV
- near/far clip
- position track
- target track
- roll track
- base positions

## Light

转换要保留：

- type
- bone
- position
- ambient color/intensity
- diffuse color/intensity
- attenuation start/end
- visibility

任何一项内部 track 都需要重新序列化。

---

# 11. Ribbon emitters

Ribbon 是版本差异较大的区域。

旧客户端 runtime 会为 ribbon 建立独立对象，并读取：

- texture indices
- material indices
- color/alpha
- height/edge params
- lifetime/texture slot
- animation tracks

因此第一版不能简单：

```text
source ribbon record -> memcpy target
```

正确：

```text
SourceRibbon
   -> NormalizedRibbon
   -> TargetRibbonWriter
```

如果某些 3.3.5 字段目标版本不存在：

- 有等价表示 -> 映射
- 纯视觉增强且可安全丢弃 -> explicit downgrade + Warning
- 会改变 record alignment/核心逻辑 -> 必须重构

---

# 12. Particle emitters

Particle 是 M2 最危险的版本差异区之一。

旧客户端存在不同 particle emitter runtime subtype，文件记录会驱动：

- emitter type
- spawn geometry
- lifespan
- speed
- gravity
- emission rate
- area
- color/alpha gradients
- scale
- texture animation
- visibility

所以第一版目标不是“100%视觉一致”，而是：

1. 不崩溃。
2. 保持核心粒子语义。
3. 对无法映射的扩展字段明确降级。
4. 输出 diagnostics。

建议：

```cpp
struct ParticleDowngradeReport
{
    bool lossy;
    std::vector<std::string> droppedFeatures;
};
```

---

# 13. Offset rebuild

所有 M2 arrays 统一通过 allocator/writer 分配：

```cpp
struct ArrayRef
{
    uint32_t count;
    uint32_t offset;
};
```

建议 writer 两阶段：

```text
Phase 1
  build all target records in memory
  reserve array locations

Phase 2
  serialize nested arrays
  patch target offsets
```

或使用 relocatable writer：

```cpp
PatchHandle writer.writeArrayRefPlaceholder();
...
writer.patchArrayRef(handle, count, finalOffset);
```

---

# 14. M2 validator

至少检查：

## Header

- magic
- target version
- name range
- every array count/offset

## Geometry

- vertex count
- bone indices < boneCount
- skin vertex lookup < vertexCount
- triangle/index lookup valid

## Animation

- lookup indices valid
- track sub-array counts match
- timestamps/value ranges valid

## Materials

- texture lookup valid
- render flag lookup valid
- transparency/transform lookup valid

## Other

- attachment bone valid
- light bone valid
- event bone valid
- camera arrays valid
- ribbon/particle nested arrays valid

---

# 15. Diagnostics policy

每个转换模型输出一份 report：

```text
Input: Creature/Foo.m2
Target: Creature/Foo.m2

Source version: ...
Target version: ...
Vertices: ...
Bones: ...
Skins: ...
Textures: ...
Animations: ...

Warnings:
- particle field X downgraded
- ribbon feature Y dropped
- texture path normalized
```

批量转换时可生成 JSON：

```json
{
  "asset": "Creature/Foo.m2",
  "status": "converted-with-warnings",
  "warnings": []
}
```

---

# 16. 当前优先级

1. 把已逆向出的 Turtle target MD20 header/record stride 整理成正式 C++ structs。
2. 把 3.3.5 source header/record stride 同样冻结。
3. 先完成无粒子/无 ribbon 的静态模型转换。
4. 接入 skin/submesh/material。
5. 接入 animation/bone。
6. 最后处理 particle/ribbon 高风险结构。

建议第一批测试模型：

```text
A. 静态简单 M2
B. 有骨骼但无粒子
C. 有多动画
D. 有 ribbon
E. 有 particle
```

每一层通过后再增加复杂度。

---

# 17. Source references

- Penqle/tortoise-wow:
  - https://github.com/Penqle/tortoise-wow
- TrinityCore 3.3.5:
  - https://github.com/TrinityCore/TrinityCore/tree/3.3.5
- warcraft-rs M2 support:
  - https://github.com/wowemulation-dev/warcraft-rs
- Vanilla 1.12 client internals reverse engineering:
  - https://github.com/samwhosung/wow-1121-client-internals

最终 target layout 仍以 Turtle 1.18.1 实际 `WoW.exe` loader 为最终裁判。
