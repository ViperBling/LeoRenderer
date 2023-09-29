#version 450

#extension GL_GOOGLE_include_directive : require

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

#include "../Base/Common.glsl"

layout (std430, set = 3, binding = 0) buffer SSBO
{
    ShaderMaterial materials[];
};

void main()
{
    vec3 N = CalculateNormal();
    vec3 V = normalize(uboScene.camPos - inWorldPos);
    vec3 L = normalize(uboParams.lightPos.xyz);
    vec3 H = normalize(L + V);

    MaterialFactor matFactor;
    {
        matFactor.albedo = ALBEDO;
        matFactor.metalic = texture(samplerMetalicRoughnessMap, inUV0).b;
        matFactor.roughness = texture(samplerMetalicRoughnessMap, inUV0).g;
        matFactor.AO = texture(samplerAOMap, inUV0).r;
        matFactor.emissive = texture(samplerEmissiveMap, inUV0).rgb;
    }
    
    PBRFactors pbrFactor;
    {
        float F0 = 0.04;
        pbrFactor.NoL = clamp(dot(N, L), 0.001, 1.0);
        pbrFactor.NoV = clamp(abs(dot(N, V)), 0.001, 1.0);
        pbrFactor.NoH = clamp(dot(N, H), 0.0, 1.0);
        pbrFactor.LoH = clamp(dot(L, H), 0.0, 1.0);
        pbrFactor.VoH = clamp(dot(V, H), 0.0, 1.0);

        pbrFactor.diffuseColor = matFactor.albedo.rgb * (1.0 - F0);
        pbrFactor.diffuseColor *= 1.0 - matFactor.metalic;
        pbrFactor.specularColor = mix(vec3(F0), matFactor.albedo, matFactor.metalic);
        
        pbrFactor.perceptualRoughness = clamp(matFactor.roughness, 0.04, 1.0);
        pbrFactor.alphaRoughness = pbrFactor.perceptualRoughness * pbrFactor.perceptualRoughness;

        float reflectance = max(max(pbrFactor.specularColor.r, pbrFactor.specularColor.g), pbrFactor.specularColor.b);
        pbrFactor.reflectance0 = pbrFactor.specularColor.rgb;
        pbrFactor.reflectance90 = vec3(clamp(reflectance * 25.0, 0.0, 1.0));
    }

    float ambient = 1.0f;

    vec3 color = ambient * matFactor.AO * GetDirectionLight(matFactor, pbrFactor) + matFactor.emissive;

    color = UnchartedTonemap(color * uboParams.exposure);
    color = color * (1.0f / UnchartedTonemap(vec3(11.2f)));
    color = pow(color, vec3(1.0f / uboParams.gamma));

    outColor = vec4(color.rgb, 1.0);
}