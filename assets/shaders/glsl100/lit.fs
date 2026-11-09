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

// Point lights (up to 4, LightParams::kMaxPointLights; keep the array sizes and loop bound in sync): world position, colour (rgb) * intensity in a, and attenuation as
// (constant, linear, quadratic). Only the first pointCount entries are used.
uniform vec3 pointPos[4];
uniform vec4 pointColor[4];
uniform vec3 pointAtten[4];
uniform int pointCount;

uniform float shadowPass;
uniform vec4 shadowColor;

uniform float unlit; // > 0.5: output the base colour without lighting

void main() {
	if (shadowPass > 0.5) {
		gl_FragColor = shadowColor;
		return;
	}

	vec4 texel = texture2D(texture0, fragTexCoord);
	vec4 base = texel * colDiffuse * fragColor;

	if (unlit > 0.5) {
		gl_FragColor = base;
		return;
	}

	vec3 normal = normalize(fragNormal);
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
	gl_FragColor = vec4(color, base.a);
}
