#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inUV;

layout (location = 0) smooth out vec3 outUV;

void main(){
	outUV = inUV;
	gl_Position = vec4(inPos, 1.0);
}
