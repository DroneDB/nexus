#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <cstdint>
#include <fstream>
#include <string>

#include <vcg/math/matrix44.h>

#include "../common/traversal.h"

class Extractor: public nx::Traversal {
public:
	//compression options
	int coord_q;      //step of coord quantizations (a power of 2: 0 means 1 unit)
	double error_factor; //unused error for internal vertices
	int color_bits[4];
	int norm_bits;
	float tex_step; //in pixel units

	Extractor(nx::NexusData *nexus);

	int levelCount(); //return number of levels in the nexus (assuming it's a constant depth tree)
	void setMatrix(vcg::Matrix44f m);
	void selectBySize(uint64_t size);
	void selectByLevel(int level); //select up to level included (zero based)
	void selectByError(float error);
	void selectByTriangles(uint64_t triangles);
	void dropLevel();

	void save(const std::string &output, nx::Signature &signature);
	void savePly(const std::string &filename);
	void saveStl(const std::string &filename);

	void saveUnifiedPly(const std::string &filename);

	virtual float nodeError(uint32_t node, bool &visible);
	nx::Traversal::Action expand(HeapNode h);

protected:

	bool transform;
	vcg::Matrix44f matrix;
	uint64_t max_size, current_size;
	float min_error, current_error;
	int max_level = -1, nlevels = 0;
	uint64_t max_triangles, current_triangles;

	uint32_t pad(uint32_t s);
	int sinkDistance(int node);

	void compress(std::ofstream &file, nx::Signature &signature, nx::Node &node, nx::NodeData &data, nx::Patch *patches);
	//counts vertices and faces in the selected mesh.
	void countElements(uint64_t &n_vertices, uint64_t &n_faces);
};

#endif // EXTRACTOR_H
