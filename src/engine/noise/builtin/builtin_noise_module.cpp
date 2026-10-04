#include "builtin_noise_module.hpp"

#include <memory>

#include <spdlog/spdlog.h>

#include "../noise_registry.hpp"
#include "perlin_strategy.hpp"
#include "simplex_strategy.hpp"
#include "value_generator.hpp"
#include "worley_generator.hpp"

namespace engine::noise::builtin {

BuiltinNoiseModule::BuiltinNoiseModule(const flecs::world& world) {
	auto& registry = world.get_mut<NoiseRegistry>();
	registry.Register("perlin", [] { return std::make_unique<PerlinGenerator>(); });
	registry.Register("simplex", [] { return std::make_unique<SimplexGenerator>(); });
	registry.Register("value", [] { return std::make_unique<ValueGenerator>(); });
	registry.Register("worley_f1", [] { return std::make_unique<WorkleyGenerator>(WorkleyMode::F1); });
	registry.Register("worley_f2", [] { return std::make_unique<WorkleyGenerator>(WorkleyMode::F2); });
	registry.Register("worley_f2_f1", [] { return std::make_unique<WorkleyGenerator>(WorkleyMode::F2MinusF1); });
	spdlog::info("[BuiltinNoiseModule] Registered generators: perlin, simplex, value, worley_f1/f2/f2_f1");
}

} // namespace engine::noise::builtin
