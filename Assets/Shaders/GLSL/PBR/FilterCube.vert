#version 450

layout (location = 0) in vec3 inPos;

layout (push_constant) uniform PushConstant 
{
    layout (offset = 0) mat4 mvp;
} pushConst;

layout (location = 0) out vec3 outUVW;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    outUVW = inPos;
    gl_Position = pushConst.mvp * vec4(inPos.xyz, 1.0);
}