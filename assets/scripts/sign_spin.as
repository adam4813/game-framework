// sign_spin.as — Turns the host entity slowly around the world up axis.
//
// Shows both faces of the double-sided demo sign (and the culled back of the single-sided one) without camera
// controls. Transform applies rotation x first, then y, so the stood-up quad turns about world up.
//
// Demonstrates:
//   - GetTransform() on a non-const self: a mutable handle that doesn't mark the component modified, which is all
//     a per-frame write needs (TransformPropagation recomputes the WorldTransform each frame).

const float SPIN_SPEED = 0.6f; // radians per second

void Tick(Entity self, float dt) {
    Transform@ t = self.GetTransform();
    t.rotation = vec3(t.rotation.x, t.rotation.y + SPIN_SPEED * dt, t.rotation.z);
}
