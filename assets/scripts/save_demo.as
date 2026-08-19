// save_demo.as — Drives the JSON save system from a script.
//
// The save schema (registered in src/game/meta_save_example.cpp) captures both abstract
// meta-progression (tokens, score, upgrades) AND actual visible scene state:
//   scene.cube.transform  — FallingCube position/rotation (teleports the physics body on load)
//   scene.cube.velocity   — linear + angular velocity at save time
//   scene.sun.color       — current hue from the sun_cycle.as animation
//
// Controls (while the Game scene is active):
//   F5  -> SaveGame("quicksave")  — snapshot to Platform storage (file / localStorage)
//   F9  -> LoadGame("quicksave")  — restore: scene reloads, then saved state applied
//   1   -> AddParlorTokens(5) + AddScorePoints(100)
//   2   -> UnlockUpgrade("magnet")
//   3   -> CompleteRun()
//   P   -> Print current meta-progression
//
// Try: let the cube fall a bit, F5 to save. Watch the sun cycle colour. F9 — the cube jumps
// back to its saved position with saved velocity, and the sun snaps back to its saved hue.
// The UI panel (src/game/save_ui.cpp) manages named slots visually.

void PrintProgress(string tag) {
	MetaProgress p = GetMetaProgress();
	Print("[SaveDemo] " + tag +
		": tokens=" + p.parlorTokens +
		" score=" + p.scorePoints +
		" runs=" + p.runsCompleted +
		" upgrades=" + UpgradeCount());
}

void OnInit(Entity self) {
	Print("[SaveDemo] Keys: 1=+tokens/score  2=unlock magnet  3=complete run  F5=save  F9=load  P=print");
	PrintProgress("initial");
}

void Tick(Entity self, float dt) {
	InputState input = GetInputState();

	if (input.WasKeyPressed(Key_Key1)) {
		AddParlorTokens(5);
		AddScorePoints(100);
		PrintProgress("earned");
	}
	if (input.WasKeyPressed(Key_Key2)) {
		UnlockUpgrade("magnet");
		PrintProgress("unlocked");
	}
	if (input.WasKeyPressed(Key_Key3)) {
		CompleteRun();
		PrintProgress("run complete");
	}
	if (input.WasKeyPressed(Key_F5)) {
		SaveGame("quicksave");
		PrintProgress("after save");
	}
	if (input.WasKeyPressed(Key_F9)) {
		LoadGame("quicksave");
		PrintProgress("after load");
	}
	if (input.WasKeyPressed(Key_P)) {
		PrintProgress("current");
	}
}
