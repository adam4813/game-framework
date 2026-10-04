#include "noise_module.hpp"

#include "noise_config.hpp"
#include "noise_registry.hpp"
#include "noise_sampler.hpp"

#include <flecs.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "engine/scripting/scripting.hpp"

// TODO(scripting): AngelScript supports real namespaces via SetDefaultNamespace / SetNamespace.
// When the scripting backend is extended to support namespace registration, move all noise
// globals (Noise2D, FractalNoise2D, Noise_SetAlgorithm, etc.) into a 'noise' namespace so
// scripts can write `noise::Sample2D(pos)` instead of free globals.
// See: https://www.angelcode.com/angelscript/sdk/docs/manual/doc_adv_namespace.html

namespace engine::noise {

namespace {

struct ScriptNoiseSamplerCache {
	NoiseSampler sampler;
	std::string algorithm;
	int seed{-1}; // -1 forces rebuild on first use
};

NoiseSampler* GetOrRefreshScriptSampler(const flecs::world& world) {
	const auto& config = world.get<NoiseConfig>();
	auto& cache = world.get_mut<ScriptNoiseSamplerCache>();
	// Rebuild only when the active algorithm or seed has changed, amortising the permutation
	// table construction across all free-global calls within the same algorithm+seed.
	if (!cache.sampler.IsValid() || cache.algorithm != config.active_algorithm || cache.seed != config.seed) {
		cache.sampler = world.get<NoiseRegistry>().GetSampler(config.active_algorithm, config.seed);
		cache.algorithm = config.active_algorithm;
		cache.seed = config.seed;
	}
	return cache.sampler.IsValid() ? &cache.sampler : nullptr;
}

} // namespace

NoiseModule::NoiseModule(const flecs::world& world) {
	// Default-construct the singletons — backend modules fill in generator factories.
	if (!world.has<NoiseRegistry>()) {
		world.emplace<NoiseRegistry>();
	}
	if (!world.has<NoiseConfig>()) {
		world.set<NoiseConfig>({});
	}
	if (!world.has<ScriptNoiseSamplerCache>()) {
		world.emplace<ScriptNoiseSamplerCache>();
	}

	// === Reflection ===
	world.component<FractalNoiseParams>("FractalNoiseParams")
		.member<int>("octaves")
		.member<float>("persistence")
		.member<float>("lacunarity");
	world.component<NoiseSampler>("NoiseSampler");

	// === Scripting: FractalNoiseParams value type + constructor ===
	scripting::RegisterValueTypeForScripts(world, world.component<FractalNoiseParams>());
	scripting::RegisterObjectConstructorForScripts(
		world,
		world.component<FractalNoiseParams>(),
		"int octaves, float persistence, float lacunarity",
		[](scripting::ScriptCallContext& ctx) {
			new (ctx.GetObject()) FractalNoiseParams{
				.octaves = ctx.GetArgInt(0),
				.persistence = ctx.GetArgFloat(1),
				.lacunarity = ctx.GetArgFloat(2)
			};
		},
		nullptr
	);

	// === Scripting: NoiseSampler handle type + methods ===
	scripting::RegisterCallbackViewTypeForScripts(world, world.component<NoiseSampler>(), {}, {});
	scripting::RegisterComponentMethodForScripts<&NoiseSampler::Sample2D>(world, "Sample2D");
	scripting::RegisterComponentMethodForScripts<&NoiseSampler::Sample3D>(world, "Sample3D");
	scripting::RegisterComponentMethodForScripts<&NoiseSampler::FractalNoise2D>(world, "FractalNoise2D");
	scripting::RegisterComponentMethodForScripts<&NoiseSampler::FractalNoise3D>(world, "FractalNoise3D");

	// === Scripting global functions ===

	// NoiseSampler@ GetNoiseSampler()
	scripting::RegisterGlobalFunctionForScripts(
		world,
		scripting::ScriptMethodSignature{
			.name = "GetNoiseSampler",
			.return_type = scripting::ScriptValueType::MakeObjectHandle(world.component<NoiseSampler>()),
			.params = {},
		},
		[](scripting::ScriptCallContext& ctx, const flecs::world& w) {
			ctx.SetReturnObjectHandle(GetOrRefreshScriptSampler(w));
		}
	);

	// float Noise2D(const vec2 &in pos)
	scripting::RegisterGlobalFunctionForScripts(
		world,
		scripting::ScriptMethodSignature{
			.name = "Noise2D",
			.return_type = scripting::ScriptValueType::MakeFloat(),
			.params = {{
				.type = scripting::ScriptValueType::MakeObject(world.component<glm::vec2>()),
				.by_reference = true,
				.name = "pos",
			}},
		},
		[](scripting::ScriptCallContext& ctx, const flecs::world& w) {
			const auto& pos = *static_cast<const glm::vec2*>(ctx.GetArgObject(0));
			const NoiseSampler* sp = GetOrRefreshScriptSampler(w);
			if (!sp) return;
			ctx.SetReturnFloat(sp->Sample2D(pos));
		}
	);

	// float Noise3D(const vec3 &in pos)
	scripting::RegisterGlobalFunctionForScripts(
		world,
		scripting::ScriptMethodSignature{
			.name = "Noise3D",
			.return_type = scripting::ScriptValueType::MakeFloat(),
			.params = {{
				.type = scripting::ScriptValueType::MakeObject(world.component<glm::vec3>()),
				.by_reference = true,
				.name = "pos",
			}},
		},
		[](scripting::ScriptCallContext& ctx, const flecs::world& w) {
			const auto& pos = *static_cast<const glm::vec3*>(ctx.GetArgObject(0));
			const NoiseSampler* sp = GetOrRefreshScriptSampler(w);
			if (!sp) return;
			ctx.SetReturnFloat(sp->Sample3D(pos));
		}
	);

	// float FractalNoise2D(const vec2 &in pos, const FractalNoiseParams &in params)
	scripting::RegisterGlobalFunctionForScripts(
		world,
		scripting::ScriptMethodSignature{
			.name = "FractalNoise2D",
			.return_type = scripting::ScriptValueType::MakeFloat(),
			.params =
				{
					{.type = scripting::ScriptValueType::MakeObject(world.component<glm::vec2>()),
					 .by_reference = true,
					 .name = "pos"},
					{.type = scripting::ScriptValueType::MakeObject(world.component<FractalNoiseParams>()),
					 .by_reference = true,
					 .name = "params"},
				},
		},
		[](scripting::ScriptCallContext& ctx, const flecs::world& w) {
			const auto& pos = *static_cast<const glm::vec2*>(ctx.GetArgObject(0));
			const auto& params = *static_cast<const FractalNoiseParams*>(ctx.GetArgObject(1));
			const NoiseSampler* sp = GetOrRefreshScriptSampler(w);
			if (!sp) return;
			ctx.SetReturnFloat(sp->FractalNoise2D(pos, params));
		}
	);

	// float FractalNoise3D(const vec3 &in pos, const FractalNoiseParams &in params)
	scripting::RegisterGlobalFunctionForScripts(
		world,
		scripting::ScriptMethodSignature{
			.name = "FractalNoise3D",
			.return_type = scripting::ScriptValueType::MakeFloat(),
			.params =
				{
					{.type = scripting::ScriptValueType::MakeObject(world.component<glm::vec3>()),
					 .by_reference = true,
					 .name = "pos"},
					{.type = scripting::ScriptValueType::MakeObject(world.component<FractalNoiseParams>()),
					 .by_reference = true,
					 .name = "params"},
				},
		},
		[](scripting::ScriptCallContext& ctx, const flecs::world& w) {
			const auto& pos = *static_cast<const glm::vec3*>(ctx.GetArgObject(0));
			const auto& params = *static_cast<const FractalNoiseParams*>(ctx.GetArgObject(1));
			const NoiseSampler* sp = GetOrRefreshScriptSampler(w);
			if (!sp) return;
			ctx.SetReturnFloat(sp->FractalNoise3D(pos, params));
		}
	);

	// Noise control globals — Noise_ prefix groups them semantically until the scripting
	// backend gains namespace support (see TODO at top of file).
	// void Noise_SetSeed(int seed)
	scripting::RegisterGlobalFunctionForScripts(
		world,
		scripting::ScriptMethodSignature{
			.name = "Noise_SetSeed",
			.return_type = scripting::ScriptValueType::MakeVoid(),
			.params = {{.type = scripting::ScriptValueType::MakeInt(), .by_reference = false, .name = "seed"}},
		},
		[](const scripting::ScriptCallContext& ctx, const flecs::world& w) {
			w.get_mut<NoiseConfig>().seed = ctx.GetArgInt(0);
		}
	);

	// void Noise_SetAlgorithm(string name)
	scripting::RegisterGlobalFunctionForScripts(
		world,
		scripting::ScriptMethodSignature{
			.name = "Noise_SetAlgorithm",
			.return_type = scripting::ScriptValueType::MakeVoid(),
			.params = {{.type = scripting::ScriptValueType::MakeString(), .by_reference = false, .name = "name"}},
		},
		[](const scripting::ScriptCallContext& ctx, const flecs::world& w) {
			w.get_mut<NoiseConfig>().active_algorithm = ctx.GetArgString(0);
		}
	);

	spdlog::info("[NoiseModule] Initialized");
}

} // namespace engine::noise
