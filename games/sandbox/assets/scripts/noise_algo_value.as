// Noise demo — Value algorithm button.
// Attached as a child of the Value button entity; binds its onClick handler so that clicking
// the button switches the active noise algorithm to "value" and prints a diagnostic sample.
// Value noise uses smoothly interpolated random corner values — simpler and blockier than
// gradient noise, useful for cloud or terrain textures with a distinct chunky style.

void OnInit(Entity self) {
    SetOnClick(self, @OnValueClicked);
}

void OnValueClicked(Entity button) {
    Noise_SetAlgorithm("value");

    NoiseSampler@ ns = GetNoiseSampler();
    FractalNoiseParams params(4, 0.5, 2.0);
    float center = ns.FractalNoise2D(vec2(0.5, 0.5), params);
    Print("[NoiseDemo] Switched to Value noise. FBM(centre)=" + center);
}
