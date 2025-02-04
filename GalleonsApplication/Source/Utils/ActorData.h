// This is a sample of how to load a level in a data oriented fashion.
// Feel free to use this code as a base and tweak it for your needs.
// *NEW* The new version of this loader saves blender names.
#pragma once

#include "h2bParser.h"
#include <string>
#include <filesystem>

// This reads .h2b files (which are optimized binary .obj+.mtl files) for actor game objects.
class ActorData 
{

public:

	struct Model
	{
		const char* fileName;
		unsigned vertexCount, indexCount, materialCount, meshCount;
		unsigned vertexStart, indexStart, materialStart, meshStart, batchStart;
		unsigned colliderIndex;
		unsigned int texId;
		unsigned int transformStart;
		// Object aligned bounding box data: LBN, LTN, LTF, LBF, RBN, RTN, RTF, RBF
		// F,N = front, back	L,R = left, right	T,B = top, bottom
		GW::MATH2D::GVECTOR3F boundry[8];
		mutable std::vector<std::string> blenderNames;

		// Converts the vec3 boundries to an OBB
		GW::MATH::GOBBF ComputeOBB() const
		{
			GW::MATH::GOBBF out = {
				GW::MATH::GIdentityVectorF,
				GW::MATH::GIdentityVectorF,
				GW::MATH::GIdentityQuaternionF // initally unrotated (local space)
			};

			out.center.x = (boundry[0].x + boundry[4].x) * 0.5f;
			out.center.y = (boundry[0].y + boundry[1].y) * 0.5f;
			out.center.z = (boundry[0].z + boundry[2].z) * 0.5f;
			out.extent.x = std::fabsf(boundry[0].x - boundry[4].x) * 0.5f;
			out.extent.y = std::fabsf(boundry[0].y - boundry[1].y) * 0.5f;
			out.extent.z = std::fabsf(boundry[0].z - boundry[2].z) * 0.5f;
			return out;
		}
	};

	// swaps string pointers for loaded texture offsets
	struct MaterialTextures 
	{
		unsigned int albedoIndex, roughnessIndex, metalIndex, normalIndex;
	};

	// *NEW* Used to track individual objects in blender
	struct BlenderObject 
	{
		const char* blenderName; // *NEW* name of model straight from blender (FLECS)
		unsigned int modelIndex, transformIndex;
	};


	std::vector<H2B::Vertex> vertices;
	std::vector<unsigned> indices;

	std::vector<H2B::Material> materials;
	std::vector<MaterialTextures> textures;

	// All level boundry data used by the models
	std::vector<GW::MATH::GOBBF> colliders;

	std::vector<H2B::Mesh> meshes;
	// Contains material indices for each mesh
	std::vector<H2B::Batch> batches;

	std::vector<std::string> h2bNames;
	std::vector<Model> models;

	// Each item from the blender scene graph
	std::vector<BlenderObject> blenderObjects;

	// Imports the default level txt format and collects all .h2b data
	bool LoadActors(const char* _actorH2bFolderPath, GW::SYSTEM::GLog _log)
	{
		_log.LogCategorized("EVENT", "LOADING GAME LEVEL [DATA ORIENTED]");

		UnloadActors();// clear previous level data if there is any

		if (ReadAndCombineH2Bs(_actorH2bFolderPath, _log) == false)
		{
			_log.LogCategorized(
				"ERROR", 
				"Fatal error combining H2B mesh data, aborting level load.");

			return false;
		}

		// level loaded into CPU ram
		_log.LogCategorized("EVENT", "GAME LEVEL WAS LOADED TO CPU [DATA ORIENTED]");

		return true;
	}

	// used to wipe CPU level data between levels
	void UnloadActors() 
	{
		dataStrings.clear();
		vertices.clear();
		indices.clear();
		materials.clear();
		textures.clear();
		batches.clear();
		meshes.clear();
		models.clear();
	}

private:

