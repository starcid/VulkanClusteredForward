#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (std140, binding = 0) uniform TransformData {
    mat4 mvp;
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 proj_view;
    vec3 cam_pos;
} transform;

layout(std140, binding = 1) uniform MaterialData
{
    int has_albedo_map;
    int has_normal_map;
} material;

layout(std140, binding = 2) uniform PointLightData
{
    vec3 pos;
	float radius;
	vec3 intensity;
} pointLight;

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inTexcoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragTexCoord;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 tanLightPos;
layout(location = 4) out vec3 tanViewPos;
layout(location = 5) out vec3 tanFragPos;

void main() {
    gl_Position = transform.mvp * inPosition;
    fragColor = inColor;
    fragTexCoord = inTexcoord;
    fragPos = vec3(transform.model * inPosition);

    mat3 normalMatrix = transpose(inverse(mat3(transform.model)));
    vec3 T = normalize(vec3(normalMatrix * inTangent));
    vec3 N = normalize(vec3(normalMatrix * inNormal));
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    TBN = transpose(TBN);
    tanLightPos = TBN * pointLight.pos;
    tanViewPos  = TBN * transform.cam_pos;
    tanFragPos  = TBN * fragPos;
}