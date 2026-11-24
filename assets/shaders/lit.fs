// Phong lighting fragment shader, paired with lit.vs, lit_skinned.vs and lit_inst.vs.
// texture0 and colDiffuse are bound automatically by raylib from the material's diffuse map.
#include "lighting.glsl"

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Planar-shadow pass: when shadowPass > 0.5 the primitive is flattened onto the
// ground plane and drawn as a flat, semi-transparent shadow.
uniform float shadowPass;
uniform vec4 shadowColor;

uniform float unlit; // > 0.5: output the base color without lighting

out vec4 finalColor;

void main() {
	if (shadowPass > 0.5) {
		finalColor = shadowColor;
		return;
	}

	vec4 base = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
	if (unlit > 0.5) {
		finalColor = base;
		return;
	}

	vec3 normal = normalize(fragNormal);
	// A back face is only rasterized for a double-sided material (culled draws never reach here). It carries its
	// front face's normal, so flip it to light the side the camera sees.
	if (!gl_FrontFacing) normal = -normal;

	finalColor = vec4(base.rgb * SceneLighting(fragPosition, normal), base.a);
}
