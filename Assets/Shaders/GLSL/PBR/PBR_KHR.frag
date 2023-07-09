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
} uboMat;

layout (set = 0, binding = 1) uniform UBOParams
{
    vec4 mLigthDir;
    float mExposure;
    float mGamma;
    float mPreFilterCubeMipLevel;
    float mScaleIBLAmbient;
    float mDebugViewInputs;
    float mDebugViewEquation;
} uboParams;

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
} materialParams;

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
    vec3 outcol = Uncharted2Tonemap(color.rgb * uboParams.mExposure);
    outcol = outcol * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    return vec4(pow(outcol, vec3(1.0f / uboParams.mGamma)), color.a);
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
    vec3 tangentNormal = texture(smpNormalMap, materialParams.mNormalTexSet == 0 ? inUV0 : inUV1).xyz * 2.0;

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

// IBL系数
vec3 GetIBLContribution(PBRInfo pbrInputs, vec3 n, vec3 reflection)
{
    // 根据粗糙徐度等级确定LOD
    float LOD = (pbrInputs.mPerceptualRoughness * uboParams.mPreFilterCubeMipLevel);
    // 从粗糙度与法线和视角的夹角中对BRDFLUT进行打表，取得预计算的BRDF值
    vec3 BRDF = (texture(smpBRDFLUT, vec2(pbrInputs.mNdotV, 1.0 - pbrInputs.mPerceptualRoughness))).rgb;

    // 采样环境辐照度图来得到Diffuse
    vec3 diffuseLight = SRGBtoLINEAR(ToneMap(texture(smpIrradiance, n))).rgb;
    // 根据不同粗糙度等级的LOD采样预过滤的环境光贴图得到高光
    vec3 specularLight = SRGBtoLINEAR(ToneMap(textureLod(smpPrefilteredMap, reflection, LOD))).rgb;

    vec3 diffuse = diffuseLight * pbrInputs.mDiffuseColor;
    vec3 specular = specularLight * (pbrInputs.mSpecularColor * BRDF.x + BRDF.y);

    diffuse *= uboParams.mScaleIBLAmbient;
    specular *= uboParams.mScaleIBLAmbient;

    return diffuse + specular;
}

// Lambert
vec3 Diffuse(PBRInfo pbrInputs)
{
    return pbrInputs.mDiffuseColor / M_PI;
}

// 菲涅尔项：F = F0 + （1 - F0)(1 - dot(h, v))^5
vec3 SpecularReflection(PBRInfo pbrInputs)
{
    return pbrInputs.mReflectance0 + (pbrInputs.mReflectance90 - pbrInputs.mReflectance0) * pow(clamp(1.0 - pbrInputs.mVdotH, 0.0, 1.0), 5.0);
}

// 几何项，描述微表面互遮挡。GGX分布函数
float GeometryOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.mNdotL;
    float NdotV = pbrInputs.mNdotV;
    float alpha = pbrInputs.mAlphaRoughness;

    float attenL = 2.0 * NdotL / (NdotL + sqrt(alpha * alpha + (1 - alpha * alpha) * (NdotL * NdotL)));
    float attenV = 2.0 * NdotV / (NdotV + sqrt(alpha * alpha + (1 - alpha * alpha) * (NdotV * NdotV)));

    return attenL * attenV;
}

// D项，法线分布
float MicroFacetDistribution(PBRInfo pbrInputs)
{
    float NdotH = pbrInputs.mNdotH;
    float alpha2 = pbrInputs.mAlphaRoughness * pbrInputs.mAlphaRoughness;

    float NDF = (NdotH * alpha2 - NdotH) * NdotH + 1.0;

    return alpha2 / (M_PI * NDF * NDF);
}

// 从SpecularGlossiness装换成MetallicRoughness
float ConvertMetallic(vec3 diffuse, vec3 specular, float maxSpecular)
{
    float perceivedDiffuse = sqrt(0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b);
    float perceivedSpecular = sqrt(0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b);

    if (perceivedSpecular < c_MinRoughness) return 0.0;

    float a = c_MinRoughness;
    float b = perceivedDiffuse * (1.0 - maxSpecular) / (1.0 - c_MinRoughness) + perceivedSpecular - 2.0 * c_MinRoughness;
    float c = c_MinRoughness - perceivedSpecular;
    float D = max(b * b - 4.0 * a * c, 0.0);

    return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}

