#version 450

layout (binding = 2) uniform samplerCube samplerEnv;

layout (location = 0) in vec3 inUVW;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 1) uniform UBOParams
{
    vec4 _pad0;
    float exposure;
    float gamma;
} uboParams;

vec3 UnchartedToToneMap(vec3 color)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ( (color * (A * color + C * B) + D * E) / (color * (A * color + D * F)) - E / F );
}

vec4 ToneMap(vec4 color)
{
    vec3 OutColor = UnchartedToToneMap(color.rgb * uboParams.exposure);
    OutColor = OutColor * (1.0f / UnchartedToToneMap(vec3(11.2f)));
    return vec4(pow(OutColor, vec3(1.0f / uboParams.gamma)), color.a);
}

#define MANUAL_SRGB 1

vec4 SRGBToLinear(vec4 srgbIn)
{
#ifdef MANUAL_SRGB

#ifdef SRGB_FAST_APPROXIMATION
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
#else //SRGB_FAST_APPROXIMATION
    vec3 bLess = step(vec3(0.04045), srgbIn.xyz);
    vec3 linOut = mix( srgbIn.xyz / vec3(12.92), pow((srgbIn.xyz + vec3(0.055)) / vec3(1.055), vec3(2.4)), bLess );
#endif //SRGB_FAST_APPROXIMATION
    return vec4(linOut, srgbIn.w);;
#else //MANUAL_SRGB
    return srgbIn;

#endif //MANUAL_SRGB
}

void main()
{
    vec3 color = SRGBToLinear(ToneMap(textureLod(samplerEnv, inUVW, 1.5))).rgb;
    outColor = vec4(color * 1.0, 1.0f);
}