#version 450

layout (location = 0) smooth in vec2 uv;

layout(set = 0, binding = 1) uniform sampler2D textureSampler;

layout (location = 0) out vec4 outColor;

void main(){
	 outColor = texture(textureSampler, uv);
}