#version 100

// Instanced Phong vertex shader (GLSL ES 1.00 / WebGL). Per-instance model matrix and colour
// arrive as vertex attributes (attribute divisor 1) so thousands of primitives or models draw in a single
// instanced call. Pairs with the regular lit.fs fragment shader.
precision mediump float;

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;
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
	// The cofactor matrix: the inverse transpose up to a positive scale, so normals stay right under non-uniform
	// scale. Its sign follows the determinant, so a mirrored instance keeps its normals facing out. highp, since the
	// products square and cube the scale.
	highp vec3 cx = model[0].xyz;
	highp vec3 cy = model[1].xyz;
	highp vec3 cz = model[2].xyz;
	highp mat3 normalMatrix = mat3(cross(cy, cz), cross(cz, cx), cross(cx, cy));
	float handedness = dot(cx, normalMatrix[0]) < 0.0 ? -1.0 : 1.0;
	if (shadowPass > 0.5) {
		model = shadowMatrix * model;
	}
	vec4 world = model * vec4(vertexPosition, 1.0);
	fragTexCoord = vertexTexCoord;
	fragColor = vertexColor * instanceColor;
	fragPosition = world.xyz;
	fragNormal = normalize(handedness * (normalMatrix * vertexNormal));
	gl_Position = mvp * world;
}
