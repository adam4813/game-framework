#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 matProjection;
uniform mat4 matView;
uniform mat4 matModel;

out vec2 fragTexCoord;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    gl_Position = matProjection * matView * matModel * vec4(vertexPosition, 1.0);
}
