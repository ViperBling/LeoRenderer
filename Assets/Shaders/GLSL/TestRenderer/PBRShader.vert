#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inTangent;

layout (set = 0, binding = 0) uniform UBOScene
{
    mat4 projection;
    mat4 model;
    mat4 view;
    vec3 camPos;
} uboScene;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outTangent;

void main()
{
    vec3 positionLS = vec3(uboScene.model * vec4(inPos, 1.0));
    outWorldPos = positionLS;
    outNormal = mat3(uboScene.model) * inNormal;
    outTangent = vec4(mat3(uboScene.model) * inTangent.xyz, inTangent.w);
    outUV = inUV;
    gl_Position =  uboScene.projection * uboScene.view * vec4(outWorldPos, 1.0);
}