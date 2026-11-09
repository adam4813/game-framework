#version 100

// Phong lighting vertex shader with GPU skinning (GLSL ES 1.00 / WebGL): identical outputs to lit.vs.
precision mediump float;

#define MAX_BONES 64 // keep in sync with kMaxSkinningBones in raylib_backend.cpp

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

// OpenGL ES 2.0 can't transpose a uniform on upload, and raylib stores matrices row-major, so each bone matrix
// arrives transposed. Transpose it back by hand (raylib's own skinning shader does the same).
mat4 boneMatrix(int i) {
	return mat4(
		vec4(boneMatrices[i][0].x, boneMatrices[i][1].x, boneMatrices[i][2].x, boneMatrices[i][3].x),
		vec4(boneMatrices[i][0].y, boneMatrices[i][1].y, boneMatrices[i][2].y, boneMatrices[i][3].y),
		vec4(boneMatrices[i][0].z, boneMatrices[i][1].z, boneMatrices[i][2].z, boneMatrices[i][3].z),
		vec4(boneMatrices[i][0].w, boneMatrices[i][1].w, boneMatrices[i][2].w, boneMatrices[i][3].w));
}

void main() {
	int b0 = int(vertexBoneIds.x);
	int b1 = int(vertexBoneIds.y);
	int b2 = int(vertexBoneIds.z);
	int b3 = int(vertexBoneIds.w);

	vec4 pos = vec4(vertexPosition, 1.0);
	vec4 nrm = vec4(vertexNormal, 0.0);
	vec4 skinnedPos =
		vertexBoneWeights.x * (boneMatrix(b0) * pos)
		+ vertexBoneWeights.y * (boneMatrix(b1) * pos)
		+ vertexBoneWeights.z * (boneMatrix(b2) * pos)
		+ vertexBoneWeights.w * (boneMatrix(b3) * pos);
	vec4 skinnedNormal =
		vertexBoneWeights.x * (boneMatrix(b0) * nrm)
		+ vertexBoneWeights.y * (boneMatrix(b1) * nrm)
		+ vertexBoneWeights.z * (boneMatrix(b2) * nrm)
		+ vertexBoneWeights.w * (boneMatrix(b3) * nrm);

	fragTexCoord = vertexTexCoord;
	fragColor = vertexColor;
	fragPosition = vec3(matModel * skinnedPos);
	fragNormal = normalize(vec3(matNormal * vec4(skinnedNormal.xyz, 1.0)));
	gl_Position = mvp * skinnedPos;
}
