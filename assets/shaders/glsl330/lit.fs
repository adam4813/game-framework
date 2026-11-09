#version 330

// Phong lighting fragment shader: ambient + single directional light + specular.
// texture0 and colDiffuse are bound automatically by raylib from the material's diffuse map.
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Lighting parameters (set via SetLighting on the platform).
uniform vec3 lightDir;      // direction the light travels (world space)
uniform vec4 lightColor;    // directional light colour (rgb) * intensity in a
uniform vec4 ambient;       // ambient colour (rgb) * intensity in a
uniform vec3 viewPos;       // camera position for specular highlights
uniform float specularStrength;
uniform float shininess;

// Point lights (up to 4, LightParams::kMaxPointLights; keep the array sizes and loop bound in sync): world position, colour (rgb) * intensity in a, and attenuation as
// (constant, linear, quadratic). Only the first pointCount entries are used.
uniform vec3 pointPos[4];
uniform vec4 pointColor[4];
uniform vec3 pointAtten[4];
uniform int pointCount;

// Planar-shadow pass: when shadowPass > 0.5 the primitive is flattened onto the
// ground plane and drawn as a flat, semi-transparent shadow.
uniform float shadowPass;
uniform vec4 shadowColor;

uniform float unlit; // > 0.5: output the base colour without lighting

out vec4 finalColor;

void main() {
	if (shadowPass > 0.5) {
		finalColor = shadowColor;
		return;
	}

	vec4 texel = texture(texture0, fragTexCoord);
	vec4 base = texel * colDiffuse * fragColor;

	if (unlit > 0.5) {
		finalColor = base;
		return;
	}

	vec3 normal = normalize(fragNormal);
	// A back face is only rasterized for a double-sided material (culled draws never reach here). It carries its
	// front face's normal, so flip it to light the side the camera sees.
	if (!gl_FrontFacing) normal = -normal;
	vec3 l = normalize(-lightDir);

	float diff = max(dot(normal, l), 0.0);

	vec3 viewD = normalize(viewPos - fragPosition);
	vec3 reflectD = reflect(-l, normal);
	float spec = pow(max(dot(viewD, reflectD), 0.0), shininess) * specularStrength * diff;

	vec3 lighting = ambient.rgb * ambient.a
		+ diff * lightColor.rgb * lightColor.a
		+ spec * lightColor.rgb * lightColor.a;

	for (int i = 0; i < 4; ++i) {
		if (i >= pointCount) break;
		vec3 toLight = pointPos[i] - fragPosition;
		float dist = length(toLight);
		vec3 pl = toLight / max(dist, 0.0001);
		float atten = 1.0 / (pointAtten[i].x + pointAtten[i].y * dist + pointAtten[i].z * dist * dist);
		float pdiff = max(dot(normal, pl), 0.0);
		vec3 pReflect = reflect(-pl, normal);
		float pspec = pow(max(dot(viewD, pReflect), 0.0), shininess) * specularStrength * pdiff;
		lighting += (pdiff + pspec) * pointColor[i].rgb * pointColor[i].a * atten;
	}

	vec3 color = base.rgb * lighting;
	finalColor = vec4(color, base.a);
}
