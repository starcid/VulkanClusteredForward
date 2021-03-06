#include "GeoData.h"
#include "Renderer.h"

const std::vector<Vertex> GeoData::test_vertices = {
	{{0.0f, -2.5f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
	{{2.5f, 2.5f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
	{{-2.5f, 2.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},

	{{0.0f, 0.0f, 0.01f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
	{{5.0f, -2.5f, 0.01f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
	{{-2.5f, -2.5f, 0.01f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
};

const std::vector<int> GeoData::test_indices = {
	0, 1, 2, 3, 5, 4
};

int GeoData::CalculateHash(int idx1, int idx2, int idx3)
{
	return (idx1 * 31 + idx2) * 31 + idx3;
}