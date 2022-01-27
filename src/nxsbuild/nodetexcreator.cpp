#include <vcg/space/rect_packer.h>

#include "nodetexcreator.h"
#include <QDebug>
#include <QPainter>

using namespace nx;
using namespace std;


unsigned int nextPowerOf2 ( unsigned int n ) {
	unsigned count = 0;

	// First n in the below condition
	// is for the case where n is 0
	if (n && !(n & (n - 1)))
		return n;

	while( n != 0) {
		n >>= 1;
		count += 1;
	}

	return 1 << count;
}

void closestPowerOf2(int32_t &p) {
	int32_t n = nextPowerOf2(p);
	if(n/(float)p > 1.40)
		n /= 2;
	p = n;
}


void closestPowerOf2(vcg::Point2i &p) {
	closestPowerOf2(p[0]);
	closestPowerOf2(p[1]);
	return;
	//find the smallest power of two that contains the box
	uint32_t W = nextPowerOf2(p[0]);
	uint32_t H = nextPowerOf2(p[1]);
	//compute scaling factore and compare with the shrinking factor.
	float scaling = W/(float)p[0];
	if(scaling > 4.0f/3.0f) {
		W /= 2; H /= 2;
	}
	p[0] = W;
	p[1] = H;
}

class UnionFind {
public:
	std::vector<int> parents;
	void init(int size) {
		parents.resize(size);
		for(int i = 0; i < size; i++)
			parents[i] = i;
	}

	int root(int p) {
		while(p != parents[p])
			p = parents[p] = parents[parents[p]];
		return p;
	}
	void link(int p0, int p1) {
		int r0 = root(p0);
		int r1 = root(p1);
		parents[r1] = r0;
	}
	int compact(std::vector<int> &node_component) { //change numbering of the connected components, return number
		node_component.resize(parents.size());
		std::map<int, int> remap;// inser root here and order them.
		for(int i = 0; i < parents.size(); i++) {
			int root = i;
			while(root != parents[root])
				root = parents[root];
			parents[i] = root;
			node_component[i] = remap.emplace(root, remap.size()).first->second;
		}
		return remap.size();
	}

};

/*
 * each face reference a material.
 * use material_map in materials to unify identical materials
 * materials can also be grouped if all parameters are the same (but not the same texture), using texture_map.
 *
 * Step 1: find connected components texture-wise
 * Step 2: build a vertex-to-tex index (actually material as unified in material_map)
 *		PROBLEM: how to deal with non textured materials?
 * Step 3: compute the box for each (material)
 *		 erase boxes assigned to non textured materials.
 * Step 4: enlarge box by 1 pix and compute origin and sizes
 *		PROBLEM: resoluition between specular/diuffuse maps could be different!
 * Step 5: pack boxes into a single tex
 *		we actually can pack only if unified by texture_map
 */

