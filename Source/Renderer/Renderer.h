#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <iostream>
#include <vector>

#include "GLMConfig.h"

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

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
	glm::vec4 color;
	glm::vec4 texcoord;
	glm::vec4 normal;
	glm::vec4 tangent;
};

/// meshlet for mesh shading
struct Meshlet {
	glm::uint vertexCount;
	glm::uint primCount;
	glm::uint vertexBegin;
	glm::uint primBegin;
};

/// transform data for shader
struct TransformData {
	glm::mat4x4 mvp;
	glm::mat4x4 model;
	glm::mat4x4 view;
	glm::mat4x4 proj;
	glm::mat4x4 proj_view;
	glm::vec3 cam_pos;	/// obj space
	bool isClusteShading;
	glm::uvec4 tileSizes;
	float zNear;
	float zFar;
	float scale;
	float bias;
	glm::vec4 light_pos[MAX_LIGHT_NUM];	/// obj_space
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
class GeoData;
class TransformEntity;
class Material;
class Texture;
class PointLight;
class Renderer
{
public:
	const int MAX_MATERIAL_NUM = 50;
	const int MAX_MODEL_NUM = 1000;

	enum Type
	{
		Vulkan = 0,
		DX12 = 1,
	};
	static Renderer::Type GetType() { return renderer_type; }

	Renderer(GLFWwindow* win);
	virtual ~Renderer();

	virtual void RenderBegin() { isRenderBegin = true; };
	virtual void RenderEnd() { isRenderBegin = false; };
	virtual void Flush() = 0;
	virtual void WaitIdle() = 0;

	virtual GeoData* CreateGeoData() = 0;
	virtual void Draw(GeoData* geoData, std::vector<Material*>& mats) = 0;

	virtual void UpdateCameraMatrix() = 0;
	virtual void UpdateTransformMatrix(TransformEntity* transform) = 0;

	virtual void OnSceneExit() = 0;

	virtual void AddLight(PointLight* light);
	virtual void ClearLight();

	void SetCamera(Camera* c) { camera = c; }
	Camera* GetCamera() { return camera; }

	void SetDefaultTex(std::string& path);

	inline void SetClearColor(float r, float g, float b, float a) 
	{ 
		clear_color[0] = r; 
		clear_color[1] = g;
		clear_color[2] = b;
		clear_color[3] = a;
	}
	inline void SetClearDepth(float depth, uint32_t stencil) { clear_depth = depth, clear_stencil = stencil; }

protected:
	Camera* camera;
	GLFWwindow* window;
	int winWidth;
	int winHeight;

	Texture* default_tex;
	float clear_color[4];
	float clear_depth;
	uint32_t clear_stencil;

	bool isRenderBegin;

	std::vector<PointLightData> light_infos;

	static Renderer::Type renderer_type;
};

#endif // !__RENDERER_H__
