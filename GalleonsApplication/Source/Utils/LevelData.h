// This is a sample of how to load a level in a data oriented fashion.
// Feel free to use this code as a base and tweak it for your needs.
// *NEW* The new version of this loader saves blender names.
#pragma once

// This reads .h2b files which are optimized binary .obj+.mtl files
#include "h2bParser.h"
#include <string>

// * NOTE: *
// Unlike the OOP version, this class was not designed to be a dynamic/evolving data structure.
// It excells at storing/traversing the static level geometry extremely efficiently.
// Removing or adding unique instances dynamically to these arrays is possible but very tricky.
// Instead, I would recommend handling what to draw externally with something like this:
// struct DRAW_INSTRUCTION { // These are cheap to make, you can generate them each frame if you have to
//	unsigned vStart, iStart, iCount, materialID; // the specific data you want to instanced draw
//  std::vector<GW::MATH::GMATRIXF> matrixCache; // each copy to be drawn/sent to GPU this frame
// };
// If your Renderer intializes a container of the above from the Level_Data it will be much more flexible.
// This flexibility may not seem that valuable at first... (ex: Can't I just use the instances directly?)
// However, try to dynamically add/remove objects(ex: enemies/bullets) or do frustum culling or shadows.
// You will quickly see there is an important distinction in what you CAN draw vs. what you SHOULD draw.
// *Tip:* D3D12 & Vulkan will not allow updates to buffers between draws() without the use of fences.
// For those APIs consider having one large per-frame matrix cache you fill/offset into when instancing. 

// class Level_Data contains the compacted verison of all level data needed by the GPU.
// Ideally you should consider this data what you *can* draw, not what you *must* draw. (see above)    
class LevelData 
{

	// transfered from parser
	std::set<std::string> dataStrings;

public: 

	// one model in the level
	struct LevelModel
	{
		const char* fileName;
		unsigned vertexCount, indexCount, materialCount, meshCount;
		unsigned vertexStart, indexStart, materialStart, meshStart, batchStart;
		unsigned colliderIndex;
		unsigned int texId;
	};
	// instances of each model in the level
	struct ModelInstances
	{
		unsigned modelIndex, transformStart, transformCount, flags;
	};
	struct MaterialTextures
	{
		unsigned int albedoIndex, roughnessIndex, metalIndex, normalIndex;
	};
	struct BlenderObject
	{
		const char* blenderName;
		unsigned int modelIndex, transformIndex;
	};

	
	std::vector<H2B::Vertex> vertices;
	std::vector<unsigned> indices;

	std::vector<H2B::Material> materials;
	std::vector<MaterialTextures> textures;

	// All level boundry data used by the models
	std::vector<GW::MATH::GOBBF> levelColliders;

	std::vector<H2B::Mesh> levelMeshes;
	// Contains material indices for each mesh
	std::vector<H2B::Batch> levelBatches;

	std::vector<LevelModel> levelModels;
	std::vector<ModelInstances> levelInstances;
	std::vector<GW::MATH::GMATRIXF> transforms;
	
	// each item from the blender scene graph
	std::vector<BlenderObject> blenderObjects;

	std::vector<H2B::Light> sceneLights;
	

	// Imports the default level txt format and collects all .h2b data
	bool LoadLevel(	const char* _gameLevelPath, 
					const char* _h2bFolderPath, 
					GW::SYSTEM::GLog _log) 
	{
		// What this does:
		// Parse GameLevel.txt 
		// For each model found in the file...
			// if not encountered create new unique temporary model entry.
				// Add model transform to a list of transforms for this model.(instances)
			// if already encountered, just add its transfrom to the existing model entry.
		// when finished, traverse model entries to import each model's data to the class.
		std::set<TempModelEntry> uniqueModels; // unique models and their locations
		_log.LogCategorized("EVENT", "LOADING GAME LEVEL [DATA ORIENTED]");

		UnloadLevel();// clear previous level data if there is any

		if (ReadGameLevel(_gameLevelPath, uniqueModels, _log) == false) 
		{
			_log.LogCategorized("ERROR", "Fatal error reading game level, aborting level load.");
			return false;
		}

		if (ReadAndCombineH2Bs(_h2bFolderPath, uniqueModels, _log) == false) 
		{
			_log.LogCategorized("ERROR", "Fatal error combining H2B mesh data, aborting level load.");
			return false;
		}

		// level loaded into CPU ram
		_log.LogCategorized("EVENT", "GAME LEVEL WAS LOADED TO CPU [DATA ORIENTED]");
		return true;
	}


