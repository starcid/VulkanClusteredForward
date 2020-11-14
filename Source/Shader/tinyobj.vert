#version 450
#extension GL_ARB_separate_shader_objects : enable
#define MAX_LIGHT_NUM 16
layout (std140, binding = 0, set = 0) uniform TransformData {
    mat4 mvp;
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 proj_view;
    vec3 cam_pos;
    bool isClusteShading;
    uvec4 tileSizes;
    float zNear;
    float zFar;
    float scale;
    float bias;
} transform;

layout(std140, binding = 1, set = 0) uniform MaterialData
{
    int has_albedo_map;
    int has_normal_map;
} material;

layout(std140, binding = 2, set = 0) uniform PointLightData
{
    vec3 pos;
	float radius;
	vec3 color;
    uint enabled;
    float ambient_intensity;
	float diffuse_intensity;
	float specular_intensity;
    float attenuation_constant;
	float attenuation_linear;
	float attenuation_exp;
    vec2 padding;
} pointLight[MAX_LIGHT_NUM];

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inTexcoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

layout (location = 0) out Interpolants {
    vec3 fragColor;
    vec3 fragTexCoord;
    vec3 fragPos;
    vec3 tanViewPos;
    vec3 tanFragPos;
    vec3 tanLightPos[16];
} OUT;

void main() {
    gl_Position = transform.mvp * inPosition;
    OUT.fragColor = inColor;
    OUT.fragTexCoord = inTexcoord;
    OUT.fragPos = vec3(transform.model * inPosition);

    mat3 normalMatrix = transpose(inverse(mat3(transform.model))); /// maybe have scale
    vec3 T = normalize(vec3(normalMatrix * inTangent));
    vec3 N = normalize(vec3(normalMatrix * inNormal));
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    TBN = transpose(TBN);
    for(int i = 0; i < MAX_LIGHT_NUM; i++)
        OUT.tanLightPos[i] = TBN * pointLight[i].pos;
    OUT.tanViewPos  = TBN * transform.cam_pos;
    OUT.tanFragPos  = TBN * OUT.fragPos;
}