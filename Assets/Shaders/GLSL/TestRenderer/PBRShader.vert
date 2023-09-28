#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inColor;
layout (location = 5) in vec4 inJoint;
layout (location = 6) in vec4 inWeight;
layout (location = 7) in vec4 inTangent;

#define MAX_NUM_JOINTS 128

layout (set = 0, binding = 0) uniform UBOScene
{
    mat4 projection;
    mat4 model;
    mat4 view;
    vec3 camPos;
} uboScene;

layout (set = 2, binding = 0) uniform UBONode
{
    mat4 matrix;
    mat4 jointMatrix[MAX_NUM_JOINTS];
    float jointCount;
} node;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV0;
layout (location = 3) out vec2 outUV1;
layout (location = 4) out vec4 outTangent;

void main()
{
    vec4 locPos;
    if (node.jointCount > 0.0)
    {
        // Mesh is skinned
        mat4 skinMat = 
            inWeight.x * node.jointMatrix[int(inJoint.x)] +
            inWeight.y * node.jointMatrix[int(inJoint.y)] +
            inWeight.z * node.jointMatrix[int(inJoint.z)] +
            inWeight.w * node.jointMatrix[int(inJoint.w)];

        locPos = uboScene.model * node.matrix * skinMat * vec4(inPos, 1.0);
        outNormal = normalize(transpose(inverse(mat3(uboScene.model * node.matrix * skinMat))) * inNormal);
    } 
    else 
    {
        locPos = uboScene.model * node.matrix * vec4(inPos, 1.0);
        outNormal = normalize(transpose(inverse(mat3(uboScene.model * node.matrix))) * inNormal);
    }

    outTangent = inTangent;
    // outTangent = normalize(outTangent - dot(outTangent, outNormal) * outNormal);

    // locPos.y = -locPos.y;
    vec3 positionWS = vec3(uboScene.model * vec4(inPos, 1.0));
    outWorldPos = locPos.xyz / locPos.w;
    outUV0 = inUV0;
    outUV1 = inUV1;
    gl_Position =  uboScene.projection * uboScene.view * vec4(outWorldPos, 1.0);
}