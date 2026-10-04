# Noise Module

Procedural noise functions for 2D/3D terrain, height, and texture generation.
Uses named generator factories so algorithms are interchangeable at runtime by name without shared mutable state.

## Architecture

```
INoiseGenerator     — abstract algorithm interface (SetSeed, Sample2D, Sample3D)
ICellularGenerator  — INoiseGenerator extension for Worley/cellular: SampleCellular2D/3D returns {F1, F2}
NoiseRegistry       — Flecs singleton: pure registry of named generator factories (no active concept)
NoiseConfig         — Flecs singleton: active algorithm name + seed for scripting globals
NoiseSampler        — move-only owning generator handle with FBM helpers
FractalNoiseParams  — POD FBM params; scripting value type
NoiseModule         — Flecs module; registers singletons and scripting globals
BuiltinNoiseModule  — registers the built-in generator factories
```

## Available Generators

| Name             | Class            | Description                                                 |
|------------------|------------------|-------------------------------------------------------------|
| `"perlin"`       | PerlinGenerator  | Improved Perlin gradient noise (Ken Perlin, 2002)           |
| `"simplex"`      | SimplexGenerator | Simplex noise (Ken Perlin, 2001) — smoother, more isotropic |
| `"value"`        | ValueGenerator   | Smoothly interpolated random corner values. Blockier look.  |
| `"worley_f1"`    | WorkleyGenerator | Worley F1 — nearest feature-point distance                  |
| `"worley_f2"`    | WorkleyGenerator | Worley F2 — second nearest distance                         |
| `"worley_f2_f1"` | WorkleyGenerator | Worley F2-F1 — cell border highlights                       |

## C++ Usage

```cpp
const auto& registry = world.get<engine::noise::NoiseRegistry>();

auto sampler = registry.GetSampler("perlin", seed);
float h = sampler.Sample2D({x, y});

engine::noise::FractalNoiseParams params{.octaves = 6, .persistence = 0.5f, .lacunarity = 2.0f};
float fbm = sampler.FractalNoise2D({x, y}, params);

// Heightmap: sweeps FractalNoise2D, contrast-stretches to [0, 1]
auto map = engine::noise::GenerateHeightMap(sampler, 256, 256, {0, 0}, 0.025f, params);
```

## Scripting (AngelScript)

```angelscript
// Object-oriented sampler handle (NoiseSampler@)
NoiseSampler@ ns = GetNoiseSampler();
FractalNoiseParams p(6, 0.5, 2.0);
float h = ns.FractalNoise2D(vec2(x, y), p);

// Free-function shortcuts
float h = Noise2D(vec2(x, y));
float v = Noise3D(vec3(x, y, z));

// Fractal (multi-octave)
FractalNoiseParams p(6, 0.5, 2.0);
float h = FractalNoise2D(vec2(x, y), p);
float v = FractalNoise3D(vec3(x, y, z), p);

// Control
Noise_SetSeed(42);
Noise_SetAlgorithm("worley_f1");
```

## Cellular / Worley Extension

`ICellularGenerator` extends `INoiseGenerator` with `SampleCellular2D/3D` returning
`{F1, F2}`. The three named Worley variants expose the distance combinations via the base
`Sample2D/Sample3D` interface so all scripting and C++ sampling code works unchanged.

## Adding a New Generator

1. Implement `INoiseGenerator` (or `ICellularGenerator` for multi-distance generators).
2. Register a factory in `BuiltinNoiseModule`:

```cpp
  registry.Register("my_algo", [] { return std::make_unique<MyGenerator>(); });
```

3. From C++: `registry.GetSampler("my_algo", seed)`.
   From script: `Noise_SetAlgorithm("my_algo")`.
