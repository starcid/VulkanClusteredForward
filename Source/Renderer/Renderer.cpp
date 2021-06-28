#include "Renderer.h"

#include "Texture.h"
#include "Light.h"

Renderer::Renderer(GLFWwindow* win) 
	: window(win) 
{
	glfwGetWindowSize(win, &winWidth, &winHeight); 
	camera = NULL; 
	SetClearColor(0, 0, 0, 1);
	SetClearDepth(1, 0);
	default_tex = NULL;
	isRenderBegin = false;
}

Renderer::~Renderer() 
{
}

void Renderer::SetDefaultTex(std::string& path)
{
	default_tex = new Texture(path);
}

void Renderer::AddLight(PointLight* light)
{
	PointLightData lightData;
	lightData.color = light->GetColor();
	lightData.pos = light->GetPosition();
	lightData.radius = light->GetRadius();
	lightData.enabled = 1;
	lightData.ambient_intensity = light->GetAmbientIntensity();
	lightData.diffuse_intensity = light->GetDiffuseIntensity();
	lightData.specular_intensity = light->GetSpecularIntensity();
	lightData.attenuation_constant = light->GetAttenuationConstant();
	lightData.attenuation_linear = light->GetAttenuationLinear();
	lightData.attenuation_exp = light->GetAttenuationExp();
	light_infos.push_back(lightData);
}

void Renderer::ClearLight()
{
	light_infos.clear();
}