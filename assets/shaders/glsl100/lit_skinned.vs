#version 100

// Phong lighting vertex shader with GPU skinning (GLSL ES 1.00 / WebGL): identical outputs to lit.vs.
precision mediump float;

#define MAX_BONES 64

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;
attribute vec4 vertexBoneIds;
attribute vec4 vertexBoneWeights;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 boneMatrices[MAX_BONES];

varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragPosition;
varying vec3 fragNormal;

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
