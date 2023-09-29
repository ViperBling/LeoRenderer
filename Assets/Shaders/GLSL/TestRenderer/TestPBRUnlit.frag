#version 450

#extension GL_GOOGLE_include_directive : enable

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
    outColor = vec4(vec3(1.0), 1.0f);
}
