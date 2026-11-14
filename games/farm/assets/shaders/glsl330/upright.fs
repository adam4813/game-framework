#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

// Upright sprite quads (trees, buildings, villagers): an alpha-tested cutout tinted by the vertex
// color. The vertex alpha scales the opacity, so the occluder mesh (drawn by the translucent pass)
// shows the player through an object that stands in front of it.
void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    // Test the sprite's own alpha, so a see-through occluder keeps its silhouette.
    if (texelColor.a < 0.5)
        discard;
    finalColor = texelColor * fragColor * colDiffuse;
}
