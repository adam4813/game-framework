#version 330

// Instanced Phong vertex shader. The per-instance model matrix and colour arrive as vertex
// attributes (attribute divisor 1) so thousands of primitives draw in a single instanced call.
// Pairs with the regular lit.fs fragment shader (fragColor carries the per-instance tint).
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in mat4 instanceTransform;
in vec4 instanceColor;

uniform mat4 mvp;          // view * projection (the model matrix is per-instance)
uniform mat4 shadowMatrix; // planar shadow projection, applied when shadowPass > 0.5
uniform float shadowPass;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;

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
