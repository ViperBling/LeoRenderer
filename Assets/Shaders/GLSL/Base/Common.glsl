#define PI 3.1415926535897932384626433832795
const float c_MinRoughness = 0.04;
const float PBR_WORKFLOW_METALLIC_ROUGHNESS = 0.0;
const float PBR_WORKFLOW_SPECULAR_GLOSINESS = 1.0f;

struct ShaderMaterial 
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 diffuseFactor;
    vec4 specularFactor;
    float workflow;
    int baseColorTextureSet;
    int physicalDescriptorTextureSet;
    int normalTextureSet;	
    int occlusionTextureSet;
    int emissiveTextureSet;
    float metallicFactor;	
    float roughnessFactor;	
    float alphaMask;	
    float alphaMaskCutoff;
    float emissiveStrength;
};

struct MaterialFactor
{
    vec4 albedo;
    vec3 diffuseColor;
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
    float metalness;
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

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
#define MANUAL_SRGB 1
#ifdef MANUAL_SRGB
	#ifdef SRGB_FAST_APPROXIMATION
	    vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	#else //SRGB_FAST_APPROXIMATION
	    vec3 bLess = step(vec3(0.0392, 0.0392, 0.0392),srgbIn.xyz);
	    vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
	#endif //SRGB_FAST_APPROXIMATION
	return vec4(linOut,srgbIn.w);;
#else //MANUAL_SRGB
	return srgbIn;
#endif //MANUAL_SRGB
}

// Gets metallic factor from specular glossiness workflow inputs 
float ConvertMetallic(vec3 diffuse, vec3 specular, float maxSpecular)
{
    float perceivedDiffuse = sqrt(0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b);
    float perceivedSpecular = sqrt(0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b);
    if (perceivedSpecular < c_MinRoughness) 
    {
        return 0.0;
    }
    float a = c_MinRoughness;
    float b = perceivedDiffuse * (1.0 - maxSpecular) / (1.0 - c_MinRoughness) + perceivedSpecular - 2.0 * c_MinRoughness;
    float c = c_MinRoughness - perceivedSpecular;
    float D = max(b * b - 4.0 * a * c, 0.0);
    return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
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

vec3 CalculateNormal(vec3 tangentNormal, vec3 inWorldPos, vec3 inNormal, vec2 inUV)
{
    vec3 q1 = dFdx(inWorldPos);
    vec3 q2 = dFdy(inWorldPos);
    vec2 st1 = dFdx(inUV);
    vec2 st2 = dFdy(inUV);

    vec3 N = normalize(inNormal);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    // vec3 T = normalize(inTangent.xyz);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

vec3 GetDirectionLight(MaterialFactor matFactor, PBRFactors pbrFactor)
{
    vec3 radiance = vec3(1.0) * 10.0f;

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