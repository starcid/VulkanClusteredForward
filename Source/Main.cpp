#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Renderer/GLMConfig.h"

#include <iostream>
#include <exception>
#include <conio.h>

#include "Application/Application.h"
#include "Renderer/VRenderer.h"
#include "Scene/SampleScene.h"

int main(int argc, char* argv[]) 
{
	char* renderer = NULL;
	for (int i = 1; i < argc; i++)
	{
		if (argv[i] != NULL)
		{
			/// check renderer
			if ((renderer = strstr(argv[i], "renderer=")) != NULL)
			{
				renderer = argv[i] + strlen("renderer=");
			}
		}
	}

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	
	GLFWwindow* window = glfwCreateWindow((int)Application::Inst()->GetWidth(), (int)Application::Inst()->GetHeight(), "Clustered Forward", nullptr, nullptr);
	try {
		Application::Inst()->CreateRenderer(window, renderer);
		Application::Inst()->NextScene(new SampleScene());
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			if (!Application::Inst()->MainLoop())
				break;
		}
	}
	catch (std::exception& e)
	{
		std::cout << "Catch exception: " << e.what() << std::endl;
		std::cout << "Press any key to exit." << std::endl;
		while (!_getch()) {}
	}
	delete Application::Inst();
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}