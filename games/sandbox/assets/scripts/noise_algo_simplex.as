// Noise demo — Simplex algorithm button.
// Attached as a child of the Simplex button entity; binds its onClick handler so that clicking
// the button switches the active noise algorithm to "simplex" and prints a diagnostic sample.
// Demonstrates the script-button binding pattern alongside the Perlin button.

void OnInit(Entity self) {
    SetOnClick(self, @OnSimplexClicked);
}

void OnSimplexClicked(Entity button) {
    Noise_SetAlgorithm("simplex");

    NoiseSampler@ ns = GetNoiseSampler();
    FractalNoiseParams params(4, 0.5, 2.0);
    float center = ns.FractalNoise2D(vec2(0.5, 0.5), params);
    float corner = ns.FractalNoise2D(vec2(0.0, 0.0), params);
    Print("[NoiseDemo] Switched to Simplex. FBM(centre)=" + center + ", FBM(corner)=" + corner);
}