	bool StartsWith(std::string input, std::string subString)
	{
		std::string_view inputView(input);
		std::string_view subStringView(subString);

		if (inputView.size() >= subStringView.size())
		{
			return inputView.substr(0, subStringView.size()) == subStringView;
		}
		return false;
	}


	// used to wipe CPU level data between levels
	void UnloadLevel() {
		dataStrings.clear();
		vertices.clear();
		indices.clear();
		materials.clear();
		textures.clear();
		levelBatches.clear();
		levelMeshes.clear();
		levelModels.clear();
		transforms.clear();
		levelInstances.clear();
		blenderObjects.clear();
	}

private:

	// internal defintion for reading the GameLevel layout 
	struct TempModelEntry
	{
		std::string modelFile; // path to .h2b file
		// object aligned bounding box data: LBN, LTN, LTF, LBF, RBN, RTN, RTF, RBF
		// F,N = front, back   L,R = left, right   T,B = top, bottom
		GW::MATH2D::GVECTOR3F boundry[8];
		mutable std::vector<std::string> blenderNames;
		mutable std::vector<GW::MATH::GMATRIXF> instanceTransforms;
		
		// override < operator, you need this for std::set to work for some reaosn
		bool operator < (const TempModelEntry& _cmp) const {
			return modelFile < _cmp.modelFile; 
		}

