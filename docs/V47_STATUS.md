# ModelPort V47 Status

## Current Branch

feature/v47-effect-validator


## Version

v47.0-effect-validator-framework


## Commit

ea2d29b


## Completed

### M2

- WotLK M2 reader
- Classic M2 writer
- Whole M2 conversion pipeline


### Validation

- Classic M2 validator
- Golden regression framework
- EffectValidator framework


## Design Decision

Effect validation is separated from conversion.

Current converters remain unchanged.

Reason:

Avoid breaking validated V46 Golden results.


## Tests

Passed:

ctest --test-dir build-v46 -C Debug -R effect_validator


## Next Phase

V47.1 Particle Semantic Validator

Targets:

- ParticleEmitter
- Texture lookup
- Alpha
- Blend mode
- Animation references