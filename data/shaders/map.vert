#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;

layout (location = 0) out vec3 outColor;

layout (binding = 0) uniform UBO {
	mat4 projection;
	mat4 model;
} ubo;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() 
{
	gl_Position = ubo.projection * ubo.model * vec4(inPos.xyz, 1.0);
	outColor = inColor;
}
