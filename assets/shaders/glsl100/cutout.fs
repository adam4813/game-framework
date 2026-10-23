precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragWorldPos;

uniform sampler2D texture0;

// Occluder fade: upright quads that cover the focus point (the camera's LookAt target, i.e. the
// player) on screen become alpha-blended translucent so the player shows through — Stardew-style.
// Driven per-frame by the engine via Platform::SetOcclusionFade. Screen-space coverage is used (not
// world distance) so tall quads whose canopy leans over the player on screen fade too. A
// fadeStrength of 0 disables the effect, rendering a plain alpha-tested cutout.
uniform vec3  fadeFocus;     // world-space focus position (player) — only .z is used, for the in-front test
uniform vec2  fadeScreenPos; // focus position in gl_FragCoord space (pixels, origin bottom-left)
uniform float fadeRadius;    // screen-space radius (pixels) of the fade region around the focus
uniform float fadeStrength;  // transparency applied over the focus (0 = off, 1 = fully invisible)
uniform float fadePass;      // 0 = opaque pass (punch the hole), 1 = transparent pass (draw the ghost)

void main()
{
    vec4 texelColor = texture2D(texture0, fragTexCoord);
    // RGB vertex colour tints the sprite; the vertex-colour ALPHA carries occluder metadata (below),
    // so the sprite's own texture alpha drives the cutout test.
    vec4 finalColor = texelColor * vec4(fragColor.rgb, 1.0);

    // Alpha-tested cutout: drop near-transparent texels (sprite gaps) using the source texture alpha,
    // before any fade logic, so the surviving body texels can be split between the two passes below.
    if (finalColor.a < 0.5)
        discard;

    // Per-vertex "fadeable" flag baked by the upright mesh builder into the vertex-colour alpha: 1 for
    // quads taller than one tile (which can obscure the player), 0 for short quads that never fade.
    float fadeable = fragColor.a;

    // Screen-space coverage of the focus by this fragment (0 = not covering, up to fadeStrength).
    float fade = 0.0;
    if (fadeStrength > 0.0 && fadeable > 0.5) {
        // Only fragments in front of the focus (closer to the camera, which looks along +Z) can cover it.
        float dz = fadeFocus.z - fragWorldPos.z;
        if (dz > 0.02) {
            float d = distance(gl_FragCoord.xy, fadeScreenPos);
            float cover = 1.0 - smoothstep(fadeRadius * 0.6, fadeRadius, d);
            fade = fadeStrength * cover;
        }
    }

    if (fadePass < 0.5) {
        // Opaque pass: draw everything except the covering fragments, leaving a hole (no colour, no
        // depth) exactly where the player is, so the opaque player pass fills it.
        if (fade > 0.0)
            discard;
    } else {
        // Transparent pass: redraw ONLY the covering fragments as a translucent ghost over the player.
        if (fade <= 0.0)
            discard;
        finalColor.a *= (1.0 - fade);
    }

    gl_FragColor = finalColor;
}
