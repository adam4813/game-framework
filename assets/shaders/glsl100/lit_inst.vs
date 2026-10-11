#version 100

// Instanced Phong vertex shader (GLSL ES 1.00 / WebGL). Per-instance model matrix and colour
// arrive as vertex attributes (attribute divisor 1) so thousands of primitives draw in a single
// instanced call. Pairs with the regular lit.fs fragment shader.
precision mediump float;

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute mat4 instanceTransform;
attribute vec4 instanceColor;

uniform mat4 mvp;          // view * projection (the model matrix is per-instance)
uniform mat4 shadowMatrix; // planar shadow projection, applied when shadowPass > 0.5
uniform float shadowPass;

varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragPosition;
varying vec3 fragNormal;

void main() {
	mat4 model = instanceTransform;
	if (shadowPass > 0.5) {
		model = shadowMatrix * model;
	}
	vec4 world = model * vec4(vertexPosition, 1.0);
	mat3 normalMatrix = mat3(model[0].xyz, model[1].xyz, model[2].xyz);
	fragTexCoord = vertexTexCoord;
	fragColor = instanceColor;
	fragPosition = world.xyz;
	fragNormal = normalize(normalMatrix * vertexNormal);
	gl_Position = mvp * world;
}
