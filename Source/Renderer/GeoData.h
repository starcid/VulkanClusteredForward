/*
	Geometry data
*/

#ifndef __GEO_DATA_H__
#define __GEO_DATA_H__

#include <iostream>
#include <stdlib.h>
#include <vector>

#include "Renderer.h"

#include <tiny_obj_loader.h>
/*view https://github.com/syoyo/tinyobjloader for more informations*/

class GeoData
{
public:
	GeoData(Renderer* renderer) :m_pRenderer(renderer){}
	virtual ~GeoData() {}

	virtual void initTestData() = 0;
	virtual void initTinyObjData(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, std::vector<tinyobj::material_t>& materials) = 0;

protected:
	Renderer* m_pRenderer;

	/// <summary>
	///  test data
	/// </summary>
	static const std::vector<Vertex> vertices;
};

#endif // !__GEO_DATA_H__
