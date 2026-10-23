precision mediump float;

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec4 vertexColor;

uniform mat4 matProjection;
uniform mat4 matView;
uniform mat4 matModel;

varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragWorldPos;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    vec4 worldPos = matModel * vec4(vertexPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    gl_Position = matProjection * matView * worldPos;
}
