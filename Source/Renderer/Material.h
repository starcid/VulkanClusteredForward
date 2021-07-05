#ifndef __MATERIAL_H__
#define	__MATERIAL_H__

#include <string>
#include <tiny_obj_loader.h>

#include "Texture.h"

class Material
{
public:
	Material();
	virtual ~Material();

	void InitWithTinyMat(tinyobj::material_t* mat, std::string& basePath);

	void PrepareToDraw() { desc_sets_updated = false; }
	bool IsDescSetUpdated() { return desc_sets_updated; }
	void SetDescUpdated() { desc_sets_updated = true; }

	inline Texture* GetAmbientTexture() { return ambient_tex; }
	inline Texture* GetDiffuseTexture() { return diffuse_tex; }
	inline Texture* GetNormalTexture() { return bump_tex; }

	inline int GetHasAlbedoMap() { return has_albedo_map; }
	inline int GetHasNormalMap() { return has_normal_map; }

	inline int GetMatId() { return mat_id; }

protected:
	virtual void InitPlatform() {}

protected:
	tinyobj::material_t* tiny_mat;

	Texture* ambient_tex;
	Texture* diffuse_tex;
	Texture* specular_tex;
	Texture* specular_highlight_tex;
	Texture* bump_tex;
	Texture* displacement_tex;
	Texture* alpha_tex;
	Texture* reflection_tex;

	int has_albedo_map;
	int has_normal_map;

	int mat_id;
	static int mat_count;

	bool desc_sets_updated;
};

#endif // !__MATERIAL_H__
