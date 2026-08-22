add_library(turtle335_core
    src/model/ModelPackage.cpp
    src/model/ModelScanner.cpp
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
    src/m2/ExternalAnim.cpp
    src/m2/QuaternionCodec.cpp
    src/m2/BoneWriter.cpp
    src/m2/AuxiliaryTrackWriters.cpp
    src/m2/CameraWriter.cpp
    src/m2/EventWriter.cpp
    src/m2/LightWriter.cpp
    src/m2/ClassicM2Header.cpp
    src/m2/RibbonWriter.cpp
    src/m2/ParticleWriter.cpp
    src/m2/SkinViewWriter.cpp
    src/m2/ClassicM2Validator.cpp
    src/m2/ClassicM2Writer.cpp

    src/m2/effect/EffectValidator.cpp

    src/m2/particle/ParticleValidator.cpp
    src/m2/particle/ParticleProbe.cpp
    src/m2/particle/ParticleReader.cpp

)
