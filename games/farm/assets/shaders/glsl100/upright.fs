precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Upright sprite quads (trees, buildings, villagers): an alpha-tested cutout tinted by the vertex
// color. The vertex alpha scales the opacity, so the occluder mesh (drawn by the translucent pass)
// shows the player through an object that stands in front of it.
void main()
{
    vec4 texelColor = texture2D(texture0, fragTexCoord);
    // Test the sprite's own alpha, so a see-through occluder keeps its silhouette.
    if (texelColor.a < 0.5)
        discard;
    gl_FragColor = texelColor * fragColor * colDiffuse;
}
