#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inColor;

layout (set = 0, binding = 0) uniform UBO
{
    mat4 mProj;
    mat4 mModel;
    mat4 mView;
    vec3 mCamPos;
} UBO;

layout (set = 0, binding = 1) uniform UBOParams
{
    vec4 mLigthDir;
    float mExposure;
    float mGamma;
    float mPreFilterCubeMipLevel;
    float mScaleIBLAmbient;
    float mDebugViewInputs;
    float mDebugViewEquation;
} UBOParams;

layout (set = 0, binding = 2) uniform samplerCube smpIrradiance;
layout (set = 0, binding = 3) uniform samplerCube smpPrefilteredMap;
layout (set = 0, binding = 4) uniform sampler2D   smpBRDFLUT;

// Material Bindings
layout (set = 1, binding = 0) uniform sampler2D smpColorMap;
layout (set = 1, binding = 1) uniform sampler2D smpPhysicalDescMap;
layout (set = 1, binding = 2) uniform sampler2D smpNormalMap;
layout (set = 1, binding = 3) uniform sampler2D smpAOMap;
layout (set = 1, binding = 4) uniform sampler2D smpEmissiveMap;

layout (push_constant) uniform Material
{
    vec4 mBaseColorFactor;
    vec4 mEmissiveFactor;
    vec4 mDiffuseFactor;
    vec4 mSpecularFactor;
    float mWorkFlow;
    int mBaseColorTexSet;
    int mPhysicalDescTexSet;
    int mNormalTexSet;
    int mOcclusionTexSet;
    int mEmissiveTexSet;
    float mMetallicFactor;
    float mRoughnessFactor;
    float mAlphaMask;
    float mAlphaMaskCutOff;
} Material;

layout (location = 0) out vec4 outColor;

// 预计算的PBR数据
struct PBRInfo
{
    float mNdotL;                  // cos angle between normal and light direction
    float mNdotV;                  // cos angle between normal and view direction
    float mNdotH;                  // cos angle between normal and half vector
    float mLdotH;                  // cos angle between light direction and half vector
    float mVdotH;                  // cos angle between view direction and half vector
    float mPerceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float mMetalness;              // metallicfull reflectance color (normal incidence angle)
    vec3 mReflectance90;           // reflectance color at grazing angle value at the surface
    vec3 mReflectance0;            //
    float mAlphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 mDiffuseColor;            // color contribution from diffuse lighting
    vec3 mSpecularColor;           // color contribution from specular lighting
};

const float M_PI = 3.141592653589793;
const float c_MinRoughness = 0.04;

const float PBR_WORKFLOW_METALLIC_ROUGHNESS = 0.0;
const float PBR_WORKFLOW_SPECULAR_GLOSINESS = 1.0f;

#define MANUAL_SRGB 1

vec3 Uncharted2Tonemap(vec3 color)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

vec4 ToneMap(vec4 color)
{
    vec3 outcol = Uncharted2Tonemap(color.rgb * uboParams.exposure);
    outcol = outcol * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    return vec4(pow(outcol, vec3(1.0f / uboParams.gamma)), color.a);
}

vec4 SRGBtoLINEAR(vec4 srgbIn)
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

// 使用预定义的法线贴图或者mesh的法线和切线信息计算得到当前片段的Normal
vec3 GetNormal()
{
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(smpNormalMap, Material.mNormalTexSet == 0 ? inUV0 : inUV1).xyz * 2.0;

    vec3 q1 = dFdx(inWorldPos);
    vec3 q2 = dFdy(inWorldPos);
    vec2 st1 = dFdx(inUV0);
    vec2 st2 = dFdy(inUV0);

    vec3 N = normalize(inNormal);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

