// Noise demo — Perlin algorithm button.
// Attached as a child of the Perlin button entity; binds its onClick handler so that clicking
// the button switches the active noise algorithm to "perlin".
//
// Also adds a repeating timer to the button entity: every ROAM_INTERVAL seconds it derives a
// new seed from the noise itself (self-referential feedback!) and calls Noise_SetSeed, which
// causes NoiseScene::Tick to rebuild the visualisation texture with a fresh pattern.
//
// Demonstrates:
//   - Script-button binding (SetOnClick).
//   - Timer component: AddTimer / MutTimer, repeat, expired.
//   - GetNoiseSampler(): obtaining an OO sampling handle (NoiseSampler@).
//   - Noise_SetSeed(): modifying the active noise state from a script.

const float ROAM_INTERVAL = 4.0f;

int g_seed_counter = 0;

void OnInit(Entity self) {
    SetOnClick(self, @OnPerlinClicked);

    // Repeating timer that roams through noise patterns automatically.
    Timer@ t = self.AddTimer();
    t.duration  = ROAM_INTERVAL;
    t.remaining = ROAM_INTERVAL;
    t.repeat    = true;

    Print("[NoiseDemo] Perlin button ready. Auto-roaming every " + ROAM_INTERVAL + "s");
}

void Tick(Entity self, float dt) {
    Timer@ t = self.MutTimer();
    if (!t.expired) return;
    t.expired = false;

    // Sample the current noise at a slowly-drifting position to derive a new seed.
    // Self-referential: the noise decides the next noise pattern.
    NoiseSampler@ ns = GetNoiseSampler();
    FractalNoiseParams params(4, 0.5, 2.0);
    float probe = ns.FractalNoise2D(
        vec2(float(g_seed_counter) * 0.37f, float(g_seed_counter) * 0.73f),
        params
    );

    // Map [-1, 1] → integer seed [0, 9999].
    int new_seed = int((probe * 0.5f + 0.5f) * 9999.0f);
    Noise_SetSeed(new_seed);
    g_seed_counter++;

    Print("[NoiseDemo] Auto-roam tick " + g_seed_counter +
          " — new seed: " + new_seed +
          ", probe FBM: " + probe);
}

void OnPerlinClicked(Entity button) {
    Noise_SetAlgorithm("perlin");

    NoiseSampler@ ns = GetNoiseSampler();
    FractalNoiseParams params(4, 0.5, 2.0);
    float center = ns.FractalNoise2D(vec2(0.5, 0.5), params);
    float corner = ns.FractalNoise2D(vec2(0.0, 0.0), params);
    Print("[NoiseDemo] Switched to Perlin. FBM(centre)=" + center + ", FBM(corner)=" + corner);
}

void OnDestroy(Entity self) {}
