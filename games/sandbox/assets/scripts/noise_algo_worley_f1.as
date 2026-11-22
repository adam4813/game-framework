void OnInit(Entity self) { SetOnClick(self, @OnWorleyF1Clicked); }
void OnWorleyF1Clicked(Entity button) {
    Noise_SetAlgorithm("worley_f1");
    NoiseSampler@ ns = GetNoiseSampler();
    FractalNoiseParams p(4, 0.5, 2.0);
    Print("[NoiseDemo] Switched to Worley F1. FBM(centre)=" + ns.FractalNoise2D(vec2(0.5,0.5),p));
}
