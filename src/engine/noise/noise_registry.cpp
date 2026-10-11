#include "noise_registry.hpp"

#include <spdlog/spdlog.h>

#include "noise_generator.hpp"
#include "noise_sampler.hpp"

namespace engine::noise {

void NoiseRegistry::Register(std::string name, std::function<std::unique_ptr<INoiseGenerator>()> factory) {
	if (!factory) {
		spdlog::warn("[NoiseRegistry] Ignoring registration for '{}' with null factory", name);
		return;
	}

	factories_.insert_or_assign(std::move(name), std::move(factory));
}

bool NoiseRegistry::Has(const std::string_view name) const { return factories_.contains(std::string{name}); }

NoiseSampler NoiseRegistry::GetSampler(const std::string_view name, const int seed) const {
	const auto it = factories_.find(std::string{name});
	if (it == factories_.end()) {
		spdlog::warn("[NoiseRegistry] Unknown generator '{}'", name);
		return NoiseSampler{};
	}

	auto generator = it->second();
	if (!generator) {
		spdlog::warn("[NoiseRegistry] Generator factory '{}' returned null", name);
		return NoiseSampler{};
	}

	generator->SetSeed(seed);
	return NoiseSampler(std::move(generator));
}

} // namespace engine::noise
