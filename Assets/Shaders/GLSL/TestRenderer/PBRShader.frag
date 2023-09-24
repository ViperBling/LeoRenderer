#version 450

layout (location = 0) in vec3 inWorldPos;
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

layout (set = 0, binding = 1) uniform UBOParam
{
    vec4 lightPos;
    float exposure;
    float gamma;
} uboParams;

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;
layout (set = 1, binding = 1) uniform sampler2D samplerNormalMap;
layout (set = 1, binding = 2) uniform sampler2D samplerMetalicRoughnessMap;
layout (set = 1, binding = 3) uniform sampler2D samplerAOMap;
layout (set = 1, binding = 4) uniform sampler2D samplerEmissiveMap;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
#define ALBEDO pow(texture(samplerColorMap, inUV).rgb, vec3(2.2))


void main()
{
    vec4 color = texture(samplerColorMap, inUV);
    vec3 normal = texture(samplerNormalMap, inUV).xyz;

    vec3 N = normalize(inNormal);
    vec3 L = normalize(uboParams.lightPos.xyz - inWorldPos);
    float diffuse = max(dot(N, L), 0.15);
    outColor = vec4(color.rgb, 1.0) * 2.0f;
}
