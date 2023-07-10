#version 450

#extension GL_GOOGLE_include_directive : enable
#include "Common.h"

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;




// Based omn http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
// float Random(vec2 co)
// {
// 	float a = 12.9898;
// 	float b = 78.233;
// 	float c = 43758.5453;
// 	float dt = dot(co.xy ,vec2(a,b));
// 	float sn = mod(dt,3.14);
// 	return fract(sin(sn) * c);
// }

// vec2 Hammersley2D(uint i, uint N) 
// {
// 	// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// 	uint bits = (i << 16u) | (i >> 16u);
// 	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
// 	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
// 	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
// 	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
// 	float rdi = float(bits) * 2.3283064365386963e-10;
// 	return vec2(float(i) /float(N), rdi);
// }

// // Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
// vec3 ImportanceSampleGGX(vec2 Xi, float roughness, vec3 normal)
// {
//     float alpha = roughness * roughness;
//     float phi = 2.0 * PI * Xi.x + Random(normal.xz) * 0.1;
//     float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha * alpha - 1.0) * Xi.y));
//     float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
//     vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

//     // 切线空间
//     vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
//     vec3 tangentX = normalize(cross(up, normal));
//     vec3 tangentY = normalize(cross(normal, tangentX));
//     // 转换到世界空间
//     return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
// }

// float GSchlickSmithGGX(float NdotL, float NdotV, float roughness)
// {
//     float K = (roughness * roughness) / 2.0;
//     float GL = NdotL / (NdotL * (1.0 - K) + K);
//     float GV = NdotV / (NdotV * (1.0 - K) + K);
//     return GL * GV;
// }

// vec2 BRDF(float NdotV, float roughness)
// {
//     const vec3 N = vec3(0.0, 0.0, 1.0);
//     vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

//     vec2 LUT = vec2(0.0);
//     for (uint i = 0u; i < NUM_SAMPLES; i++)
//     {
//         vec2 Xi = Hammersley2D(i, NUM_SAMPLES);
//         vec3 H = ImportanceSampleGGX(Xi, roughness, N);
//         vec3 L = 2.0 * dot(V, H) * H - V;

//         float NdotL = max(dot(N, L), 0.0);
//         float NdotV = max(dot(N, V), 0.0);
//         float NdotH = max(dot(N, H), 0.0);
//         float VdotH = max(dot(V, H), 0.0);

//         if (NdotL > 0.0)
//         {
//             float G = GSchlickSmithGGX(NdotL, NdotV, roughness);
//             float GVis = (G * VdotH) / (NdotH * NdotV);
//             float FC = pow(1.0 - VdotH, 5.0);

//             LUT += vec2((1.0 - FC) * GVis, FC * GVis);
//         }
//     }
//     return LUT / float(NUM_SAMPLES);
// }

void main()
{
    outColor = vec4(BRDF(inUV.s, 1.0 - inUV.t), 0.0, 1.0);
}