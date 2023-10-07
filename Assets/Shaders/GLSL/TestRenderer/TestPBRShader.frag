#version 450

#extension GL_GOOGLE_include_directive : require

#include "../Base/Common.glsl"

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inTangent;
layout (location = 5) in vec4 inColor;

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

layout (std430, set = 3, binding = 0) buffer SSBO
{
    ShaderMaterial materials[];
};

layout (push_constant) uniform PushConstants 
{
	int materialIndex;
} pushConstants;

void main()
{
    ShaderMaterial material = materials[pushConstants.materialIndex];

    vec3 N = (material.normalTextureSet > -1) ? CalculateNormal(texture(samplerNormalMap, inUV0).xyz * 2.0 - vec3(1.0), inWorldPos, inNormal, inUV0) : normalize(inNormal);
    vec3 V = normalize(inWorldPos - uboScene.camPos);
    vec3 L = normalize(uboParams.lightPos.xyz);
    vec3 H = normalize(L + V);

    MaterialFactor matFactor;
    {
        if (material.alphaMask == 1.0f) 
        {
            if (material.baseColorTextureSet > -1) 
            {
                matFactor.albedo = SRGBtoLINEAR(texture(samplerColorMap, material.baseColorTextureSet == 0 ? inUV0 : inUV1)) * material.baseColorFactor;
            } 
            else 
            {
                matFactor.albedo = material.baseColorFactor;
            }
            
            if (matFactor.albedo.a < material.alphaMaskCutoff) 
            {
                discard;
            }
        }
        
        if (material.workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS)
        {
            matFactor.roughness = material.roughnessFactor;
            matFactor.metalic = material.metallicFactor;
            if (material.physicalDescriptorTextureSet > -1) 
            {
                // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
                // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
                vec4 mrSample = texture(samplerMetalicRoughnessMap, material.physicalDescriptorTextureSet == 0 ? inUV0 : inUV1);
                matFactor.roughness *= mrSample.g;
                matFactor.metalic *= mrSample.b;
            } 
            else 
            {
                matFactor.roughness = clamp(matFactor.roughness, c_MinRoughness, 1.0);
                matFactor.metalic = clamp(matFactor.metalic, 0.0, 1.0);
            }
            // Roughness is authored as perceptual roughness; as is convention,
            // convert to material roughness by squaring the perceptual roughness [2].
            // The albedo may be defined from a base texture or a flat color
            if (material.baseColorTextureSet > -1) 
            {
                matFactor.albedo = SRGBtoLINEAR(texture(samplerColorMap, material.baseColorTextureSet == 0 ? inUV0 : inUV1)) * material.baseColorFactor;
            } 
            else 
            {
                matFactor.albedo = material.baseColorFactor;
            }
        }

        if (material.workflow == PBR_WORKFLOW_SPECULAR_GLOSINESS) 
        {
            // Values from specular glossiness workflow are converted to metallic roughness
            if (material.physicalDescriptorTextureSet > -1) 
            {
                matFactor.roughness = 1.0 - texture(samplerMetalicRoughnessMap, material.physicalDescriptorTextureSet == 0 ? inUV0 : inUV1).a;
            } 
            else 
            {
            	matFactor.roughness = 0.0;
            }
        
            const float epsilon = 1e-6;
        
            vec4 diffuse = SRGBtoLINEAR(texture(samplerColorMap, inUV0));
            vec3 specular = SRGBtoLINEAR(texture(samplerMetalicRoughnessMap, inUV0)).rgb;
        
            float maxSpecular = max(max(specular.r, specular.g), specular.b);
        
            // Convert metallic value from specular glossiness inputs
            matFactor.metalic = ConvertMetallic(diffuse.rgb, specular, maxSpecular);
        
            vec3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - c_MinRoughness) / max(1 - matFactor.metalic, epsilon)) * material.diffuseFactor.rgb;
            vec3 baseColorSpecularPart = specular - (vec3(c_MinRoughness) * (1 - matFactor.metalic) * (1 / max(matFactor.metalic, epsilon))) * material.specularFactor.rgb;
            matFactor.albedo = vec4(mix(baseColorDiffusePart, baseColorSpecularPart, matFactor.metalic * matFactor.metalic), diffuse.a);
        }
    }
    
    PBRFactors pbrFactor;
    {
        vec3 F0 = vec3(0.04);
        pbrFactor.NoL = clamp(dot(N, L), 0.001, 1.0);
        pbrFactor.NoV = clamp(abs(dot(N, V)), 0.001, 1.0);
        pbrFactor.NoH = clamp(dot(N, H), 0.0, 1.0);
        pbrFactor.LoH = clamp(dot(L, H), 0.0, 1.0);
        pbrFactor.VoH = clamp(dot(V, H), 0.0, 1.0);

        pbrFactor.diffuseColor = matFactor.albedo.rgb * (vec3(1.0) - F0);
        pbrFactor.diffuseColor *= 1.0 - matFactor.metalic;
        pbrFactor.specularColor = mix(F0, matFactor.albedo.rgb, matFactor.metalic);
        
        // pbrFactor.perceptualRoughness = clamp(matFactor.roughness, 0.04, 1.0);
        pbrFactor.alphaRoughness = matFactor.roughness * matFactor.roughness;

        float reflectance = max(max(pbrFactor.specularColor.r, pbrFactor.specularColor.g), pbrFactor.specularColor.b);
        pbrFactor.reflectance0 = pbrFactor.specularColor.rgb;
        pbrFactor.reflectance90 = vec3(clamp(reflectance * 25.0, 0.0, 1.0));
    }

    vec3 color = GetDirectionLight(vec3(1.0f), matFactor, pbrFactor);

    const float u_OcclusionStrength = 1.0f;
    if (material.occlusionTextureSet > -1) 
    {
        float ao = texture(samplerAOMap, (material.occlusionTextureSet == 0 ? inUV0 : inUV1)).r;
        color = mix(color, color * ao, u_OcclusionStrength);
    }

    vec3 emissive = vec3(0.0f);
    if (material.emissiveTextureSet > -1) 
    {
        emissive = material.emissiveFactor.rgb * material.emissiveStrength;
        emissive *= SRGBtoLINEAR(texture(samplerEmissiveMap, material.emissiveTextureSet == 0 ? inUV0 : inUV1)).rgb;
    };
    color += emissive;

    color = pow(vec3(color), vec3(0.4545));
    outColor = vec4(color.rgb, matFactor.albedo.a);
    // outColor = vec4(vec3(matFactor.metalic), matFactor.albedo.a);
}