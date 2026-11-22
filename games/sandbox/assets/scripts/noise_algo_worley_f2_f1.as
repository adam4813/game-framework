void OnInit(Entity self) { SetOnClick(self, @OnWorleyF2F1Clicked); }
void OnWorleyF2F1Clicked(Entity button) {
    Noise_SetAlgorithm("worley_f2_f1");
    NoiseSampler@ ns = GetNoiseSampler();
    FractalNoiseParams p(4, 0.5, 2.0);
    Print("[NoiseDemo] Switched to Worley F2-F1. FBM(centre)=" + ns.FractalNoise2D(vec2(0.5,0.5),p));
}
