#version 450

layout (location = 0) in vec3 inPos;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform samplerCube samplerEnv;

layout(push_constant) uniform PushConstans 
{
	layout (offset = 64) float deltaPhi;
	layout (offset = 68) float deltaTheta;
} pushConsts;

#define PI 3.1415926535897932384626433832795

void main()
{
    vec3 N = normalize(inPos);
    vec3 Up = vec3(0.0, 1.0, 0.0);
    vec3 Right = normalize(cross(Up, N));
    Up = cross(N, Right);

    const float TWO_PI = PI * 2.0;
    const float HALF_PI = PI * 0.5;

    vec3 color = vec3(0.0);
    uint sampleCount = 0u;
    for (float phi = 0.0; phi < TWO_PI; phi += pushConsts.deltaPhi)
    {
        for (float theta = 0.0; theta < HALF_PI; theta += pushConsts.deltaTheta)
        {
            vec3 tempVec = cos(phi) * Right + sin(phi) * Up;
            vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;
            color += texture(samplerEnv, sampleVector).rgb * cos(theta) * sin(theta);
            sampleCount++;
        }
    }
    outColor = vec4(PI * color / float(sampleCount), 1.0);
}