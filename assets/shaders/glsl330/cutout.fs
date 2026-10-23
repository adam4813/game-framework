#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;

out vec4 finalColor;

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    finalColor = texelColor * fragColor;

    // Alpha-tested cutout: drop near-transparent texels so the depth buffer resolves overlapping
    // upright sprites with no manual back-to-front sort. Kept fragments write opaque depth.
    if (finalColor.a < 0.5)
        discard;
}
