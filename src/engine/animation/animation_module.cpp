#include "animation_module.hpp"

#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "animation_components.hpp"
#include "animation_playback.hpp"
#include "engine/assets/assets.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"

namespace engine::animation {

AnimationModule::AnimationModule(const flecs::world& world) {
	// === Reflection ===
	world.component<SkeletalAnimationSet>().member<std::string>("path").member<int>("handle");
	world.component<AnimationPlayer>()
		.member<std::string>("clip")
		.member<int>("clip_index")
		.member<float>("clip_duration")
		.member<float>("time")
		.member<float>("speed")
		.member<bool>("loop")
		.member<bool>("playing");
	world.component<UnboundAnimationWarned>();

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<SkeletalAnimationSet>());
	scripting::RegisterComponentForScripts(world, world.component<AnimationPlayer>());

	// === Level loaders ===
	level::RegisterComponentLoader(world, "skeletal_animation_set", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<SkeletalAnimationSet>({.path = assets::ResolveAsset(e.world(), j.value("path", std::string{}))});
	});
	level::RegisterComponentLoader(world, "animation", [](const flecs::entity e, const nlohmann::json& j) {
		AnimationPlayer p{};
		p.clip = j.value("clip", p.clip);
		p.speed = j.value("speed", p.speed);
		p.loop = j.value("loop", p.loop);
		p.playing = j.value("playing", p.playing);
		e.set<AnimationPlayer>(p);
	});

	// === Path-asset resolver ===
	assets::RegisterPathAsset<SkeletalAnimationSet>(world, assets::AssetType::Animation);

	// === SYSTEM: BindSkeletalClips ===
	// Resolves a player's clip name against its entity's loaded SkeletalAnimationSet once, so it doesn't depend
	// on the order the asset observer and the player's OnSet fire in. An empty name is the first clip; an
	// unknown name warns once (the player is bound right after) and falls back to the first clip.
	world.system<AnimationPlayer, const SkeletalAnimationSet>("BindSkeletalClips")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t i, AnimationPlayer& player, const SkeletalAnimationSet& set) {
			if (player.clip_index >= 0 || set.handle < 0) return;
			auto& platform = *it.world().get<platform::PlatformRef>().ptr;
			const int count = platform.SkeletalClipCount(set.handle);
			const auto name = std::string{it.entity(i).name().c_str()};
			if (count <= 0) {
				// Bind to an empty clip so the player stops looking unbound; nothing is drawn for it.
				spdlog::warn("[Animation] '{}' has a SkeletalAnimationSet with no clips", name);
				player.clip_index = 0;
				return;
			}
			int index = 0;
			for (int c = 0; c < count && !player.clip.empty(); ++c) {
				if (platform.SkeletalClipName(set.handle, c) == player.clip) {
					index = c;
					break;
				}
				if (c == count - 1) {
					spdlog::warn(
						"[Animation] '{}' has no clip '{}'; playing '{}'",
						name,
						player.clip,
						platform.SkeletalClipName(set.handle, 0)
					);
				}
			}
			player.clip_index = index;
			player.clip_duration = platform.SkeletalClipDuration(set.handle, index);
		})
		.add<scene::GameScene>();

	// === SYSTEM: AdvanceAnimation ===
	// Headless playback: knows nothing of meshes, so any Animation Kind's bound players advance here.
	world.system<AnimationPlayer>("AdvanceAnimation")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t i, AnimationPlayer& player) {
			if (AdvanceAnimation(player, it.delta_time())) return;
			const auto entity = it.entity(i);
			if (entity.has<UnboundAnimationWarned>()) return;
			spdlog::warn(
				"[Animation] '{}' has an AnimationPlayer no Animation Kind has bound; it will not play",
				std::string{entity.name().c_str()}
			);
			entity.add<UnboundAnimationWarned>();
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	spdlog::info("[AnimationModule] Registered animation systems with Flecs");
}

} // namespace engine::animation
