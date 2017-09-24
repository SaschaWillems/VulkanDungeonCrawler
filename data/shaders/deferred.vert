#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inUV;
//layout (location = 4) in vec3 inTangent;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
	vec4 instancePos[3];
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outUV;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec3 outWorldPos;
layout (location = 4) out vec3 outTangent;

layout(push_constant) uniform PushConsts {
	vec3 objPos;
} pushConsts;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	vec3 locPos = vec3(ubo.model * vec4(inPos.xyz, 1.0));
	vec4 tmpPos = vec4(locPos + pushConsts.objPos, 1.0);

	gl_Position = ubo.projection * ubo.view * ubo.model * tmpPos;
	
	outUV = inUV;
	outUV.t = 1.0 - outUV.t;

	// Vertex position in world space
	outWorldPos = vec3(ubo.model * tmpPos);
	// GL to Vulkan coord space
	outWorldPos.y = -outWorldPos.y;
	
	// Normal in world space
	mat3 mNormal = transpose(inverse(mat3(ubo.model)));
	outNormal = mNormal * normalize(inNormal);	
	//outTangent = mNormal * normalize(inTangent);
	
	// Currently just vertex color
	outColor = vec3(1.0);
}
