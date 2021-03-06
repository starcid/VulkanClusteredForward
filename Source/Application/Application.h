#ifndef __APPLICATION_H__
#define	__APPLICATION_H__

#include <iostream>

#include "../Renderer/GLMConfig.h"

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

class Scene;
class Camera;
class Renderer;
class Application
{
	static Application* inst;

	Application();

public:
	static Application* Inst()
	{
		if (inst == NULL)
		{
			inst = new Application();
		}
		return inst;
	}

	virtual ~Application();
	void CreateRenderer(GLFWwindow* window, const char* renderStr = NULL);

	bool MainLoop();
	void NextScene(Scene* scene);

	float GetWidth();
	float GetHeight();

	void SetRendererCamera(Camera* cam);
	Renderer* GetRenderer() { return renderer; }

	int GetControlState() { return control_state; }
	void SetControlState(int state) { control_state = state; }
	float GetScrollOffset() { return scroll_offset; }
	void SetScrollOffset(float offset) { scroll_offset = offset; }
	glm::vec2 GetMoveOffset() { return move_offset; }
	void SetMoveOffset(glm::vec2 offset) { move_offset = offset; }
	void SetKeyPressed(int key) 
	{
		if (key < 0)
			key_pressed = false;
		key_pressed = true;
		pressed_key = key;
	}
	int GetPressedKey()
	{
		if (!key_pressed)
			return -1;
		return pressed_key;
	}

	void SceneRender();

private:
	void SceneUpdate(float dt);

	void showFPS(GLFWwindow *pWindow);

private:
	Scene* current_scene;
	bool update_scene;

	Scene* next_scene;

	float delta_time;
	double last_time;

	double last_fps_time;
	int nb_frames;

	Renderer* renderer;

	GLFWwindow* current_window;

	int control_state;	// 0 normal 1 rotate 2 shift
	float scroll_offset;
	glm::vec2 move_offset;
	bool key_pressed;
	int pressed_key;
};

#endif // !__APPLICATION_H__
