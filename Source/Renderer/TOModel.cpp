#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Renderer/VRenderer.h"
#include "Camera.h"
#include "Application/Application.h"
#include "TOModel.h"

const std::vector<Vertex> vertices = {
	{{0.0f, -2.5f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
	{{2.5f, 2.5f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	{{-2.5f, 2.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

	{{0.0f, 0.0f, 0.01f, 1.0f}, {1.0f, 0.0f, 0.0f}},
	{{5.0f, -2.5f, 0.01f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	{{-2.5f, -2.5f, 0.01f, 1.0f}, {0.0f, 0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 3, 5, 4
};

TOModel::TOModel()
{
	Renderer* renderer = Application::Inst()->GetRenderer();
	geo_data = renderer->CreateGeoData();
}

TOModel::~TOModel()
{
	for (int i = 0; i < material_insts.size(); i++)
	{
		delete material_insts[i];
	}
	material_insts.clear();

	delete geo_data;
}

bool TOModel::LoadTestData()
{
	geo_data->initTestData();

	return true;
}

void TOModel::Draw()
{
	Renderer* renderer = Application::Inst()->GetRenderer();
	renderer->UpdateTransformMatrix(this);
	renderer->Draw(geo_data, material_insts);
}

bool TOModel::LoadFromPath(std::string path)
{
	std::string err;
	std::string warn;

	///std::string filename = "Data/sponza_full/sponza.obj";
	std::string basePath = "";
	int slashIdx = path.find_last_of("/");
	if (slashIdx >= 0)
	{
		basePath = path.substr(0, slashIdx);
	}

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), basePath.c_str()))
	{
		throw std::runtime_error(err);
		return false;
	}

	/// material instances
	for (int i = 0; i < materials.size(); i++)
	{
		Material* mat = new Material();
		mat->InitWithTinyMat(&materials[i], basePath);
		material_insts.push_back(mat);
	}

	geo_data->initTinyObjData(attrib, shapes, materials);

	return true;
}