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

struct MaterialFactor
{
    vec3 albedo;
    float metalic;
    float roughness;
    vec3 emissive;
    float AO;
};

struct PBRFactors
{
    float NoL;
    float NoV;
    float NoH;
    float VoH;
    float LoH;
    float perceptualRoughness;
    float alphaRoughness;
    vec3 diffuseColor;
    vec3 specularColor;
    vec3 reflectance0;
    vec3 reflectance90;
};

vec3 UnchartedTonemap(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

float D_GGX(PBRFactors pbrFactor)
{
    float alpha = pbrFactor.alphaRoughness * pbrFactor.alphaRoughness;
    // float alpha2 = alpha * alpha;
    float denom = pbrFactor.NoH * pbrFactor.NoH * (alpha - 1.0) + 1.0;
    return (alpha) / (PI * denom * denom); 
}

float G_SchlicksmithGGX(PBRFactors pbrFactor)
{
    float alphaRoughness2 = pbrFactor.alphaRoughness * pbrFactor.alphaRoughness;
    float NoL2 = pbrFactor.NoL * pbrFactor.NoL;
    float NoV2 = pbrFactor.NoV * pbrFactor.NoV;
    float GL = 2.0 * pbrFactor.NoL / (pbrFactor.NoL + sqrt(alphaRoughness2 + (1.0 - alphaRoughness2) * NoL2));
    float GV = 2.0 * pbrFactor.NoV / (pbrFactor.NoV + sqrt(alphaRoughness2 + (1.0 - alphaRoughness2) * NoV2));

    return GL * GV;
}

vec3 F_Schlick(PBRFactors pbrFactor)
{
    return pbrFactor.reflectance0 + (pbrFactor.reflectance90 - pbrFactor.reflectance0) * pow(clamp(1.0 - pbrFactor.VoH, 0.0, 1.0), 5.0);
}

vec3 F_SchlickR(PBRFactors pbrFactor)
{
    return pbrFactor.reflectance0 + (max(vec3(1.0 - pbrFactor.alphaRoughness), pbrFactor.reflectance0) - pbrFactor.reflectance0) * pow(1.0 - pbrFactor.VoH, 5.0);
}

vec3 CalculateNormal()
{
    vec3 tangentNormal = texture(samplerNormalMap, inUV0).xyz * 2.0 - vec3(1.0);

    vec3 q1 = dFdx(inWorldPos);
    vec3 q2 = dFdy(inWorldPos);
    vec2 st1 = dFdx(inUV0);
    vec2 st2 = dFdy(inUV0);

    vec3 N = normalize(inNormal);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    // vec3 T = normalize(inTangent.xyz);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

vec3 GetDirectionLight(MaterialFactor matFactor, PBRFactors pbrFactor)
{
    vec3 radiance = vec3(1.0) * 1.0f;

    vec3 F = F_Schlick(pbrFactor);
    float D = D_GGX(pbrFactor);
    float G = G_SchlicksmithGGX(pbrFactor);

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - matFactor.metalic;

    vec3 nominator = D * G * F;
    float denominator = 4.0 * pbrFactor.NoV * pbrFactor.NoL;

    vec3 diffuse = kD * (pbrFactor.diffuseColor / PI);
    vec3 specular = nominator / max(denominator, 0.0001);

    return (diffuse + specular) * radiance * pbrFactor.NoL;
}

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
    vec3 emissive = pow(matFactor.emissive.xyz,vec3(2.2));
    outColor = vec4(vec3(emissive), 1.0);
    
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

    float ambient = 10.0f;

    vec3 color = ambient * matFactor.AO * GetDirectionLight(matFactor, pbrFactor) + matFactor.emissive;

    color = UnchartedTonemap(color * uboParams.exposure);
    color = color * (1.0f / UnchartedTonemap(vec3(11.2f)));
    color = pow(color, vec3(1.0f / uboParams.gamma));

    outColor = vec4(color.rgb, 1.0);
}
