#version 450

#extension GL_GOOGLE_include_directive : enable

#include "../Base/Common.glsl"

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inTangent;

layout (set = 0, binding = 0) uniform UBOScene
{
    mat4 projection;
    mat4 model;
    mat4 view;
    vec3 camPos;
} uboScene;

layout (set = 0, binding = 1) uniform UBOParam
{
    vec4 lightPos;
    float exposure;
    float gamma;
} uboParams;

layout(std430, set = 3, binding = 0) buffer SSBO
{
   ShaderMaterial materials[ ];
};

layout (push_constant) uniform PushConstants 
{
    int materialIndex;
} pushConstants;

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;
layout (set = 1, binding = 1) uniform sampler2D samplerMetalicRoughnessMap;
layout (set = 1, binding = 2) uniform sampler2D samplerNormalMap;
layout (set = 1, binding = 3) uniform sampler2D samplerAOMap;
layout (set = 1, binding = 4) uniform sampler2D samplerEmissiveMap;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
#define ALBEDO pow(texture(samplerColorMap, inUV0).rgb, vec3(2.2))

void main()
{
    ShaderMaterial material = materials[pushConstants.materialIndex];

    vec4 baseColor;

    if (material.baseColorTextureSet > -1) 
    {
        baseColor = SRGBtoLINEAR(texture(samplerColorMap, material.baseColorTextureSet == 0 ? inUV0 : inUV1)) * material.baseColorFactor;
    } else 
    {
        baseColor = material.baseColorFactor;
    }

    outColor = baseColor;
}
