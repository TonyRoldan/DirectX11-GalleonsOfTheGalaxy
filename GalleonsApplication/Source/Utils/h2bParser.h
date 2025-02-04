#ifndef _H2BPARSER_H_
#define _H2BPARSER_H_

#include <fstream>
#include <vector>
#include <set>

#include "../Precompiled.h"

namespace H2B {

#pragma pack(push,1)

	struct Vector { 
		float x, y, z; 
	};

	struct Vertex { 
		Vector pos, uvw, nrm; 
	};

	struct alignas(void*) Attributes {
		Vector Kd; float d;
		Vector Ks; float Ns;
		Vector Ka; float sharpness;
		Vector Tf; float Ni;
		Vector Ke; unsigned illum;
	};

	struct Batch {
		unsigned indexCount, indexOffset;
	};

#pragma pack(pop)

	struct Material {
		Attributes attrib;
		const char* name;
		const char* mapKd;
		const char* mapKs;
		const char* mapKa; 
		const char* mapKe;
		const char* mapNs;
		const char* mapD;
		const char* disp;
		const char* decal;
		const char* bump;
		const void* padding[2];
	};

	struct Mesh {
		const char* name;
		Batch drawInfo;
		unsigned materialIndex;
	};

	struct Light {
		GW::MATH::GMATRIXF world;
		GW::MATH::GVECTORF worldPos;
		GW::MATH::GVECTORF color;
		GW::MATH::GVECTORF direction;
		float radius;
		float innerCone;
		float outerCone;
	};

	class Parser
	{

		std::set<std::string> file_strings;

	public:

		char version[4];
		unsigned vertexCount;
		unsigned indexCount;
		unsigned materialCount;
		unsigned meshCount;
		std::vector<Vertex> vertices;
		std::vector<unsigned> indices;
		std::vector<Material> materials;
		std::vector<Batch> batches;
		std::vector<Mesh> meshes;

		bool Parse(const char* _h2bPath)
		{
			Clear();
			std::ifstream file;
			char buffer[260] = { 0, };
			file.open(_h2bPath,	std::ios_base::in | 
								std::ios_base::binary);

			if (file.is_open() == false)
				return false;

			file.read(version, 4);

			if (version[1] < '1' || version[2] < '9' || version[3] < 'd')
				return false;

			file.read(reinterpret_cast<char*>(&vertexCount), 4);
			file.read(reinterpret_cast<char*>(&indexCount), 4);
			file.read(reinterpret_cast<char*>(&materialCount), 4);
			file.read(reinterpret_cast<char*>(&meshCount), 4);
			vertices.resize(vertexCount);
			file.read(reinterpret_cast<char*>(vertices.data()), 36 * vertexCount);
			indices.resize(indexCount);
			file.read(reinterpret_cast<char*>(indices.data()), 4 * indexCount);
			materials.resize(materialCount);

			for (int i = 0; i < materialCount; ++i) {
				file.read(reinterpret_cast<char*>(&materials[i].attrib), 80);
				for (int j = 0; j < 10; ++j) {
					buffer[0] = '\0';
					*((&materials[i].name) + j) = nullptr;
					file.getline(buffer, 260, '\0');
					if (buffer[0] != '\0') {
						auto last = file_strings.insert(buffer);
						*((&materials[i].name) + j) = last.first->c_str();
					}
				}
			}

			batches.resize(materialCount);
			file.read(reinterpret_cast<char*>(batches.data()), 8 * materialCount);
			meshes.resize(meshCount);

			for (int i = 0; i < meshCount; ++i) {
				buffer[0] = '\0';
				meshes[i].name = nullptr;
				file.getline(buffer, 260, '\0');

				if (buffer[0] != '\0') {
					auto last = file_strings.insert(buffer);
					meshes[i].name = last.first->c_str();
				}

				file.read(reinterpret_cast<char*>(&meshes[i].drawInfo), 8);
				file.read(reinterpret_cast<char*>(&meshes[i].materialIndex), 4);
			}

			return true;
		}
		void Clear()
		{
			*reinterpret_cast<unsigned*>(version) = 0;
			file_strings.clear();
			vertices.clear();
			indices.clear();
			materials.clear();
			batches.clear();
			meshes.clear();
		}
	};
}
#endif