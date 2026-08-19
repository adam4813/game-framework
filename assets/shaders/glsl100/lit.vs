#version 100

// Phong lighting vertex shader (GLSL ES 1.00 / WebGL). Names match raylib defaults.
precision mediump float;

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragPosition;
varying vec3 fragNormal;

void main() {
	fragTexCoord = vertexTexCoord;
	fragColor = vertexColor;
	fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
	fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));
	gl_Position = mvp * vec4(vertexPosition, 1.0);
}