	// transfered from parser
	std::set<std::string> dataStrings;

	// loads all file names in the pathed folder into h2bNames
	bool FindH2BNames(const char* _h2bFolderPath, GW::SYSTEM::GLog log)
	{
		std::string folderPath = _h2bFolderPath;

		WIN32_FIND_DATAA findFileData;
		//finds the first .h2b file in the directory and stores it in @findHandle
		// the /*.h2b means any .h2b file
		HANDLE findHandle = FindFirstFileA((folderPath + "/*.h2b").c_str(), &findFileData);

		//if the handle is valid, loop through every file in directory and add their path
		//to textureFilePaths
		if (findHandle != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					h2bNames.push_back(findFileData.cFileName);
				}
			} while (FindNextFileA(findHandle, &findFileData) != 0);

			FindClose(findHandle);

			log.LogCategorized("MESSAGE", "Actor Filepaths Found.");
		}
		else
		{
			log.LogCategorized("MESSAGE", "Error opening directory.");
			return false;
		}

		return true;
	}

	// internal helper for collecting all .h2b data into unified arrays
	bool ReadAndCombineH2Bs(const char* _h2bFolderPath,
							GW::SYSTEM::GLog _log)
	{
		_log.LogCategorized("MESSAGE", "Begin Importing .H2B File Data.");
		// parse each model adding to overall arrays
		H2B::Parser parser; // reads the .h2b format
		FindH2BNames(_h2bFolderPath, _log);
		std::string relativePath(_h2bFolderPath);

		for (int i = 0; i < h2bNames.size(); i += 1)
		{
			if (parser.Parse((relativePath + h2bNames[i]).c_str()))
			{
				_log.LogCategorized(
					"INFO", 
					(std::string("H2B Imported: ") + h2bNames[i]).c_str());

				// transfer all string data
				for (int j = 0; j < parser.materialCount; ++j) {
					for (int k = 0; k < 10; ++k) {
						if (*((&parser.materials[j].name) + k) != nullptr)
							*((&parser.materials[j].name) + k) =
							dataStrings.insert(*((&parser.materials[j].name) + k)).first->c_str();
					}
				}
				for (int j = 0; j < parser.meshCount; ++j) {
					if (parser.meshes[j].name != nullptr)
						parser.meshes[j].name =
						dataStrings.insert(parser.meshes[j].name).first->c_str();
				}


				// record source file name & sizes
				Model currModel{};
				currModel.fileName = h2bNames[i].c_str();
				currModel.vertexCount = parser.vertexCount;
				currModel.indexCount = parser.indexCount;
				currModel.materialCount = parser.materialCount;
				currModel.meshCount = parser.meshCount;
				currModel.vertexStart = vertices.size();
				currModel.indexStart = indices.size();
				currModel.materialStart = materials.size();
				currModel.batchStart = batches.size();
				currModel.meshStart = meshes.size();
				currModel.colliderIndex = colliders.size();
				currModel.transformStart = i;
				models.push_back(currModel);

				// append/move all data
				vertices.insert(vertices.end(), parser.vertices.begin(), parser.vertices.end());
				indices.insert(indices.end(), parser.indices.begin(), parser.indices.end());
				materials.insert(materials.end(), parser.materials.begin(), parser.materials.end());
				batches.insert(batches.end(), parser.batches.begin(), parser.batches.end());
				meshes.insert(meshes.end(), parser.meshes.begin(), parser.meshes.end());
				colliders.push_back(models[i].ComputeOBB());
			}
			else 
			{
				// notify user that a model file is missing but continue loading
				_log.LogCategorized("ERROR",
					(std::string("H2B Not Found: ") + h2bNames[i]).c_str());
				_log.LogCategorized("WARNING", "Loading will continue but model(s) are missing.");
			}
		}
		_log.LogCategorized("MESSAGE", "Importing of .H2B File Data Complete.");
		return true;
	}
};
