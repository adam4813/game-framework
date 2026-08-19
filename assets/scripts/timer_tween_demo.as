// timer_tween_demo.as — Demonstrates Timer and Tween components from the engine::timer module.
//
// Attached as a child script to "TimerDemoSphere" (a sphere to the left of the falling cube).
//
// Demonstrates:
//   - self.AddTimer()   — creates a Timer component on the host entity; TimerAdvance advances it.
//   - t.expired         — one-shot flag raised when the countdown reaches zero; consume by clearing.
//   - t.repeat          — auto-rearms by adding duration back to remaining (no script polling needed).
//   - self.AddTween()   — creates a Tween; TweenAdvance interpolates value from `from` to `to`.
//   - tw.value          — current eased scalar (0→1); used here to lerp the sphere's material colour.
//   - tw.done           — raised when elapsed >= duration; reset elapsed+done to replay.
//
// The Timer fires every PULSE_INTERVAL seconds and restarts the Tween, which then flashes the
// sphere from its base colour to white over FLASH_DURATION seconds.

const float PULSE_INTERVAL = 3.0f;
const float FLASH_DURATION = 0.8f;

// Base colour (Boost orange) and flash peak (white) in float [0..255].
const float BASE_R = 220.0f, BASE_G = 150.0f, BASE_B = 60.0f;
const float PEAK_R = 255.0f, PEAK_G = 255.0f, PEAK_B = 255.0f;

void OnInit(Entity self) {
	// AddTimer / AddTween ensure the component exists on the host entity and return a mutable handle.
	Timer@ t = self.AddTimer();
	t.duration  = PULSE_INTERVAL;
	t.remaining = PULSE_INTERVAL;
	t.repeat    = true;   // TimerAdvance re-arms automatically

	Tween@ tw = self.AddTween();
	tw.from     = 1.0f;  // start at peak (white flash)
	tw.to       = 0.0f;  // fade back to base colour
	tw.duration = FLASH_DURATION;

	Print("[TimerTweenDemo] Ready — sphere pulses every " + PULSE_INTERVAL + "s");
}

void Tick(Entity self, float dt) {
	// Consume the expired flag and restart the tween.
	Timer@ t = self.GetTimer();
	if (t.expired) {
		t.expired = false;
		Tween@ tw = self.GetTween();
		tw.elapsed = 0.0f;
		tw.done    = false;
		Print("[TimerTweenDemo] Pulse! Restarting colour flash (remaining=" + t.remaining + ")");
	}

	// Apply tween.value (0=base colour, 1=white peak) to the host Material each frame.
	float v = self.GetTween().value;
	Material@ mat = self.GetMaterial();
	mat.color.r = uint8(BASE_R + (PEAK_R - BASE_R) * v);
	mat.color.g = uint8(BASE_G + (PEAK_G - BASE_G) * v);
	mat.color.b = uint8(BASE_B + (PEAK_B - BASE_B) * v);
}

void OnDestroy(Entity self) {}
