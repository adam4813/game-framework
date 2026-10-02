#pragma once

#include <flecs.h>

namespace engine::ui {

/// UI module for Flecs ECS integration.
/// Registers the 2D UI components (reflection + scripting), the UIDrawList singleton, and the
/// interaction/layout/build/render systems (see ui_systems.hpp). Systems are untagged (they run in
/// every scene pipeline) and are not Pausable, so UI keeps working while the game is paused.
///
/// The module is split across focused headers, all re-exported by ui.hpp:
///   - ui_components.hpp — plain-data components (UIRect, Panel, Label, Button, ...).
///   - ui_draw_list.hpp  — deferred-render command buffer (UIDrawCommand, UIDrawList).
///   - ui_events.hpp     — callback/behaviour components (OnClick, ...).
///   - ui_factories.hpp  — Create* convenience factories.
///   - ui_systems.hpp    — RegisterUISystems (interaction/layout/build/render).
class UIModule {
public:
	explicit UIModule(const flecs::world& world);
};

} // namespace engine::ui
