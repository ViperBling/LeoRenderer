#version 450

#extension GL_GOOGLE_include_directive : require

#include "../Base/Common.glsl"

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(BRDF(inUV.s, 1.0 - inUV.t), 0.0, 1.0);
}