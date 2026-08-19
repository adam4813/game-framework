// cube_jump.as — Makes the parent (host) entity jump on Space or a left-mouse click.
//
// Demonstrates the scripting interface:
//   - self.GetWorldTransform()       returns a WorldTransform@ handle into ECS storage
//   - GetInputState()                reads the InputState world singleton (value type)
//   - input.WasKeyPressed(key)       native method on the singleton value
//   - self.AddPhysicsImpulse()       ensures the component exists, returns a PhysicsImpulse@
//   - self.GetAlbedoMap()            Get texture path at runtime to toggle from initial to empty (T key)
//   - self.GetSoundEffect().Fire()     modifies the SoundEffect in-place via its handle

const float JUMP_FORCE = 5.0f;
const float MAX_Y_TO_JUMP = 1.2f; // only jump when near the ground

// Module-global: stores the original texture path so we can restore it after toggling off.
string g_texturePath = "";

void OnInit(Entity self) {
    // Prove that the string member of SoundEffect is accessible from script.
    Print("[OnInit] SoundEffect.path = '" + self.GetSoundEffect().path + "'");
    // Cache the cube's initial texture path for the T-key toggle.
    g_texturePath = self.GetAlbedoMap().path;
    Print("[OnInit] AlbedoMap.path  = '" + g_texturePath + "'");
}

void Tick(Entity self, float dt) {
    WorldTransform@ wt   = self.GetWorldTransform();
    InputState      input = GetInputState();

    // Jump on Space or left-click.
    bool jump = input.WasKeyPressed(Key_Space) || input.mouse.left.pressed;
    if (jump && wt.position.y <= MAX_Y_TO_JUMP) {
        PhysicsImpulse@ imp = self.AddPhysicsImpulse();
        imp.impulse = vec3(Vec3_Up.x * JUMP_FORCE, Vec3_Up.y * JUMP_FORCE, Vec3_Up.z * JUMP_FORCE);
        Print("Jumping y=" + wt.position.y + " imp.y=" + imp.impulse.y);

        // Fire() sets playing = true in the SoundEffect component directly via the handle.
        // SoundEffectPlayback system picks it up this frame, plays and resets the flag.
        self.GetSoundEffect().Fire();
    }

    // T key: toggle the cube's checker texture on/off.
    // Setting path = "" triggers ResolveAlbedoMap (via modified()) which clears texture_handle.
    // Setting path = original triggers ResolveAlbedoMap which reloads the texture handle.
    if (input.WasKeyPressed(Key_T)) {
        AlbedoMap@ tmap = self.GetAlbedoMap();
        if (tmap.path != "") {
            tmap.path = "";
            Print("[T] Texture removed");
        } else {
            tmap.path = g_texturePath;
            Print("[T] Texture restored: " + g_texturePath);
        }
    }
}

void OnDestroy(Entity self) {}
