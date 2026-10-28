# Audio Module

Two kinds of audio, both behind `Platform`:

- **File-based**: `SoundEffect` / `Music` components, loaded through the assets module and played
  by the `SoundEffectPlayback` and `MusicPlayback` systems.
- **Procedural**: one synthesised stream that the platform pulls from a `SynthSource` on the
  audio thread.

## Music

`Music` is a streamed track (`path`, `loop`, `volume`). Setting it loads the file through the assets
module (`AssetType::Music`, backed by `Platform::LoadMusic`); `MusicPlayback` then starts it once and
refills its stream every frame, so it keeps playing while a scene is paused. Removing the component or
destroying the entity releases the track and stops it, so a scene owns its music by parenting the entity
to its root. The level JSON loader is `"music"` (`path`, `loop`, `volume`). A missing file or no audio
device leaves `handle` at -1 and the system does nothing.

```cpp
world.entity("Music").child_of(sceneRoot).set<engine::audio::Music>(
	{.path = engine::assets::ResolveAsset(world, "audio/mygame/music.ogg"), .loop = true, .volume = 0.5F});
```

## Procedural audio

```
SynthSource   — interface: Render(std::span<float> out) fills mono f32 at kSynthSampleRate (48 kHz)
SynthParam    — game→audio continuous control (std::atomic<float> target)
SynthTrigger  — game→audio one-shot (atomic counter; fires between renders coalesce into one)
dsp/          — raylib-free building blocks: SineOsc, WhiteNoise, Biquad (RBJ LP/HP/BP),
                AdEnvelope, OnePole (control smoother)
```

`Platform::StartSynth(source)` opens the stream and `StopSynth()` closes it. There is one stream,
so mix all voices inside one `SynthSource`. The source must outlive `StopSynth()`. Once that call
returns, `Render()` is guaranteed not to be running. See `game/synth_scene.cpp` for a worked
example, reachable from the sandbox title screen ("Synth Demo").

```cpp
class Radio final : public engine::audio::SynthSource {
public:
	engine::audio::SynthParam pitchHz{440.0F};
	engine::audio::SynthTrigger click;

	void Render(std::span<float> out) noexcept override {
		const float target = pitchHz.Target(); // read controls once per render
		if (click.Consume()) {
			envelope_.Trigger();
		}
		for (float& s : out) {
			s = osc_.Next(glide_.Next(target), engine::audio::kSynthSampleRate) * envelope_.Next();
		}
	}
	// ...
};
```

### Rules for `Render()`

It runs on the audio thread (desktop) while raylib's global audio lock is held, and on the main
thread between frames on web. Inside it:

- **Never** lock, allocate, log, or call raylib or Flecs. A raylib audio call self-deadlocks;
  a slow render also stalls every `PlaySound` on the game thread.
- Handle **any** frame count, including odd ones. Don't assume a block size.
- Read game input only through `SynthParam` / `SynthTrigger`, and smooth every
  continuous control with `OnePole`, or 60 Hz updates will zipper.
- Do setup math (`Biquad::LowPass`, `AdEnvelope::Configure`, `OnePole::Configure`) at init or on
  the game thread, not per sample.

The backend wraps each render in a Tracy zone, `Platform::SynthCallback`, in profiled builds.

### Web

Browsers start Web Audio suspended. miniaudio resumes it on `click`/`touchend`, and
`tools/wasm_audio_unlock.js` (`--pre-js`) adds `keydown`/`pointerdown`. While audio is suspended,
`Render()` isn't called, so don't assume time advances.

The stream uses the ScriptProcessorNode path, with about 43 ms blocks and playback that can
drop out on long frames. AudioWorklets are deferred: they need cross-origin isolation (which
GitHub Pages can't provide) and either a patched raylib port or our own miniaudio device. See
`docs/research/procedural-audio.md`.
