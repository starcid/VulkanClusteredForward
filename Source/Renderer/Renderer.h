#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <iostream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#define MAX_LIGHT_NUM 16
#define CLUSTE_X 16
#define CLUSTE_Y 9
#define CLUSTE_Z 24
#define CLUSTE_NUM (CLUSTE_X * CLUSTE_Y * CLUSTE_Z)
#define MAX_MESH_SHADER_PRIMITIVE 126
#define MAX_MESH_SHADER_VERTICES 64

/// one buffers(simple first)
struct Vertex {
	glm::vec4 pos;
	glm::vec3 color;
	glm::vec3 texcoord;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec3 bitangent;
};

/// std 430 usgae
struct Vertex_Storage {
	glm::vec4 pos;
	glm::vec3 color;
	float padding1;
	glm::vec3 texcoord;
	float padding2;
	glm::vec3 normal;
	float padding3;
	glm::vec3 tangent;
	float padding4;
	glm::vec3 bitangent;
	float padding5;
};

/// meshlet for mesh shading
struct Meshlet {
	glm::uint vertices[MAX_MESH_SHADER_VERTICES];
	glm::uint vertex_count;
};

/// transform data for shader
struct TransformData {
	glm::mat4x4 mvp;
	glm::mat4x4 model;
	glm::mat4x4 view;
	glm::mat4x4 proj;
	glm::mat4x4 proj_view;
	glm::vec3 cam_pos;
	bool isClusteShading;
	glm::uvec4 tileSizes;
	float zNear;
	float zFar;
	float scale;
	float bias;
};

/// material flag for shader
struct MaterialData {
	int has_albedo_map;
	int has_normal_map;
};

/// light structure for shader
struct PointLightData {
	glm::vec3 pos;
	float radius;
	glm::vec3 color;
	glm::uint enabled;
	float ambient_intensity;
	float diffuse_intensity;
	float specular_intensity;
	float attenuation_constant;
	float attenuation_linear;
	float attenuation_exp;
	glm::vec2 padding;
};

/// cluste AABB
struct VolumeTileAABB {
	glm::vec4 minPoint;
	glm::vec4 maxPoint;
};

/// screen to view for cluste calculate
struct ScreenToView
{
	glm::mat4 inverseProjection;
	glm::mat4 viewMatrix;
	glm::uvec4 tileSizes;
	glm::uvec2 screenDimensions;
	float zNear;
	float zFar;
};

/// light grid
struct LightGrid {
	glm::uint offset;
	glm::uint count;
};

class Model;
class Camera;
class Renderer
{
public:
	const int MAX_MATERIAL_NUM = 50;
	const int MAX_MODEL_NUM = 1000;

	Renderer(GLFWwindow* win) : window(win) { glfwGetWindowSize(win, &winWidth, &winHeight); camera = NULL; }
	virtual ~Renderer() {}

	virtual void RenderBegin() = 0;
	virtual void RenderEnd() = 0;
	virtual void Flush() = 0;
	virtual void WaitIdle() = 0;

	virtual void OnSceneExit() = 0;

	void SetCamera(Camera* c) { camera = c; }
	Camera* GetCamera() { return camera; }

protected:
	Camera* camera;
	GLFWwindow* window;
	int winWidth;
	int winHeight;
};

#endif // !__RENDERER_H__
