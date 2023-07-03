#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inColor;
layout (location = 5) in vec4 inJoint0;
layout (location = 6) in vec4 inWeight0;

layout (set = 0, binding = 0) uniform UBO
{
    mat4 mProj;
    mat4 mModel;
    mat4 mView;
    vec3 mCamPos;
} ubo;

#define MAX_NUM_JOINTS 128

layout (set = 2, binding = 0) uniform UBONode
{
    mat4 mMat;
    mat4 mJointMatrix[MAX_NUM_JOINTS];
    float mJointCount;
} node;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV0;
layout (location = 3) out vec2 outUV1;
layout (location = 4) out vec4 outColor;

void main()
{
    outColor = inColor;

    vec4 localPos;
    if (node.mJointCount > 0.0)
    {
        mat4 skinMat =
            inWeight0.x * node.mJointMatrix[int(inJoint0.x)] +
            inWeight0.y * node.mJointMatrix[int(inJoint0.y)] +
            inWeight0.z * node.mJointMatrix[int(inJoint0.z)] +
            inWeight0.w * node.mJointMatrix[int(inJoint0.w)];

        localPos = ubo.mModel * node.mMat * skinMat * vec4(inPos, 1.0);
        outNormal = normalize(transpose(inverse(mat3(ubo.mModel * node.mMat * skinMat))) * inNormal);
    }
    else
    {
        localPos = ubo.mModel * node.mMat * vec4(inPos, 1.0);
        outNormal = normalize(transpose(inverse(mat3(ubo.mModel * node.mMat))) * inNormal);
    }
    localPos.y = -localPos.y;
    outWorldPos = localPos.xyz / localPos.w;
    outUV0 = inUV0;
    outUV1 = inUV1;
    gl_Position = ubo.mProj * ubo.mView * vec4(outWorldPos, 1.0);
}