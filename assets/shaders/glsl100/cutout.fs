precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;

uniform sampler2D texture0;

void main()
{
    vec4 texelColor = texture2D(texture0, fragTexCoord);
    vec4 finalColor = texelColor * fragColor;

    // Alpha-tested cutout: drop near-transparent texels so the depth buffer resolves overlapping
    // upright sprites with no manual back-to-front sort. Kept fragments write opaque depth.
    if (finalColor.a < 0.5)
        discard;

    gl_FragColor = finalColor;
}
