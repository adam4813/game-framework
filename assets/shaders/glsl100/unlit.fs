precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;

uniform sampler2D texture0;

void main()
{
    vec4 texelColor = texture2D(texture0, fragTexCoord);
    gl_FragColor = texelColor * fragColor;
}
