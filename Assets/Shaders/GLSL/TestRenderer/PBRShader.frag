#version 450

#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inTangent;

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

float D_GGX(float NoH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NoH * NoH * (alpha2 - 1.0) + 1.0;
    return (alpha2) / (PI * denom * denom); 
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
    return pbrFactor.reflectance0 + (pbrFactor.reflectance90 - pbrFactor.reflectance0) * pow(1.0 - pbrFactor.VoH, 5.0);
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 CalculateNormal()
{
    vec3 tangentNormal = texture(samplerNormalMap, inUV).xyz * 2.0 - 1.0;
    
    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTangent);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

vec3 GetDirectionLight(vec3 N, vec3 L, vec3 V, vec3 H, MaterialFactor matFactor, PBRFactors pbrFactor)
{
    pbrFactor.NoL = max(dot(N, L), 0.001);
    pbrFactor.NoH = max(dot(N, H), 0.001);
    pbrFactor.VoH = max(dot(H, V), 0.001);

    vec3 radiance = vec3(1.0) * 50.0f;

    vec3 F = F_Schlick(pbrFactor);
    float D = D_GGX(pbrFactor.NoH, matFactor.roughness);
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
    vec3 R = -normalize(reflect(V, N));
    vec3 L = normalize(uboParams.lightPos.xyz);
    vec3 H = normalize(V + L);

    MaterialFactor matFactor;
    {
        matFactor.albedo = ALBEDO;
        matFactor.metalic = texture(samplerMetalicRoughnessMap, inUV).b;
        matFactor.roughness = texture(samplerMetalicRoughnessMap, inUV).g;
        // matFactor.metalic = 0.5f;
        // matFactor.roughness = 0.1f;
        matFactor.AO = texture(samplerAOMap, inUV).r;
        matFactor.emissive = texture(samplerEmissiveMap, inUV).rgb;
    }
    
    PBRFactors pbrFactor;
    {
        float F0 = 0.04;
        pbrFactor.NoV = max(dot(N, V), 0.001);
        pbrFactor.diffuseColor = matFactor.albedo.rgb * (1.0 - F0);
        pbrFactor.diffuseColor *= 1.0 - matFactor.metalic;
        pbrFactor.specularColor = mix(vec3(F0), matFactor.albedo, matFactor.metalic);
        
        pbrFactor.perceptualRoughness = clamp(matFactor.roughness, 0.04, 1.0);
        pbrFactor.alphaRoughness = pbrFactor.perceptualRoughness * pbrFactor.perceptualRoughness;

        float reflectance = max(max(pbrFactor.specularColor.r, pbrFactor.specularColor.g), pbrFactor.specularColor.g);
        pbrFactor.reflectance0 = pbrFactor.specularColor.rgb;
        pbrFactor.reflectance90 = vec3(clamp(reflectance * 25.0, 0.0, 1.0));
    }

    float ambient = 1000.0f;

    vec3 color = ambient * GetDirectionLight(N, L, V, H, matFactor, pbrFactor);

    color = UnchartedTonemap(color * uboParams.exposure) * matFactor.AO;
    color = color * (1.0f / UnchartedTonemap(vec3(11.2f)));
    color = pow(color, vec3(1.0f / uboParams.gamma));

    outColor = vec4(color.rgb, 1.0);
}
