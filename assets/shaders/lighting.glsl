// The scene lighting, shared by every lit fragment shader: #include "lighting.glsl", then call SceneLighting. A custom
// shader opts in the same way; the platform feeds these uniforms to any program that declares them.
//
// Point lights are clustered (ADR 0009): the view frustum is split into screen tiles and depth slices, and each fragment
// evaluates only the lights its cluster lists. The layout is packed by the backend's UploadLightClusters
// (raylib_backend.cpp) from BuildLightClusters (light_clusters.hpp).

uniform vec3 lightDir;   // direction the directional light travels (world space)
uniform vec4 lightColor; // directional light color (rgb) * intensity in a
uniform vec4 ambient;    // ambient color (rgb) * intensity in a
uniform vec3 viewPos;     // camera position
uniform vec3 viewForward; // camera look direction (unit length)
uniform float specularStrength;
uniform float shininess;

uniform int pointLightCount; // lights kept for this frame; 0 skips the cluster lookup
uniform ivec3 clusterGrid;   // tiles across, tiles up, depth slices
uniform vec2 clusterTile;    // tiles per pixel
uniform vec2 clusterSlice;   // slice = log(view depth) * x + y

// All three hold RGBA32F texels read by index, row by row.
uniform sampler2D lightTexture;      // 4 texels per light: (position, range), (color, intensity),
                                     // (constant, linear, quadratic, unused), and one spare for spot lights
uniform sampler2D clusterTexture;    // 1 texel per cluster: (first entry in lightIndexTexture, entry count, unused, unused)
uniform sampler2D lightIndexTexture; // 4 light indices per texel

vec4 FetchTexel(sampler2D tex, int index) {
	int width = textureSize(tex, 0).x;
	return texelFetch(tex, ivec2(index % width, index / width), 0);
}

int ClusterIndex(vec3 position) {
	float depth = max(dot(position - viewPos, viewForward), 0.0001);
	ivec3 cell = ivec3(gl_FragCoord.xy * clusterTile, floor(log(depth) * clusterSlice.x + clusterSlice.y));
	cell = clamp(cell, ivec3(0), clusterGrid - 1);
	return cell.x + clusterGrid.x * (cell.y + clusterGrid.y * cell.z);
}

// Diffuse plus specular of one light arriving from the unit direction `toLight`.
float PhongTerm(vec3 normal, vec3 viewDir, vec3 toLight) {
	float diffuse = max(dot(normal, toLight), 0.0);
	float specular = pow(max(dot(viewDir, reflect(-toLight, normal)), 0.0), shininess) * specularStrength * diffuse;
	return diffuse + specular;
}

vec3 PointLighting(vec3 position, vec3 normal, vec3 viewDir) {
	vec3 total = vec3(0.0);
	vec2 entries = FetchTexel(clusterTexture, ClusterIndex(position)).xy;
	int first = int(entries.x);
	int count = int(entries.y);
	for (int i = 0; i < count; ++i) {
		int entry = first + i;
		int light = int(FetchTexel(lightIndexTexture, entry / 4)[entry % 4]) * 4;

		vec4 positionRange = FetchTexel(lightTexture, light);
		vec3 toLight = positionRange.xyz - position;
		float dist = length(toLight);
		if (dist >= positionRange.w) continue;

		vec4 color = FetchTexel(lightTexture, light + 1);
		vec3 falloff = FetchTexel(lightTexture, light + 2).xyz;
		float attenuation = 1.0 / (falloff.x + falloff.y * dist + falloff.z * dist * dist);
		// Takes the attenuation, which never reaches zero by itself, smoothly to zero at the light's range.
		float reach = dist / positionRange.w;
		float window = 1.0 - reach * reach * reach * reach;
		attenuation *= window * window;

		total += PhongTerm(normal, viewDir, toLight / max(dist, 0.0001)) * color.rgb * color.a * attenuation;
	}
	return total;
}

// The light arriving at a world-space surface point from the ambient, directional and point lights. `normal` is unit
// length and faces the camera's side of the surface.
vec3 SceneLighting(vec3 position, vec3 normal) {
	vec3 viewDir = normalize(viewPos - position);
	vec3 lighting = ambient.rgb * ambient.a;
	lighting += PhongTerm(normal, viewDir, normalize(-lightDir)) * lightColor.rgb * lightColor.a;
	if (pointLightCount > 0) lighting += PointLighting(position, normal, viewDir);
	return lighting;
}
