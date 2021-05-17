#ifndef __TO_MODEL_H__
#define __TO_MODEL_H__

#include <tiny_obj_loader.h>
/*view https://github.com/syoyo/tinyobjloader for more informations*/

#include "Material.h"
#include "Model.h"

#include "GeoData.h"

class TOModel : public Model
{
public:
	TOModel();
	virtual ~TOModel();

	virtual bool LoadFromPath(std::string path);
	virtual void Draw();

	bool LoadTestData();	/// test usage

private:
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	/// material instance
	std::vector<Material*> material_insts;

	/// renderering data
	GeoData* geo_data;
};

#endif // !__TO_MODEL_H__