TextureGroupBuild NodeTexCreator::process(TMesh &mesh, int level) {
	TextureGroupBuild group;
	float &error = group.error;
	float &pixelXedge = group.pixelXEdge;

	std::vector<vcg::Box2f> boxes;
	std::vector<int> box_texture; //which texture each box belongs;
	std::vector<int> vertex_to_material(mesh.vert.size(), -1);
	std::vector<int> vertex_to_box;
	std::set<int> material_set;

	UnionFind components;
	components.init(mesh.vert.size());

	for(auto &face: mesh.face) {
		int v[3];
		assert(face.tex >= 0 && face.tex < materials->material_map.size());
		face.tex = materials->material_map[face.tex];
		for(int i = 0; i < 3; i++) {
			v[i] = face.V(i) - &*mesh.vert.begin();
			/* DEBUG
			TVertex &v0 = *face.V(i);
			TVertex &v1 = *face.V((i+1)%3);
			auto &p0 = v0.T().P();
			auto &p1 = v1.T().P();
			float dx = fabs(v0.T().P()[0] - v1.T().P()[0]);
			if(dx > 0.1)
				cout << "Cosa succede?\n"; */

			int &t = vertex_to_material[v[i]];
			if(t != -1 && t != face.tex)
				qDebug() << "Missing vertex replication across seams\n";
			t = face.tex;
			material_set.insert(t);
		}
		components.link(v[0], v[1]);
		components.link(v[0], v[2]);
		assert(components.root(v[1]) == components.root(v[2]));
	}
	int n_boxes = components.compact(vertex_to_box);

	for(auto &face: mesh.face) {
		for(int i = 0; i < 3; i++) {
			int v = face.V(i) - &*mesh.vert.begin();
			vertex_to_material[v] = face.tex;
		}
	}
	//assign all boxes to a tex (and remove boxes where the tex is -1

	//compute boxes
	boxes.resize(n_boxes);
	box_texture.resize(n_boxes, -1);
	for(size_t i = 0; i < mesh.vert.size(); i++) {
		int b = vertex_to_box[i];
		int tex = vertex_to_material[i];
		if(tex < 0) continue; //vertex not assigned.

		vcg::Box2f &box = boxes[b];
		box_texture[b] = tex;
		auto &t = mesh.vert[i].T().P();
		t[0] = std::min(1.0f, std::max(0.0f, t[0]));
		t[1] = std::min(1.0f, std::max(0.0f, t[1]));
		//t[0] = fmod(t[0], 1.0f);
		//t[1] = fmod(t[1], 1.0f);
		//		if(isnan(t[0]) || isnan(t[1]) || t[0] < 0 || t[1] < 0 || t[0] > 1 || t[1] > 1)
		//				cout << "T: " << t[0] << " " << t[1] << endl;
		if(t[0] != 0.0f || t[1] != 0.0f)
			box.Add(t);
	}
	//erase boxes assigned to no texture, and remap vertex_to_box
	int count = 0;
	std::vector<int> remap(mesh.vert.size(), -1);
	for(int i = 0; i < n_boxes; i++) {
		int tex = box_texture[i];
		if(!materials->at(tex).nmaps)
			continue;

		boxes[count] = boxes[i];
		box_texture[count] = box_texture[i];
		remap[i] = count++;
	}
	boxes.resize(count);
	box_texture.resize(count);
	for(int &b: vertex_to_box)
		b = remap[b];


	//enlarge box by 1 pix and compute origin and sizes
	std::vector<vcg::Point2i> sizes(boxes.size());
	std::vector<vcg::Point2i> origins(boxes.size());
	for(size_t b = 0; b < boxes.size(); b++) {
		auto &box = boxes[b];
		int tex = box_texture[b];

		//enlarge 1 pixel
		//herre we assume all the textures share the same w, h and parametrization
		int32_t first_tex = atlas->getTextureId(materials->at(tex).textures[0]);
		float w = atlas->width(first_tex, level); //img->size().width();
		float h = atlas->height(first_tex, level); //img->size().height();
		float px = 1/(float)w;
		float py = 1/(float)h;
		box.Offset(vcg::Point2f(px, py));
		//snap to higher pix (clamped by 0 and 1 anyway)
		vcg::Point2i &size = sizes[b];
		vcg::Point2i &origin = origins[b];
		origin[0] = std::max(0.0f, floor(box.min[0]/px));
		origin[1] = std::max(0.0f, floor(box.min[1]/py));
		if(origin[0] >= w) origin[0] = w-1;
		if(origin[1] >= h) origin[1] = h-1;

		size[0] = std::min(w, ceil(box.max[0]/px)) - origin[0];
		size[1] = std::min(h, ceil(box.max[1]/py)) - origin[1];
		if(size[0] <= 0) size[0] = 1;
		if(size[1] <= 0) size[1] = 1;
	}

	//unify materials by texture.
	std::set<int32_t> final_materials;
	for(int32_t m: material_set)
		final_materials.insert(materials->texture_map[m]);
	//TODO CHECK this!
	//if(final_materials.size() > 1)
	//	throw "More than one material not supported at the moment. Unless they differ only for the texture";


	//Here we should slit the boxes by material, then pack!
	//Pack boxes
	std::vector<vcg::Point2i> mapping;
	vcg::Point2i maxSize(4096, 4096);
	vcg::Point2i packedSize;
	bool success = false;
	for(int i = 0; i < 5; i++, maxSize[0]*= 2, maxSize[1]*= 2) {
		if(sizes.size() == 0) { //no texture!
			packedSize = vcg::Point2i(1, 1);
			success = true;
			break;
		}
		bool too_large = false;
		for(auto s: sizes) {
			if(s[0] > maxSize[0] || s[1] > maxSize[1])
				too_large = true;
		}
		if(too_large) { //TODO the packer should simply return false
			continue;
		}
		mapping.clear(); //TODO this should be done inside the packer
		success = vcg::RectPacker<float>::PackInt(sizes, maxSize, mapping, packedSize);
		if(success)
			break;
	}
	if(!success) {
		cerr << "Failed packing: the texture in a single nexus node would be > 16K\n";
		cerr << "Try to reduce the size of the nodes using -t (default is 4096)";
		exit(0);
	}

	vcg::Point2i finalSize = packedSize;

//	if (createPowTwoTex) {

//		finalSize[ 0 ] = (int) nextPowerOf2( finalSize[ 0 ] );
//		finalSize[ 1 ] = (int) nextPowerOf2( finalSize[ 1 ] );
//	}

	if (createPowTwoTex) {
		//could use bigger size of 2 or separated closest
		closestPowerOf2(finalSize);
	}


	//temporary: here we sould just create image and split by materials for each texture_map material
	int32_t material_id = *final_materials.begin();
	BuildMaterial &material = materials->at(material_id);

	group.material = material_id;
	group.resize(material.nmaps);
	for(int8_t i = 0; i < material.nmaps; i++) {
		group[i] = QImage(packedSize[0], packedSize[1], QImage::Format_RGB32);
		group[i].fill(QColor(127, 127, 127));
	}

	//Compute uv using origins and mapping

	//size of a pixel in texture coordinates
	float pdx = 1/(float)packedSize[0];
	float pdy = 1/(float)packedSize[1];

	for(size_t i = 0; i < mesh.vert.size(); i++) {
		auto &p = mesh.vert[i];
		auto &uv = p.T().P();
		int b = vertex_to_box[i];
		if(b == -1) {
			uv = vcg::Point2f(0.0f, 0.0f);
			continue;
		}
		vcg::Point2i &o = origins[b]; //of the box in the original texture (in pixels)
		vcg::Point2i m = mapping[b]; //position of the box in the new texture (in pixels)

		int mat = box_texture[b];
		int32_t first_tex = atlas->getTextureId(materials->at(mat).textures[0]);

		float w = atlas->width(first_tex, level);
		float h = atlas->height(first_tex, level);
		float px = 1/(float)w;
		float py = 1/(float)h;

		if(uv[0] < 0.0f)
			uv[0] = 0.0f;
		if(uv[1] < 0.0f)
			uv[1] = 0.0f;

		//dx and dy coordinate relative to the box in pixels.
		float dx = uv[0]/px - o[0];
		float dy = uv[1]/py - o[1];
		if(dx < 0.0f) dx = 0.0f;
		if(dy < 0.0f) dy = 0.0f;

		uv[0] = (m[0] + dx)*pdx; //how many pixels from the origin
		uv[1] = (m[1] + dy)*pdy; //how many pixels from the origin

		assert(!isnan(uv[0]));
		assert(!isnan(uv[1]));
	}

	//compute error:
	float pdx2 = pdx*pdx;
	error = 0.0;
	pixelXedge = 0.0f;
	float avgerror = 0.0f;
	for(auto &face: mesh.face) {
		for(int k = 0; k < 3; k++) {
			int j = (k==2)?0:k+1;

			float edge = vcg::SquaredNorm(face.P(k) - face.P(j));
			float pixel = vcg::SquaredNorm(face.V(k)->T().P() - face.V(j)->T().P())/pdx2;
			pixelXedge += pixel;
			if(pixel > 10) pixel = 10;
			if(pixel < 1)
				error += edge;
			else
				error += edge/pixel;
		}
	}
	pixelXedge = sqrt(pixelXedge/mesh.face.size()*3);
	error = sqrt(error/mesh.face.size()*3);

	double areausage = 0.0;
	//compute area waste
	for(int i = 0; i < mesh.face.size(); i++) {
		auto &face = mesh.face[i];
		int b = vertex_to_box[face.V(0) - &(mesh.vert[0])];
		vcg::Point2i &o = origins[b];
		vcg::Point2i m = mapping[b];
		auto V0 = face.V(0)->T().P();
		auto V1 = face.V(1)->T().P();
		auto V2 = face.V(2)->T().P();
		areausage += (V2 - V0)^(V2 - V1)/2;

	}
	//cout << "Area: " << (int)(100*areausage) << "% --- " << (int)areausage*finalSize[0]*finalSize[1] << " vs: " << finalSize[0]*finalSize[1] << "\n";

	for(int8_t t =0; t < material.nmaps; t++) {
		assert(t < group.size());
		//	static int boxid = 0;
		//parentesys needed to create a scope for the painter.
		{
			QPainter painter(&group[t]);
			//convert tex coordinates using mapping
			for(int i = 0; i < boxes.size(); i++) {

				/*		vcg::Color4b  color;
			color[2] = ((boxid % 11)*171)%63 + 63;
			//color[1] = 255*log2((i+1))/log2(nexus->header.n_patches);
			color[1] = ((boxid % 7)*57)%127 + 127;
			color[0] = ((boxid % 16)*135)%127 + 127; */

				int source_material = box_texture[i];
				int tex = atlas->getTextureId(materials->at(source_material).textures[t]);
				vcg::Point2i &o = origins[i];
				vcg::Point2i &s = sizes[i];

				assert(tex >= 0 && tex < atlas->pyramids.size());
				QImage rect = atlas->read(tex, level, QRect(o[0], o[1], s[0], s[1]));
				painter.drawImage(mapping[i][0], mapping[i][1], rect);
			}

#ifdef PAINT_TEX_TRIANGLES
			painter.setPen(QColor(255,0,255));
			for(int i = 0; i < mesh.face.size(); i++) {
				auto &face = mesh.face[i];
				int b = vertex_to_box[face.V(0) - &(mesh.vert[0])];
				vcg::Point2i &o = origins[b];
				vcg::Point2i m = mapping[b];

				for(int k = 0; k < 3; k++) {
					int j = (k==2)?0:k+1;
					auto V0 = face.V(k);
					auto V1 = face.V(j);
					float x0 = V0->T().P()[0]/pdx; //how many pixels from the origin
					float y0 = V0->T().P()[1]/pdy; //how many pixels from the origin
					float x1 = V1->T().P()[0]/pdx; //how many pixels from the origin
					float y1 = V1->T().P()[1]/pdy; //how many pixels from the origin
					painter.drawLine(x0, y0, x1, y1);
				}
			}
#endif
		}




		//		painter.fillRect(mapping[i][0], mapping[i][1], s[0], s[1], QColor(color[0], color[1], color[2]));
		//		boxid++;

		if (createPowTwoTex) {
			QImage final(finalSize[0], finalSize[1], QImage::Format_RGB32);
			final.fill(QColor(127, 127, 127));
			QPainter painter(&final);
			painter.drawImage(QRectF(0, 0, finalSize[0], finalSize[1]), group[t],
				QRectF(0, 0, packedSize[0], packedSize[1]));

			group[t] = final;
		}







		/*
		for(int i = 0; i < mesh.vert.size(); i++) {
			auto &p = mesh.vert[i];
			int b = vertex_to_box[i];
			vcg::Point2i &o = origins[b];
			vcg::Point2i m = mapping[b];


			float x = p.T().P()[0]/pdx; //how many pixels from the origin
			float y = p.T().P()[1]/pdy; //how many pixels from the origin
			painter.setPen(QColor(255,0,255));
			painter.drawEllipse(x-10, y-10, 20, 20);
			painter.drawPoint(x, y);
		} */
		group[t] = group[t].mirrored();
#ifdef SAVE_NODE_TEX
		static int imgcount = 0;
		group[t].save(QString("OUT_test_%1_%2.jpg").arg(imgcount++).arg(t));
#endif
	}

	//static int imgcount = 0;
	//image.save(QString("OUT_test_%1.jpg").arg(imgcount++));
	return group;
}