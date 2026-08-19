#version 100

// Phong lighting fragment shader (GLSL ES 1.00 / WebGL): ambient + directional + specular.
precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragPosition;
varying vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3 lightDir;
uniform vec4 lightColor;
uniform vec4 ambient;
uniform vec3 viewPos;
uniform float specularStrength;
uniform float shininess;

uniform float shadowPass;
uniform vec4 shadowColor;

void main() {
	if (shadowPass > 0.5) {
		gl_FragColor = shadowColor;
		return;
	}

	vec4 texel = texture2D(texture0, fragTexCoord);
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
	gl_FragColor = vec4(color, base.a);
}
