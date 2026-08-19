// sun_cycle.as — Cycles the parent SunLight's DirectionalLight colour through the hue wheel.
//
// Demonstrates a script reading and writing a *render* component every tick:
//   - self.GetDirectionalLight()   reads the host (parent) entity's DirectionalLight
//   - light.color.r/g/b            mutates the nested Rgba value (uint8 fields)
//   - self.SetDirectionalLight(l)  writes it back so the render module picks it up next frame
//
// The script entity is a child of "SunLight", so the generic get/set resolve to the parent,
// exactly like cube_jump.as resolves to its parent cube.

const float CYCLE_SPEED = 0.15f; // full rainbow every ~6.7 seconds

float g_hue = 0.0f; // module-global state persists across ticks

// Hue (0..1) to RGB with full saturation and value. Returns each channel in 0..255.
void HueToRgb(float h, float &out r, float &out g, float &out b) {
	float h6 = h * 6.0f;
	int i = int(h6);
	float f = h6 - float(i);
	float q = 1.0f - f;

	if (i == 0) { r = 1.0f; g = f;    b = 0.0f; }
	else if (i == 1) { r = q;    g = 1.0f; b = 0.0f; }
	else if (i == 2) { r = 0.0f; g = 1.0f; b = f;    }
	else if (i == 3) { r = 0.0f; g = q;    b = 1.0f; }
	else if (i == 4) { r = f;    g = 0.0f; b = 1.0f; }
	else { r = 1.0f; g = 0.0f; b = q; }

	r *= 255.0f;
	g *= 255.0f;
	b *= 255.0f;
}

void OnInit(Entity self) {
    // GetParent() navigates to the host entity this script is attached to.
    // GetName() and GetId() demonstrate the Entity identity API.
    // HasComponent() shows a defensive presence-check pattern.
    Entity parent = self.GetParent();
    Print("[SunCycleScript] Host: '" + parent.GetName() + "' (id=" + parent.GetId() + ")");
    Print("[SunCycleScript] HasComponent DirectionalLight: " + parent.HasComponent("DirectionalLight"));
}

void Tick(Entity self, float dt) {
	g_hue += dt * CYCLE_SPEED;
	while (g_hue >= 1.0f) {
		g_hue -= 1.0f;
	}

	float r, g, b;
	HueToRgb(g_hue, r, g, b);

	DirectionalLight@ light = self.GetDirectionalLight();
	light.color.r = uint8(r);
	light.color.g = uint8(g);
	light.color.b = uint8(b);
}

void OnDestroy(Entity self) {}
