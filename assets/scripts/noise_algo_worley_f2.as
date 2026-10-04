void OnInit(Entity self) { SetOnClick(self, @OnWorleyF2Clicked); }
void OnWorleyF2Clicked(Entity button) {
    Noise_SetAlgorithm("worley_f2");
    NoiseSampler@ ns = GetNoiseSampler();
    FractalNoiseParams p(4, 0.5, 2.0);
    Print("[NoiseDemo] Switched to Worley F2. FBM(centre)=" + ns.FractalNoise2D(vec2(0.5,0.5),p));
}
