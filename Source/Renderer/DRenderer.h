#ifndef __D12_RENDERER_H__
#define	__D12_RENDERER_H__

#include <vector>
#include <set>
#include <array>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Renderer.h"

class D12Renderer : public Renderer
{
public:
	D12Renderer(GLFWwindow* win);
	virtual ~D12Renderer();

	virtual void RenderBegin();
	virtual void RenderEnd();
	virtual void Flush();
	virtual void WaitIdle();

	virtual void OnSceneExit();
};

#endif // !__D12_RENDERER_H__
