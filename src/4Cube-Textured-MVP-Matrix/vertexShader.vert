#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;

layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

layout (location = 0) smooth out vec2 outUV;

void main(){
	outUV = inUV;
	gl_Position = ubo.mvp * vec4(inPos, 1.0);
}
