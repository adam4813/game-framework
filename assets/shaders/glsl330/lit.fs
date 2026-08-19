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

// Planar-shadow pass: when shadowPass > 0.5 the primitive is flattened onto the
// ground plane and drawn as a flat, semi-transparent shadow.
uniform float shadowPass;
uniform vec4 shadowColor;

out vec4 finalColor;

void main() {
	if (shadowPass > 0.5) {
		finalColor = shadowColor;
		return;
	}

	vec4 texel = texture(texture0, fragTexCoord);
	vec4 base = texel * colDiffuse * fragColor;

	vec3 normal = normalize(fragNormal);
	vec3 l = normalize(-lightDir);

	float diff = max(dot(normal, l), 0.0);

	vec3 viewD = normalize(viewPos - fragPosition);
	vec3 reflectD = reflect(-l, normal);
	float spec = pow(max(dot(viewD, reflectD), 0.0), shininess) * specularStrength * diff;

	vec3 lighting = ambient.rgb * ambient.a
		+ diff * lightColor.rgb * lightColor.a
		+ spec * lightColor.rgb * lightColor.a;

	vec3 color = base.rgb * lighting;
	finalColor = vec4(color, base.a);
}
