              #version 330 core
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
 #version 330 core
in vec2 uv;
in vec3 normal;
in vec4 lightSpaceVertex;
out vec4 fragColor;

uniform vec3 lightDirection;
uniform sampler2D shadowMap;

uniform sampler2D texAlbedo;
uniform sampler2D texNormal;

void main()
{
	vec3 albedoMap = texture(texAlbedo, uv).rgb;
	vec3 normalMap = texture(texNormal, uv).rgb * 2 - vec3(1);
	vec3 tangent = cross(vec3(0,0,1), normal);
	vec3 bitangent = cross(normal, tangent);
	mat4 ntb = mat4(
		tangent.x, bitangent.x, normal.x, 0,
		tangent.y, bitangent.y, normal.y, 0,
		tangent.z, bitangent.z, normal.z, 0,
		0, 0, 0, 1
	);
	vec3 nor = (vec4(normalMap, 0) * ntb).rgb;
	float light = dot(normal, -lightDirection);
	//light = 0.5f;

	// Shadow
	vec3 shadowCoords = lightSpaceVertex.xyz / lightSpaceVertex.w;
	shadowCoords = shadowCoords * 0.5 + 0.5;
	float closestDepth = texture(shadowMap, shadowCoords.xy).r;
	float currentDepth = lightSpaceVertex.z;
	float shadow = currentDepth > closestDepth ? 1.0 : 0.2;
	shadow = 1;

	vec4 shadowColor = texture(shadowMap, shadowCoords.xy);
	//albedoMap = vec3(shadowColor);
	//albedoMap = shadowCoords;

	//albedoMap = vec3(closestDepth);

	fragColor = vec4(albedoMap * (light * 0.5 + 0.5) * shadow, 0);
}
 