#version 450

#extension GL_GOOGLE_include_directive : require

#include "../Base/Common.glsl"

layout (binding = 2) uniform samplerCube samplerEnv;

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 1) uniform UBOParams
{
    vec4 _pad0;
    float exposure;
    float gamma;
} uboParams;

void main() 
{
    vec3 color = SRGBtoLINEAR(Tonemap(textureLod(samplerEnv, inUVW, 0), uboParams.exposure, uboParams.gamma)).rgb;	
    outColor = vec4(color * 1.0, 1.0);
}