// particle_toggle.as — Demonstrates the engine::particles module from a script.
//
// Attached as a child of FallingCube (which has a ParticleEmitter component added in C++).
//
// Demonstrates:
//   - self.GetParticleEmitter()  — mutable handle into the host entity's ParticleEmitter.
//   - emitter.emitting           — bool flag: toggling it starts/stops particle spawning.
//   - self.AddCooldown()         — creates a Cooldown on the host entity; CooldownAdvance drains it.
//   - cd.Ready()                 — returns true when remaining <= 0 (method registered by timer module).
//   - cd.remaining               — set it to cd.duration to restart the cooldown.
//
// Press P to toggle the particle burst on/off. A 0.5 s Cooldown prevents accidental rapid toggling.

const float TOGGLE_COOLDOWN = 0.5f;

void OnInit(Entity self) {
	Cooldown@ cd = self.AddCooldown();
	cd.duration  = TOGGLE_COOLDOWN;
	cd.remaining = 0.0f; // start ready

	Print("[ParticleToggle] Attached — press P to toggle particle emitter on/off");
}

void Tick(Entity self, float dt) {
	InputState input = GetInputState();
	if (!input.WasKeyPressed(Key_P)) {
		return;
	}

	Cooldown@ cd = self.GetCooldown();
	if (!cd.Ready()) {
		return; // still in cooldown — swallow the keypress silently
	}

	// Toggle the emitter and re-arm the cooldown.
	ParticleEmitter@ em = self.GetParticleEmitter();
	em.emitting  = !em.emitting;
	cd.remaining = cd.duration;

	Print("[ParticleToggle] Particles " + (em.emitting ? "ON" : "OFF"));
}

void OnDestroy(Entity self) {}
