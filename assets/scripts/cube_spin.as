// cube_spin.as — Applies a periodic torque kick to the parent cube every N seconds.
//
// Demonstrates:
//   - Timer accumulation pattern: accumulate dt each tick; fire when the
//     timer exceeds a threshold, then subtract to keep the remainder.
//   - PhysicsForce component with a direct torque field (world-space, no position math).
//     Different from PhysicsImpulse: force/torque are applied for one frame then cleared.
//   - vec3(x, y, z) constructor syntax.
//   - Vec3_Right constant: torque along X axis makes the cube tumble toward/away from camera.
//   - self.GetParent() / GetName() — entity navigation from a child script.
//   - Multiple scripts on one entity: second ScriptComponent child of FallingCube alongside
//     cube_jump.as; both run independently each frame.

const float KICK_INTERVAL = 3.0f; // seconds between torque kicks
const float SPIN_STRENGTH = 80.0f;

float g_timer = 2.0f; // start mid-interval so first kick isn't simultaneous with a jump

void OnInit(Entity self) {
    // Demonstrate GetParent() / GetName() from inside a child script entity.
    Print("[CubeSpinScript] Attached to '" + self.GetParent().GetName() + "'");
}

void Tick(Entity self, float dt) {
    // Timer accumulation: add dt each tick and fire once the interval is reached.
    g_timer += dt;
    if (g_timer < KICK_INTERVAL) {
        return;
    }
    g_timer -= KICK_INTERVAL; // subtract rather than reset to preserve the remainder

    // Apply a one-frame torque impulse via PhysicsForce (PhysicsApplyForces system picks this
    // up and removes it the same frame). Torque along Vec3_Up (Y axis) spins the cube like
    // a top — visible from any camera angle. Vec3_Right is referenced in the comment; see
    // cube_jump.as for an example that uses Vec3_Right.x as a value.
    PhysicsForce@ f = self.AddPhysicsForce();
    f.torque = vec3(Vec3_Up.x * SPIN_STRENGTH, Vec3_Up.y * SPIN_STRENGTH, Vec3_Up.z * SPIN_STRENGTH);
    f.clear_after_apply = true;
    Print("[CubeSpinScript] Torque kick — torque.y=" + f.torque.y);
}

void OnDestroy(Entity self) {}
