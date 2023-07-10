#version 450

#extension GL_GOOGLE_include_directive : enable
#include "Common.h"

layout (location = 0) in vec3 inPos;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform samplerCube samplerEnv;

layout(push_constant) uniform PushConstants 
{
	layout (offset = 64) float roughness;
	layout (offset = 68) uint numSamples;
} pushConsts;


vec3 PrefilterEnvMap(vec3 R, float roughness)
{
    vec3 N = R;
    vec3 V = R;
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;
    float envMapDim = float(textureSize(samplerEnv, 0).s);

    for (uint i = 0u; i < pushConsts.numSamples; i++)
    {
        vec2 Xi = Hammersley2D(i, pushConsts.numSamples);
        vec3 H = ImportanceSampleGGX(Xi, roughness, N);
        vec3 L = 2.0 * dot(V, H) * H - V;

        float NdotL = clamp(dot(N, L), 0.0, 1.0);
        if (NdotL > 0.0)
        {
            // Filtering based on https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
            float NdotH = clamp(dot(N, H), 0.0, 1.0);
			float VdotH = clamp(dot(V, H), 0.0, 1.0);

            // 概率分布
            float PDF = DGGX(NdotH, roughness) * NdotH / (4.0 * VdotH) + 0.0001;
            // Solid angle of current sample
            float omegaS = 1.0 / (float(pushConsts.numSamples) * PDF);
            // Solid angle of 1 pixel across all cube faces
            float omegaP = 4.0 * PI / (6.0 * envMapDim * envMapDim);
            // Biased (+1.0) mip level for better result
            float mipLevel = roughness == 0.0 ? 0.0 : max(0.5 * log2(omegaS / omegaP) + 1.0, 0.0f);

            color += textureLod(samplerEnv, L, mipLevel).rgb * NdotL;
        } 
    }
    return (color / totalWeight);
}

void main()
{
    vec3 N = normalize(inPos);
    outColor = vec4(PrefilterEnvMap(N, pushConsts.roughness), 1.0);
}