void main()
{
    float perceptualRoughness;
    float metallic;
    vec3 diffuseColor;
    vec4 baseColor;

    vec3 F0 = vec3(0.04);

    if (materialParams.mAlphaMask == 1.0f)
    {
        if (materialParams.mBaseColorTexSet > -1)
        {
            baseColor = SRGBtoLINEAR(texture(smpColorMap, materialParams.mBaseColorTexSet == 0 ? inUV0 : inUV1)) * materialParams.mBaseColorFactor;
        }
        else
        {
            baseColor = materialParams.mBaseColorFactor;
        }
        if (baseColor.a < materialParams.mAlphaMaskCutOff) discard;
    }

    if (materialParams.mWorkFlow == PBR_WORKFLOW_METALLIC_ROUGHNESS)
    {
        // Metallic和Rougness在GLTF中是放在一起的，通过一个值或者贴图来混合
        perceptualRoughness = materialParams.mRoughnessFactor;
        metallic = materialParams.mMetallicFactor;
        if (materialParams.mPhysicalDescTexSet > -1)
        {
            // Roughness存储在G通道，Metallic存储在B通道，R通道存储额外的遮挡数据
            vec4 mrSample = texture(smpPhysicalDescMap, materialParams.mPhysicalDescTexSet == 0 ? inUV0 : inUV1);
            perceptualRoughness = mrSample.g * perceptualRoughness;
            metallic = mrSample.b * metallic;
        }
        else
        {
            perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);
            metallic = clamp(metallic, 0.0, 1.0);
        }
        if (materialParams.mBaseColorTexSet > -1)
        {
            baseColor = SRGBtoLINEAR(texture(smpColorMap, materialParams.mBaseColorTexSet == 0 ? inUV0 : inUV1));
        }
        else 
        {
            baseColor = materialParams.mBaseColorFactor;
        }
    }

    if (materialParams.mWorkFlow == PBR_WORKFLOW_SPECULAR_GLOSINESS)
    {
        // Values from specular glossiness workflow are converted to metallic roughness
		if (materialParams.mPhysicalDescTexSet > -1) 
        {
			perceptualRoughness = 1.0 - texture(smpPhysicalDescMap, materialParams.mPhysicalDescTexSet == 0 ? inUV0 : inUV1).a;
		} 
        else 
        {
			perceptualRoughness = 0.0;
		}

		const float epsilon = 1e-6;

		vec4 diffuse = SRGBtoLINEAR(texture(smpColorMap, inUV0));
		vec3 specular = SRGBtoLINEAR(texture(smpPhysicalDescMap, inUV0)).rgb;

		float maxSpecular = max(max(specular.r, specular.g), specular.b);

		// Convert metallic value from specular glossiness inputs
		metallic = ConvertMetallic(diffuse.rgb, specular, maxSpecular);

		vec3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - c_MinRoughness) / max(1 - metallic, epsilon)) * materialParams.mDiffuseFactor.rgb;
		vec3 baseColorSpecularPart = specular - (vec3(c_MinRoughness) * (1 - metallic) * (1 / max(metallic, epsilon))) * materialParams.mSpecularFactor.rgb;

		baseColor = vec4(mix(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic), diffuse.a);
    }

    baseColor *= inColor;

    diffuseColor = baseColor.rgb * (vec3(1.0) - F0);
    diffuseColor *= 1.0 - metallic;

    float alphaRoughness = perceptualRoughness * perceptualRoughness;

    vec3 specularColor = mix(F0, baseColor.rgb, metallic);

    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvR0 = specularColor.rgb;
    vec3 specularEnvR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    vec3 n = (materialParams.mNormalTexSet > -1) ? GetNormal() : normalize(inNormal);
    vec3 v = normalize(uboMat.mCamPos - inWorldPos);
    vec3 l = normalize(uboParams.mLigthDir.xyz);
    vec3 h = normalize(l + v);
    vec3 reflection = -normalize(reflect(v, n));
    reflection.y *= -1.0f;

    float NdotL = clamp(dot(n, l), 0.001, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    PBRInfo pbrInputs = PBRInfo(
        NdotL,
        NdotV,
        NdotH,
        LdotH,
        VdotH,
        perceptualRoughness,
        metallic,
        specularEnvR0,
        specularEnvR90,
        alphaRoughness,
        diffuseColor,
        specularColor
    );

    vec3 F = SpecularReflection(pbrInputs);
    float G = GeometryOcclusion(pbrInputs);
    float D = MicroFacetDistribution(pbrInputs);

    const vec3 u_LightColor = vec3(1.0);

    // 直射光部分
    vec3 diffuseContrib = (1.0 - F) * Diffuse(pbrInputs);
    vec3 specularContrib = F * G * D / (4.0 * NdotL * NdotV);
    // BRDF项
    vec3 color = NdotL * u_LightColor * (diffuseContrib + specularContrib);

    // IBL
    color += GetIBLContribution(pbrInputs, n, reflection);

    const float u_OcclusionStrength = 1.0f;

    if (materialParams.mOcclusionTexSet > -1)
    {
        float AO = texture(smpAOMap, (materialParams.mOcclusionTexSet == 0 ? inUV0 : inUV1)).r;
        color = mix(color, color * AO, u_OcclusionStrength);
    }

    const float u_EmissiveFactor = 1.0f;
    if (materialParams.mEmissiveTexSet > -1)
    {
        vec3 emssive = SRGBtoLINEAR(texture(smpEmissiveMap, materialParams.mEmissiveTexSet == 0 ? inUV0 : inUV1)).rgb * u_EmissiveFactor;
        color += emssive;
    }

    outColor = vec4(color, baseColor.a);

    // Shader inputs debug visualization
	if (uboParams.mDebugViewInputs > 0.0) 
    {
		int index = int(uboParams.mDebugViewInputs);
		switch (index) 
        {
			case 1:
				outColor.rgba = materialParams.mBaseColorTexSet > -1 ? texture(smpColorMap, materialParams.mBaseColorTexSet == 0 ? inUV0 : inUV1) : vec4(1.0f);
				break;
			case 2:
				outColor.rgb = (materialParams.mNormalTexSet > -1) ? texture(smpNormalMap, materialParams.mNormalTexSet == 0 ? inUV0 : inUV1).rgb : normalize(inNormal);
				break;
			case 3:
				outColor.rgb = (materialParams.mOcclusionTexSet > -1) ? texture(smpAOMap, materialParams.mOcclusionTexSet == 0 ? inUV0 : inUV1).rrr : vec3(0.0f);
				break;
			case 4:
				outColor.rgb = (materialParams.mEmissiveTexSet > -1) ? texture(smpEmissiveMap, materialParams.mEmissiveTexSet == 0 ? inUV0 : inUV1).rgb : vec3(0.0f);
				break;
			case 5:
				outColor.rgb = texture(smpPhysicalDescMap, inUV0).bbb;
				break;
			case 6:
				outColor.rgb = texture(smpPhysicalDescMap, inUV0).ggg;
				break;
		}
		outColor = SRGBtoLINEAR(outColor);
	}

	// PBR equation debug visualization
	// "none", "Diff (l,n)", "F (l,h)", "G (l,v,h)", "D (h)", "Specular"
	if (uboParams.mDebugViewInputs > 0.0) 
    {
		int index = int(uboParams.mDebugViewInputs);
		switch (index) 
        {
			case 1:
				outColor.rgb = diffuseContrib;
				break;
			case 2:
				outColor.rgb = F;
				break;
			case 3:
				outColor.rgb = vec3(G);
				break;
			case 4: 
				outColor.rgb = vec3(D);
				break;
			case 5:
				outColor.rgb = specularContrib;
				break;				
		}
	}
}