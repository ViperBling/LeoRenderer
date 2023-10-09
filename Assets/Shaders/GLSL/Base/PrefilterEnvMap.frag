#version 450

#extension GL_GOOGLE_include_directive : require

layout (location = 0) in vec3 inPos;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform samplerCube samplerEnv;

#include "../Base/Common.glsl"

layout(push_constant) uniform PushConsts 
{
	layout (offset = 64) float roughness;
	layout (offset = 68) uint numSamples;
} consts;

vec3 PrefilterEnvMap(vec3 R, float roughness, uint numSamples, float dim)
{
    vec3 N = R;
    vec3 V = R;
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;
    for(uint i = 0u; i < numSamples; i++) 
    {
        vec2 Xi = Hammersley2D(i, numSamples);
        vec3 H = ImportanceSampleGGX(Xi, roughness, N);
        vec3 L = 2.0 * dot(V, H) * H - V;
        float NoL = clamp(dot(N, L), 0.0, 1.0);

        if(NoL > 0.0) 
        {
            // Filtering based on https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
            float NoH = clamp(dot(N, H), 0.0, 1.0);
            float VoH = clamp(dot(V, H), 0.0, 1.0);

            // Probability Distribution Function
            float pdf = D_GGX(NoH, roughness) * NoH / (4.0 * VoH) + 0.0001;
            // Slid angle of current smple
            float omegaS = 1.0 / (float(numSamples) * pdf);
            // Solid angle of 1 pixel across all cube faces
            float omegaP = 4.0 * PI / (6.0 * dim * dim);
            // Biased (+1.0) mip level for better result
            float mipLevel = roughness == 0.0 ? 0.0 : max(0.5 * log2(omegaS / omegaP) + 1.0, 0.0f);
            color += textureLod(samplerEnv, L, mipLevel).rgb * NoL;
            totalWeight += NoL;
        }
    }
    return (color / totalWeight);
}


void main()
{
    vec3 N = normalize(inPos);
    float dim = float(textureSize(samplerEnv, 0).s);
    outColor = vec4(PrefilterEnvMap(N, consts.roughness, consts.numSamples, dim), 1.0);
}