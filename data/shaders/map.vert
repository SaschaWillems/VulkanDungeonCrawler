#version 450

layout (location = 0) in vec3 inPos;

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
}