		// converts the vec3 boundries to an OBB
		GW::MATH::GOBBF ComputeOBB() const 
		{
			GW::MATH::GOBBF boundBox = {
				GW::MATH::GIdentityVectorF,
				GW::MATH::GIdentityVectorF,
				GW::MATH::GIdentityQuaternionF
			};

			boundBox.center.x = (boundry[0].x + boundry[4].x) * 0.5f;
			boundBox.center.y = (boundry[0].y + boundry[1].y) * 0.5f;
			boundBox.center.z = (boundry[0].z + boundry[2].z) * 0.5f;
			boundBox.extent.x = std::fabsf(boundry[0].x - boundry[4].x) * 0.5f;
			boundBox.extent.y = std::fabsf(boundry[0].y - boundry[1].y) * 0.5f;
			boundBox.extent.z = std::fabsf(boundry[0].z - boundry[2].z) * 0.5f;

			return boundBox;
		}
	};

	
	bool ReadGameLevel(const char* _gameLevelPath, 
						std::set<TempModelEntry> &_outModels,
						GW::SYSTEM::GLog _log) 
	{
		_log.LogCategorized("MESSAGE", "Begin Reading Game Level Text File.");
		GW::SYSTEM::GFile file;
		file.Create();

		if (-file.OpenTextRead(_gameLevelPath)) 
		{
			_log.LogCategorized(
				"ERROR", (std::string("Game level not found: ") + _gameLevelPath).c_str());
			return false;
		}

		char lineBuffer[1024];

		while (+file.ReadLine(lineBuffer, 1024, '\n'))
		{
			std::string vecSize = std::to_string(sceneLights.size());

			// having to have this is a bug, need to have Read/ReadLine return failure at EOF
			if (lineBuffer[0] == '\0')
				break;

			if (std::strcmp(lineBuffer, "MESH") == 0)
			{
				file.ReadLine(lineBuffer, 1024, '\n');
				std::string blenderName = lineBuffer;

				_log.LogCategorized("INFO", (std::string("Model Detected: ") + blenderName).c_str());

				// create the model file name from this (strip the .001)
				TempModelEntry add = { lineBuffer };
				add.modelFile = add.modelFile.substr(0, add.modelFile.find_last_of("."));
				add.modelFile += ".h2b";

				// now read the transform data as we will need that regardless
				GW::MATH::GMATRIXF transform;

				for (int i = 0; i < 4; ++i) 
				{
					file.ReadLine(lineBuffer, 1024, '\n');
					// read floats
					std::sscanf(lineBuffer + 13, "%f, %f, %f, %f",
						&transform.data[0 + i * 4], &transform.data[1 + i * 4],
						&transform.data[2 + i * 4], &transform.data[3 + i * 4]);
				}

				std::string loc = "Location: X ";
				loc += std::to_string(transform.row4.x) + " Y " +
					std::to_string(transform.row4.y) + " Z " + std::to_string(transform.row4.z);
				_log.LogCategorized("INFO", loc.c_str());

				//// *NEW* finally read in the boundry data for this model
				// NOTE: Data might be -90 degrees off on the X axis. Not verified.
				//for (int i = 0; i < 8; ++i) {
				//	file.ReadLine(linebuffer, 1024, '\n');
				//	// read floats
				//	std::sscanf(linebuffer + 9, "%f, %f, %f",
				//		&add.boundry[i].x, &add.boundry[i].y, &add.boundry[i].z);
				//}
				//std::string bounds = "Boundry: Left ";
				//bounds += std::to_string(add.boundry[0].x) +
				//" Right " +	std::to_string(add.boundry[4].x) +
				//" Bottom " + std::to_string(add.boundry[0].y) +
				//" Top " + std::to_string(add.boundry[1].y) +
				//" Near " + std::to_string(add.boundry[0].z) +
				//" Far " + std::to_string(add.boundry[2].z);
				//log.LogCategorized("INFO", bounds.c_str());

				// does this model already exist?
				auto found = _outModels.find(add);
				if (found == _outModels.end())
				{
					add.blenderNames.push_back(blenderName);
					add.instanceTransforms.push_back(transform);
					_outModels.insert(add);
				}
				else
				{
					found->blenderNames.push_back(blenderName);
					found->instanceTransforms.push_back(transform);
				}
			}
			else if (std::strcmp(lineBuffer, "LIGHT") == 0)
			{
				file.ReadLine(lineBuffer, 1024, '\n');
				std::string blenderName = lineBuffer;
				_log.LogCategorized("INFO", (std::string("Light Detected: ") + blenderName).c_str());
							
				GW::MATH::GMATRIXF lightTransform;
				GW::MATH::GVECTORF lightPos;
				float lightR;
				float lightG;
				float lightB;
				GW::MATH::GVECTORF lightColor;
				float radius;
				GW::MATH::GVECTORF lightDir;
				float innerCone;
				float outerCone;
			
				// Read the lights transform
				for (int i = 0; i < 4; ++i) {
					file.ReadLine(lineBuffer, 1024, '\n');
					
					std::sscanf(lineBuffer + 13, "%f, %f, %f, %f",
						&lightTransform.data[0 + i * 4], &lightTransform.data[1 + i * 4],
						&lightTransform.data[2 + i * 4], &lightTransform.data[3 + i * 4]);
				}

				std::string loc = "Location: X ";
				loc += std::to_string(lightTransform.row4.x) + " Y " +
					std::to_string(lightTransform.row4.y) + " Z " + std::to_string(lightTransform.row4.z);
				_log.LogCategorized("INFO", loc.c_str());

				// Read the lights position and type
				file.ReadLine(lineBuffer, 1024, '\n');
				std::sscanf(lineBuffer + 11, "%f, %f, %f, %f", &lightPos.x, &lightPos.y, &lightPos.z, &lightPos.w);

				file.ReadLine(lineBuffer, 1024, '\n');
				std::sscanf(lineBuffer + 8, "%f, %f, %f", &lightR, &lightG, &lightB);

				lightColor = { lightR, lightG, lightB, 0.0f };
				

				file.ReadLine(lineBuffer, 1024, '\n');
				std::sscanf(lineBuffer + 9, "%f", &radius);

				lightDir = { 0, 0, 0, 0 };

				if (lightPos.w == 1.0f)
				{
					H2B::Light pointLight = { lightTransform, lightPos, lightColor, lightDir, radius,  0.0f, 0.0f };
					sceneLights.push_back(pointLight);
					_log.LogCategorized("INFO", "Point Light Pushed to Vector");
				}
				else
				{
					file.ReadLine(lineBuffer, 1024, '\n');
					std::sscanf(lineBuffer + 12, "%f, %f, %f", &lightDir.x, &lightDir.y, &lightDir.z);

					file.ReadLine(lineBuffer, 1024, '\n');
					std::sscanf(lineBuffer + 13, "%f", &innerCone);

					file.ReadLine(lineBuffer, 1024, '\n');
					std::sscanf(lineBuffer + 13, "%f", &outerCone);

					H2B::Light spotLight = { lightTransform, lightPos, lightColor, lightDir, radius, innerCone, outerCone };
					sceneLights.push_back(spotLight);
					_log.LogCategorized("INFO", "Spot Light Pushed to Vector");

				}
				
			}

			_log.LogCategorized("SCENE LIGHTS SIZE: ", vecSize.c_str());
		}
		_log.LogCategorized("MESSAGE", "Game Level File Reading Complete.");
		return true;
	}
	// internal helper for collecting all .h2b data into unified arrays
	bool ReadAndCombineH2Bs(const char* _h2bFolderPath, 
							const std::set<TempModelEntry>& _modelSet,
							GW::SYSTEM::GLog _log) {
		_log.LogCategorized("MESSAGE", "Begin Importing .H2B File Data.");
		// parse each model adding to overall arrays
		H2B::Parser parser; // reads the .h2b format
		const std::string modelPath = _h2bFolderPath;
		for (auto i = _modelSet.begin(); i != _modelSet.end(); ++i)
		{
			if (parser.Parse((modelPath + "/" + i->modelFile).c_str()))
			{
				_log.LogCategorized("INFO", (std::string("H2B Imported: ") + i->modelFile).c_str());
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
				LevelModel model;
				model.fileName = dataStrings.insert(i->modelFile).first->c_str();
				model.vertexCount = parser.vertexCount;
				model.indexCount = parser.indexCount;
				model.materialCount = parser.materialCount;
				model.meshCount = parser.meshCount;
				// record offsets
				model.vertexStart = vertices.size();
				model.indexStart = indices.size();
				model.materialStart = materials.size();
				model.batchStart = levelBatches.size();
				model.meshStart = levelMeshes.size();

				std::string modelFileCopy = model.fileName;

				if (StartsWith(modelFileCopy, "Ranger"))
				{
					model.texId = 1;
				}
				else if (StartsWith(modelFileCopy, "Rogue"))
				{
					model.texId = 2;
				}
				else if (StartsWith(modelFileCopy, "Warrior"))
				{
					model.texId = 3;
				}
				else if (StartsWith(modelFileCopy, "Wizard"))
				{
					model.texId = 4;
				}
				else
				{
					model.texId = 0;
				}

				// append/move all data
				vertices.insert(vertices.end(), parser.vertices.begin(), parser.vertices.end());
				indices.insert(indices.end(), parser.indices.begin(), parser.indices.end());
				materials.insert(materials.end(), parser.materials.begin(), parser.materials.end());
				levelBatches.insert(levelBatches.end(), parser.batches.begin(), parser.batches.end());
				levelMeshes.insert(levelMeshes.end(), parser.meshes.begin(), parser.meshes.end());
				// *NEW* add overall collision volume(OBB) for this model and it's submeshes 
				model.colliderIndex = levelColliders.size();
				levelColliders.push_back(i->ComputeOBB());
				// add level model
				levelModels.push_back(model);
				// add level model instances
				ModelInstances instances;
				instances.flags = 0; // shadows? transparency? much we could do with this.
				instances.modelIndex = levelModels.size() - 1;
				instances.transformStart = transforms.size();
				instances.transformCount = i->instanceTransforms.size();
				transforms.insert(transforms.end(), i->instanceTransforms.begin(), i->instanceTransforms.end());
				// add instance set
				levelInstances.push_back(instances);
				// *NEW* Add an entry for each unique blender object
				int offset = 0;
				for (auto &n : i->blenderNames) {
					BlenderObject obj {
						dataStrings.insert(n).first->c_str(),
						instances.modelIndex, instances.transformStart + offset++
					};
					blenderObjects.push_back(obj);
				}
			}
			else {
				// notify user that a model file is missing but continue loading
				_log.LogCategorized("ERROR",
					(std::string("H2B Not Found: ") + modelPath + "/" + i->modelFile).c_str());
				_log.LogCategorized("WARNING", "Loading will continue but model(s) are missing.");
			}
		}
		_log.LogCategorized("MESSAGE", "Importing of .H2B File Data Complete.");
		return true;
	}
};

