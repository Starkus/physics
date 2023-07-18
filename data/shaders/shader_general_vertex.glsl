#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec3 nor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
out vec2 uv;
out vec3 normal;
out vec4 lightSpaceVertex;

void main()
{
	gl_Position = projection * view * model * vec4(pos, 1.0);
	normal = normalize((model * vec4(nor, 0.0)).xyz);
	uv = vec2(inUv.x, -inUv.y);
	lightSpaceVertex = lightSpaceMatrix * model * vec4(pos, 1.0);
}
