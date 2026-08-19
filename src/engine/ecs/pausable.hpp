#pragma once

namespace engine::ecs {

// Tag applied to a *system* whose work advances the game simulation (physics
// stepping, gameplay scripts, etc.). It is a zero-size marker used purely for
// pipeline filtering — it carries no data.
//
// A scene can build a second "paused" pipeline that excludes every system
// carrying this tag (`.without<engine::ecs::Pausable>()`), so pausing is a data
// question: adding `.add<Pausable>()` to a system is all that is required to make
// it stop while the game is paused. Systems that must keep running when paused
// (input polling, rendering, audio, menu/UI logic) simply omit the tag.
struct Pausable {};

} // namespace engine::ecs
