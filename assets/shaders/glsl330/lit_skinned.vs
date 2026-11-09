#version 330

// Phong lighting vertex shader with GPU skinning: identical outputs to lit.vs, so lit.fs lights it the same.
// Attribute + uniform names match raylib's defaults, which it binds and uploads for models with bones.
#define MAX_BONES 64 // keep in sync with kMaxSkinningBones in raylib_backend.cpp

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 boneMatrices[MAX_BONES];

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;

void main() {
	int b0 = int(vertexBoneIds.x);
	int b1 = int(vertexBoneIds.y);
	int b2 = int(vertexBoneIds.z);
	int b3 = int(vertexBoneIds.w);

	vec4 pos = vec4(vertexPosition, 1.0);
	vec4 nrm = vec4(vertexNormal, 0.0);
	vec4 skinnedPos =
		vertexBoneWeights.x * (boneMatrices[b0] * pos)
		+ vertexBoneWeights.y * (boneMatrices[b1] * pos)
		+ vertexBoneWeights.z * (boneMatrices[b2] * pos)
		+ vertexBoneWeights.w * (boneMatrices[b3] * pos);
	vec4 skinnedNormal =
		vertexBoneWeights.x * (boneMatrices[b0] * nrm)
		+ vertexBoneWeights.y * (boneMatrices[b1] * nrm)
		+ vertexBoneWeights.z * (boneMatrices[b2] * nrm)
		+ vertexBoneWeights.w * (boneMatrices[b3] * nrm);

	fragTexCoord = vertexTexCoord;
	fragColor = vertexColor;
	fragPosition = vec3(matModel * skinnedPos);
	fragNormal = normalize(vec3(matNormal * vec4(skinnedNormal.xyz, 1.0)));
	gl_Position = mvp * skinnedPos;
}
