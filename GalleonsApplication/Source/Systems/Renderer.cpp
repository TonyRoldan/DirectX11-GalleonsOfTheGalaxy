#include "Renderer.h"
#include "../Utils/TextStorage.h"


using namespace GOG;

#pragma region Initialize

#pragma region Init
bool GOG::DirectX11Renderer::Init(GW::SYSTEM::GWindow _win,
	GW::GRAPHICS::GDirectX11Surface _renderingSurface,
	std::shared_ptr<flecs::world> _flecsWorld,
	std::shared_ptr<flecs::world> _uiWorld,
	std::weak_ptr<const GameConfig> _gameConfig,
	LevelData* _lvl,
	ActorData* _actors,
	GW::CORE::GEventGenerator _eventPusher)
{
	window = _win;
	d3d = _renderingSurface;
	flecsWorld = _flecsWorld;
	uiWorld = _uiWorld;
	uiWorldAsync = _uiWorld->async_stage();
	uiWorldLock.Create();
	gameConfig = _gameConfig;
	std::shared_ptr<const GameConfig> readCfg = gameConfig.lock();
	levelData = _lvl;
	actorData = _actors;
	eventPusherUI = _eventPusher;

	currState = GAME_STATES::LOGO_SCREEN;
	input.Create(_win);

	//World
	GW::MATH::GMATRIXF world{ GW::MATH::GIdentityMatrixF };
	worldMatrix = world;

	//View
	GW::MATH::GVECTORF eye{ 13.0f, 21.0f, 13.0f, 1.0f };
	GW::MATH::GVECTORF at{ 0.15f, 0.75f, 0.0f, 1.0f };
	GW::MATH::GVECTORF up{ 0.0f, 1.0f, 0.0f, 1.0f };
	GW::MATH::GMATRIXF view;
	GW::MATH::GMatrix::LookAtLHF(eye, at, up, view);
	viewMatrix = view;
	camPos = eye;

	//Projection
	fov = 65.0f * 3.14f / 180.0f;
	d3d.GetAspectRatio(currAspect);
	nearPlane = 0.1f;
	farPlane = 2000.0f;
	GW::MATH::GMATRIXF projection;
	GW::MATH::GMatrix::ProjectionDirectXLHF(fov, currAspect, nearPlane, farPlane, projection);
	projectionMatrix = projection;

	//Level
	levelSegmentWidth = readCfg->at("Game").at("levelSegmentWidth").as<float>();
	H2B::Attributes levelAttrib = levelData->materials[levelData->levelInstances.front().modelIndex].attrib;
	levelAttribute = levelAttrib;

	//Actors
	H2B::Attributes actorAttrib = actorData->materials[actorData->meshes.begin()->materialIndex].attrib;
	actorAttribute = actorAttrib;
	for (int i = 0; i < instanceMax; i++)
	{
		instanceTransforms.transforms[i] = world;
		instanceTransforms.modelNdxs[i] = 0;
	}

	//UI
	screenWidth = readCfg->at("Window").at("width").as<int>();
	screenHeight = readCfg->at("Window").at("height").as<int>();
	newHeight = screenHeight;
	newWidth = screenWidth;
	creditsOffset = { (float)screenHeight * 2.0f }; 
	creditsPosCopy = { 0.0f, 0.0f };
	uiScalar = 1;

	black = {0.0f, 0.0f, 0.0f, 1.0f};
	white = { 1.0f, 1.0f, 1.0f, 1.0f };
	skyBlue = { 0.5f, 0.5f, 1.1f };
	rust = { 0.72f, 0.25f, 0.05f, 1.0f };
	bgColor = white;

	splashAlpha = 0.0f;
	pauseAlpha = 1.0f;
	playerCurrScore = 0;
	fadeInDone = false;
	maxReached = true;
	minReached = false;
	isStateChanged = false;

	//MiniMap
	mapEye = eye;
	mapAt = at;
	mapUp = { 0.0f, 1.0f, 0.0f, 1.0f };
	GW::MATH::GMatrix::LookAtLHF(mapEye, mapAt, mapUp, mapViewMatrix);
	GW::MATH::GMATRIXF mapProjection;
	mapModelScalar = { 3.0f, 3.0f, 3.0f, 1.0f };
	zoomFactor = 3;
	mapProjMatrix = (GW::MATH::GMATRIXF&)DirectX::XMMatrixOrthographicLH(screenWidth / zoomFactor, screenHeight / 4 / zoomFactor, nearPlane, farPlane);
	mapQuad = CreateQuad();

	mapViewPort.TopLeftX = 0;
	mapViewPort.TopLeftY = 0;
	mapViewPort.Width = screenWidth;
	mapViewPort.Height = screenHeight;
	mapViewPort.MinDepth = 0;
	mapViewPort.MaxDepth = 1;

	LoadSceneData();

	//Texture
	texId = levelData->levelModels[levelData->levelInstances.front().modelIndex].texId;
	modelType = 0;

	struct UIMergeAsyncStages {};
	_uiWorld->entity("UIMergeAsyncStages").add<UIMergeAsyncStages>();

	uiWorld->system<UIMergeAsyncStages>()
		.kind(flecs::OnLoad)
		.each([this](flecs::entity _entity, UIMergeAsyncStages& _uiMergeAsync)
			{
				uiWorldLock.LockSyncWrite();
				uiWorldAsync.merge();
				uiWorldLock.UnlockSyncWrite();
			});

	//Buffer Data
	mapModelData = { modelType };
	instanceData = { 0, 0, texId };

	sceneData = currentLevelSceneData;
	sceneData.viewMatrix = viewMatrix;
	sceneData.projectionMatrix = projectionMatrix;
	sceneData.camPos = camPos;

	meshData = { worldMatrix, levelAttribute };
	actorMeshData = { worldMatrix, actorAttribute };

	InitCredits();
	if (LoadEventResponders() == false)
		return false;
	if (LoadShaders() == false)
		return false;
	if (LoadBuffers() == false)
		return false;
	if (LoadShaderResources() == false)
		return false;
	if (LoadTextures() == false)
		return false;
	if (LoadGeometry() == false)
		return false;
	if (Load2D() == false)
		return false;
	if (SetupPipeline() == false)
		return false;
	InitRendererSystems();

	return true;
}

bool GOG::DirectX11Renderer::LoadSceneData()
{
	levelSceneData.push_back({});
	levelSceneData[0].dirLightColor = { 0.75f, 0.75f, 1.0f, 1.0f };
	levelSceneData[0].dirLightDir = { -2.0f, -2.0f, 0.0f, 1.0f };
	levelSceneData[0].ambientTerm = { 0.5f, 0.4f, 0.3f, 0.0f };
	levelSceneData[0].fogColor = { 0.5f, 0.5f, 1.1f, 1 };
	levelSceneData[0].fogDensity = 0.005;
	levelSceneData[0].fogStartDistance = 75;
	levelSceneData[0].contrast = 1;
	levelSceneData[0].saturation = 1.1;

	actorSceneData.push_back({});
	actorSceneData[0].dirLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	actorSceneData[0].dirLightDir = { 0.0f, -1.0f, 1.0f, 1.0f };
	actorSceneData[0].ambientTerm = { 0.5f, 0.4f, 0.3f, 0.0f };
	actorSceneData[0].fogColor = { 0.5f, 0.5f, 1.0f, 1 };
	actorSceneData[0].fogDensity = 0.005;
	actorSceneData[0].fogStartDistance = 100;
	actorSceneData[0].contrast = 1;
	actorSceneData[0].saturation = 1.25;

	bgColorData.push_back({ 0.5f, 0.5f, 1.1f, 1 });

	// Bomb effect animation
	levelSceneData.push_back({});
	levelSceneData[1].dirLightColor = { 0.75f, 0.75f, 1.0f, 1.0f };
	levelSceneData[1].dirLightDir = { -2.0f, -2.0f, 0.0f, 1.0f };
	levelSceneData[1].ambientTerm = { 0.5f, 0.4f, 0.3f, 0.0f };
	levelSceneData[1].fogColor = { 1.0f, 1.0f, 1.0f, 1 };
	levelSceneData[1].fogDensity = 1;
	levelSceneData[1].fogStartDistance = 0;
	levelSceneData[1].contrast = 1;
	levelSceneData[1].saturation = 1.1;

	actorSceneData.push_back({});
	actorSceneData[1].dirLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	actorSceneData[1].dirLightDir = { 0.0f, -1.0f, 1.0f, 1.0f };
	actorSceneData[1].ambientTerm = { 0.5f, 0.4f, 0.3f, 0.0f };
	actorSceneData[1].fogColor = { 0, 0, 0, 1 };
	actorSceneData[1].fogDensity = 1;
	actorSceneData[1].fogStartDistance = 0;
	actorSceneData[1].contrast = 1;
	actorSceneData[1].saturation = 1.25;

	bgColorData.push_back({ 1.0f, 1.0f, 1.0f, 1 });

	currentLevelSceneData = levelSceneData[0];
	currentActorSceneData = actorSceneData[0];
	currentBgColorData = bgColorData[0];

	return true;
}
#pragma endregion

#pragma region Events

bool GOG::DirectX11Renderer::LoadEventResponders()
{
	onEvent.Create([this](const GW::GEvent& _event)
		{
			PLAY_EVENT eventTag;
			PLAY_EVENT_DATA data;

			if (+_event.Read(eventTag, data))
			{
				switch (eventTag)
				{
					case PLAY_EVENT::PLAYER_DESTROYED:
					{
						UpdateLives(data.value);
						if (data.value <= 0)
						{
							currState = GAME_STATES::GAME_OVER_SCREEN;
						}
						break;
					}

					case PLAY_EVENT::UPDATE_SCORE:
					{
						UpdateScore(data.value);
						break;
					}

					case PLAY_EVENT::SMART_BOMB_ACTIVATED:
					{
						UpdateSmartBombs(data.value);

						auto now = std::chrono::system_clock::now().time_since_epoch();
						bombEffectStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

						if (bombEffect == nullptr)
						{
							bombEffect.Create(updateBombEffectTime, [this]() {
								auto curTime = std::chrono::system_clock::now().time_since_epoch();
								unsigned now = std::chrono::duration_cast<std::chrono::milliseconds>(curTime).count();
								float ratio = (now - bombEffectStartTime) / (float)bombEffectTime;
								bool isDone = false;

								if (ratio > 1)
								{
									isDone = true;
									ratio = 1;
								}

								GW::MATH::GVector::LerpF(actorSceneData[1].fogColor, actorSceneData[0].fogColor,
									ratio, currentActorSceneData.fogColor);
								currentActorSceneData.fogDensity =
									G_LERP(actorSceneData[1].fogDensity, actorSceneData[0].fogDensity, ratio);
								currentActorSceneData.fogStartDistance =
									G_LERP(actorSceneData[1].fogStartDistance, actorSceneData[0].fogStartDistance, ratio);

								GW::MATH::GVector::LerpF(levelSceneData[1].fogColor, levelSceneData[0].fogColor,
									ratio, currentLevelSceneData.fogColor);
								currentLevelSceneData.fogDensity =
									G_LERP(levelSceneData[1].fogDensity, levelSceneData[0].fogDensity, ratio);
								currentLevelSceneData.fogStartDistance =
									G_LERP(levelSceneData[1].fogStartDistance, levelSceneData[0].fogStartDistance, ratio);

								GW::MATH::GVector::LerpF(bgColorData[1], bgColorData[0], ratio, currentBgColorData);

								if (isDone)
								{
									bombEffect.Pause(0, false);
								}
								}, 1);
						}
						else 
						{
							bombEffect.Resume();
						}
						break;
					}

					case PLAY_EVENT::SMART_BOMB_GRABBED:
					{
						UpdateSmartBombs(data.value);

						break;
					}

					case PLAY_EVENT::WAVE_CLEARED:
					{
						UpdateWaves(data.value);

						break;
					}
					case PLAY_EVENT::STATE_CHANGED:
					{
						uiWorldLock.LockSyncWrite();
						splashAlpha = data.value;
						creditsOffset = (float)newHeight * 2.9f;
						uiWorldLock.UnlockSyncWrite();
						break;
					}
					default:
					{
						break;
					}
						
				}			
			}

		});
	eventPusherUI.Register(onEvent);

	onWindowResize.Create([&](const GW::GEvent& event)
		{
			GW::SYSTEM::GWindow::Events resize;
			if (+event.Read(resize) && (resize == GW::SYSTEM::GWindow::Events::RESIZE || resize == GW::SYSTEM::GWindow::Events::MAXIMIZE))
			{
				GOG::PipelineHandles handles{};
				d3d.GetImmediateContext((void**)&handles.context);			

				window.GetClientWidth(newWidth);
				window.GetClientHeight(newHeight);

				Resize(newHeight, newWidth);

				if (newWidth > screenWidth)
				{
					if (currState == GAME_STATES::PLAY_GAME || currState == GAME_STATES::PAUSE_GAME)
					{
						uiScalar = ((float)newWidth / (float)screenWidth) - 0.35f;
					}
					else
					{
						uiScalar = ((float)newWidth / (float)screenWidth);
					}				
				}
				else
				{
					uiScalar = ((float)newWidth / (float)screenWidth);
				}

				d3d.GetAspectRatio(currAspect);
	
				GW::MATH::GMatrix::ProjectionDirectXLHF(fov, currAspect, nearPlane, farPlane, projectionMatrix);

				handles.context->Release();
			}			
		});
	window.Register(onWindowResize);

	return true;
}

#pragma endregion

#pragma region Shaders
bool GOG::DirectX11Renderer::LoadShaders()
{
	ID3D11Device* device{};
	d3d.GetDevice((void**)&device);

	std::shared_ptr<const GameConfig> readCfg = gameConfig.lock();

	std::string vertexShaderSource = ReadFileIntoString("../Shaders/VertexShader.hlsl");
	std::string pixelShaderSource = ReadFileIntoString("../Shaders/PixelShader.hlsl");
	std::string mapVSSource = ReadFileIntoString("../Shaders/VSMap.hlsl");
	std::string mapPSSource = ReadFileIntoString("../Shaders/PSMap.hlsl");
	std::string mapModelsPSSource = ReadFileIntoString("../Shaders/PSColorMapModels.hlsl");
	std::string levelVSSource = ReadFileIntoString("../Shaders/VSLevelShader.hlsl");

	UINT compilerFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
	compilerFlags |= D3DCOMPILE_DEBUG;
#endif

	HRESULT vsCompilationResult = D3DCompile(vertexShaderSource.c_str(),
		vertexShaderSource.length(),
		nullptr,
		nullptr,
		nullptr,
		"main",
		"vs_5_0",
		compilerFlags,
		0,
		vsBlob.GetAddressOf(),
		errors.GetAddressOf());

	HRESULT vsMapCompResult = D3DCompile(mapVSSource.c_str(),
		mapVSSource.length(),
		nullptr,
		nullptr,
		nullptr,
		"main",
		"vs_5_0",
		compilerFlags,
		0,
		vsMapBlob.GetAddressOf(),
		errors.GetAddressOf());

	HRESULT vsLevelCompResult = D3DCompile(levelVSSource.c_str(),
		levelVSSource.length(),
		nullptr,
		nullptr,
		nullptr,
		"main",
		"vs_5_0",
		compilerFlags,
		0,
		vsLevelBlob.GetAddressOf(),
		errors.GetAddressOf());

	if (SUCCEEDED(vsCompilationResult))
	{
		device->CreateVertexShader(vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(),
			nullptr, vertexShader.GetAddressOf());
	}
	else
	{
		PrintLabeledDebugString("Vertex Shader Errors:\n", (char*)errors->GetBufferPointer());
		abort();
		return false;
	}

	if (SUCCEEDED(vsMapCompResult))
	{
		device->CreateVertexShader(vsMapBlob->GetBufferPointer(),
			vsMapBlob->GetBufferSize(),
			nullptr, mapVertexShader.GetAddressOf());
	}
	else
	{
		PrintLabeledDebugString("Vertex Shader Errors:\n", (char*)errors->GetBufferPointer());
		abort();
		return false;
	}

	if (SUCCEEDED(vsLevelCompResult))
	{
		device->CreateVertexShader(vsLevelBlob->GetBufferPointer(),
			vsLevelBlob->GetBufferSize(),
			nullptr, levelVertexShader.GetAddressOf());
	}
	else
	{
		PrintLabeledDebugString("Vertex Shader Errors:\n", (char*)errors->GetBufferPointer());
		abort();
		return false;
	}

	HRESULT psCompilationResult = D3DCompile(pixelShaderSource.c_str(),
		pixelShaderSource.length(),
		nullptr,
		nullptr,
		nullptr,
		"main",
		"ps_5_0",
		compilerFlags,
		0,
		psBlob.GetAddressOf(),
		errors.GetAddressOf());

	HRESULT psMapCompResult = D3DCompile(mapPSSource.c_str(),
		mapPSSource.length(),
		nullptr,
		nullptr,
		nullptr,
		"main",
		"ps_5_0",
		compilerFlags,
		0,
		psMapBlob.GetAddressOf(),
		errors.GetAddressOf());

	HRESULT psMapModelsCompResult = D3DCompile(mapModelsPSSource.c_str(),
		mapModelsPSSource.length(),
		nullptr,
		nullptr,
		nullptr,
		"main",
		"ps_5_0",
		compilerFlags,
		0,
		psMapModelsBlob.GetAddressOf(),
		errors.GetAddressOf());

	if (SUCCEEDED(psCompilationResult))
	{
		device->CreatePixelShader(psBlob->GetBufferPointer(),
			psBlob->GetBufferSize(),
			nullptr,
			pixelShader.GetAddressOf());

	}
	else
	{
		PrintLabeledDebugString("Pixel Shader Errors:\n", (char*)errors->GetBufferPointer());
		abort();
		return false;
	}

	if (SUCCEEDED(psMapCompResult))
	{
		device->CreatePixelShader(psMapBlob->GetBufferPointer(),
			psMapBlob->GetBufferSize(),
			nullptr,
			mapPixelShader.GetAddressOf());

	}
	else
	{
		PrintLabeledDebugString("Pixel Shader Errors:\n", (char*)errors->GetBufferPointer());
		abort();
		return false;
	}

	if (SUCCEEDED(psMapModelsCompResult))
	{
		device->CreatePixelShader(psMapModelsBlob->GetBufferPointer(),
			psMapModelsBlob->GetBufferSize(),
			nullptr,
			mapModelsPixelShader.GetAddressOf());

	}
	else
	{
		PrintLabeledDebugString("Pixel Shader Errors:\n", (char*)errors->GetBufferPointer());
		abort();
		return false;
	}

	device->Release();

	return true;
}

#pragma endregion

#pragma region Buffers
bool GOG::DirectX11Renderer::LoadBuffers()
{
	ID3D11Device* device;
	d3d.GetDevice((void**)&device);

	D3D11_BUFFER_DESC cbMeshDesc{};
	cbMeshDesc.ByteWidth = sizeof(MeshData);
	cbMeshDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbMeshDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbMeshDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbMeshDesc.MiscFlags = 0;
	cbMeshDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA meshSubData{};
	meshSubData.pSysMem = &meshData;
	meshSubData.SysMemPitch = 0;
	meshSubData.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC cbActorMeshDesc{};
	cbActorMeshDesc.ByteWidth = sizeof(MeshData);
	cbActorMeshDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbActorMeshDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbActorMeshDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbActorMeshDesc.MiscFlags = 0;
	cbActorMeshDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA actorMeshSubData{};
	actorMeshSubData.pSysMem = &actorMeshData;
	actorMeshSubData.SysMemPitch = 0;
	actorMeshSubData.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC cbSceneDesc{};
	cbSceneDesc.ByteWidth = sizeof(SceneData);
	cbSceneDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbSceneDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbSceneDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbSceneDesc.MiscFlags = 0;
	cbSceneDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA sceneSubData{};
	sceneSubData.pSysMem = &sceneData;
	sceneSubData.SysMemPitch = 0;
	sceneSubData.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC cbInstanceDesc{};
	cbInstanceDesc.ByteWidth = sizeof(PerInstanceData);
	cbInstanceDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbInstanceDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbInstanceDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbInstanceDesc.MiscFlags = 0;
	cbInstanceDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA instanceSubData{};
	instanceSubData.pSysMem = &instanceData;
	instanceSubData.SysMemPitch = 0;
	instanceSubData.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC cbMapModelDesc{};
	cbMapModelDesc.ByteWidth = sizeof(MapModelTex);
	cbMapModelDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbMapModelDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbMapModelDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbMapModelDesc.MiscFlags = 0;
	cbMapModelDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA mapModelSubData{};
	mapModelSubData.pSysMem = &mapModelData;
	mapModelSubData.SysMemPitch = 0;
	mapModelSubData.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC sbTransformDesc = {};
	sbTransformDesc.ByteWidth = sizeof(TransformData) * levelData->transforms.size();
	sbTransformDesc.Usage = D3D11_USAGE_DYNAMIC;
	sbTransformDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	sbTransformDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	sbTransformDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	sbTransformDesc.StructureByteStride = sizeof(TransformData);

	D3D11_SUBRESOURCE_DATA transformSubData = {};
	transformSubData.pSysMem = levelData->transforms.data();
	transformSubData.SysMemPitch = 0;
	transformSubData.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC sbActorTransformDesc{};
	sbActorTransformDesc.ByteWidth = sizeof(TransformData) * instanceMax;
	sbActorTransformDesc.Usage = D3D11_USAGE_DYNAMIC;
	sbActorTransformDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	sbActorTransformDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	sbActorTransformDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	sbActorTransformDesc.StructureByteStride = sizeof(TransformData);

	D3D11_SUBRESOURCE_DATA actorTransformSubData{};
	actorTransformSubData.pSysMem = &instanceTransforms.transforms;
	actorTransformSubData.SysMemPitch = 0;
	actorTransformSubData.SysMemSlicePitch = 0;

	D3D11_BUFFER_DESC sbLightDesc{};
	sbLightDesc.ByteWidth = sizeof(LightData) * levelData->sceneLights.size();
	sbLightDesc.Usage = D3D11_USAGE_DYNAMIC;
	sbLightDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	sbLightDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	sbLightDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	sbLightDesc.StructureByteStride = sizeof(LightData);

	D3D11_SUBRESOURCE_DATA lightSubData{};
	lightSubData.pSysMem = levelData->sceneLights.data();
	lightSubData.SysMemPitch = 0;
	lightSubData.SysMemSlicePitch = 0;

	D3D11_RASTERIZER_DESC cmdesc;
	ZeroMemory(&cmdesc, sizeof(D3D11_RASTERIZER_DESC));
	cmdesc.FillMode = D3D11_FILL_SOLID;
	cmdesc.CullMode = D3D11_CULL_BACK;
	cmdesc.FrontCounterClockwise = false;

	D3D11_TEXTURE2D_DESC mapDepthStencilDesc;
	mapDepthStencilDesc.Width = screenWidth;
	mapDepthStencilDesc.Height = screenHeight;
	mapDepthStencilDesc.MipLevels = 1;
	mapDepthStencilDesc.ArraySize = 1;
	mapDepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	mapDepthStencilDesc.SampleDesc.Count = 1;
	mapDepthStencilDesc.SampleDesc.Quality = 0;
	mapDepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	mapDepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	mapDepthStencilDesc.CPUAccessFlags = 0;
	mapDepthStencilDesc.MiscFlags = 0;

	device->CreateRasterizerState(&cmdesc, cullModeState.GetAddressOf());

	device->CreateTexture2D(&mapDepthStencilDesc, nullptr, mapDepthBuffer.GetAddressOf());
	device->CreateDepthStencilView(mapDepthBuffer.Get(), nullptr, mapDepthStencil.GetAddressOf());

	if (levelData->sceneLights.data() != nullptr)
	{
		device->CreateBuffer(&sbLightDesc, &lightSubData, sLightBuffer.GetAddressOf());
	}

	device->CreateBuffer(&cbInstanceDesc, &instanceSubData, cInstanceBuffer.GetAddressOf());	
	device->CreateBuffer(&cbMeshDesc, &meshSubData, cMeshBuffer.GetAddressOf());
	device->CreateBuffer(&cbActorMeshDesc, &actorMeshSubData, cActorMeshBuffer.GetAddressOf());
	device->CreateBuffer(&cbSceneDesc, &sceneSubData, cSceneBuffer.GetAddressOf());
	device->CreateBuffer(&cbMapModelDesc, &mapModelSubData, cMapModelBuffer.GetAddressOf());
	device->CreateBuffer(&sbTransformDesc, &transformSubData, sTransformBuffer.GetAddressOf());
	device->CreateBuffer(&sbActorTransformDesc, &actorTransformSubData, sActorTransformBuffer.GetAddressOf());

	device->Release();
	return true;
}
#pragma endregion

#pragma region Textures

bool GOG::DirectX11Renderer::LoadTextures()
{
	ID3D11Device* device{};
	d3d.GetDevice((void**)&device);

	D3D11_SAMPLER_DESC texSampleDesc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());

	std::vector<std::wstring> texPaths{};

	std::wstring levelTex = L"../GameModels/Textures/Atlas_Space.dds";
	std::wstring shipsTex = L"../GameModels/Textures/Atlas_Pirate.dds";
	std::wstring bombTex = L"../GameModels/Textures/SmartBomb.dds";
	
	texPaths.push_back(levelTex);
	texPaths.push_back(shipsTex);
	texPaths.push_back(bombTex);

	DirectX::CreateDDSTextureFromFile(device, texPaths[0].c_str(), nullptr, levelView.GetAddressOf());
	DirectX::CreateDDSTextureFromFile(device, texPaths[1].c_str(), nullptr, shipsView.GetAddressOf());
	DirectX::CreateDDSTextureFromFile(device, texPaths[2].c_str(), nullptr, bombView.GetAddressOf());

	device->CreateSamplerState(&texSampleDesc, texSampler.GetAddressOf());

	device->Release();
	return true;
}

#pragma endregion 

#pragma region SRVs
bool GOG::DirectX11Renderer::LoadShaderResources()
{
	ID3D11Device* device;
	d3d.GetDevice((void**)&device);

	D3D11_TEXTURE2D_DESC mapTexDesc;
	ZeroMemory(&mapTexDesc, sizeof(D3D11_TEXTURE2D_DESC));
	mapTexDesc.Width = screenWidth;
	mapTexDesc.Height = screenHeight;
	mapTexDesc.MipLevels = 1;
	mapTexDesc.ArraySize = 1;
	mapTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	mapTexDesc.SampleDesc.Count = 1;
	mapTexDesc.Usage = D3D11_USAGE_DEFAULT;
	mapTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	mapTexDesc.CPUAccessFlags = 0;
	mapTexDesc.MiscFlags = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC mapViewDesc;
	mapViewDesc.Format = mapTexDesc.Format;
	mapViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	mapViewDesc.Texture2D.MostDetailedMip = 0;
	mapViewDesc.Texture2D.MipLevels = 1;


	D3D11_RENDER_TARGET_VIEW_DESC targetViewDesc;
	targetViewDesc.Format = mapTexDesc.Format;
	targetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	targetViewDesc.Texture2D.MipSlice = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC lightViewDesc{};
	lightViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	lightViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	lightViewDesc.BufferEx.FirstElement = 0;
	lightViewDesc.BufferEx.NumElements = levelData->sceneLights.size();

	D3D11_SHADER_RESOURCE_VIEW_DESC transViewDesc{};
	transViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	transViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	transViewDesc.BufferEx.FirstElement = 0;
	transViewDesc.BufferEx.NumElements = levelData->transforms.size();

	D3D11_SHADER_RESOURCE_VIEW_DESC actorTransViewDesc{};
	actorTransViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	actorTransViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	actorTransViewDesc.BufferEx.FirstElement = 0;
	actorTransViewDesc.BufferEx.NumElements = instanceMax;

	D3D11_RENDER_TARGET_BLEND_DESC targetBlendDesc = {};
	targetBlendDesc.BlendEnable = TRUE;
	targetBlendDesc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	targetBlendDesc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	targetBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
	targetBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
	targetBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
	targetBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	targetBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	D3D11_BLEND_DESC alphaBlendDesc = {};
	alphaBlendDesc.AlphaToCoverageEnable = FALSE;
	alphaBlendDesc.IndependentBlendEnable = FALSE;
	alphaBlendDesc.RenderTarget[0] = targetBlendDesc;
	
	device->CreateTexture2D(&mapTexDesc, nullptr, targetTexMap.GetAddressOf());
	device->CreateRenderTargetView(targetTexMap.Get(), &targetViewDesc, targetViewMap.GetAddressOf());
	device->CreateShaderResourceView(targetTexMap.Get(), &mapViewDesc, mapView.GetAddressOf());
	device->CreateShaderResourceView(sTransformBuffer.Get(),
		&transViewDesc,
		transformView.GetAddressOf());
	device->CreateShaderResourceView(sActorTransformBuffer.Get(),
		&actorTransViewDesc,
		actorTransformView.GetAddressOf());

	if (levelData->sceneLights.data() != nullptr)
	{
		device->CreateShaderResourceView(sLightBuffer.Get(), &lightViewDesc, lightView.GetAddressOf());
	}
	
	device->CreateBlendState(&alphaBlendDesc, alphaBlend.GetAddressOf());

	device->Release();
	return true;
}

#pragma endregion

#pragma region Geometry
bool GOG::DirectX11Renderer::LoadGeometry()
{
	ID3D11Device* creator;
	d3d.GetDevice((void**)&creator);

	D3D11_SUBRESOURCE_DATA lvData = { levelData->vertices.data(), 0, 0 };
	CD3D11_BUFFER_DESC lvDesc(sizeof(H2B::Vertex) * levelData->vertices.size(), D3D11_BIND_VERTEX_BUFFER);
	creator->CreateBuffer(&lvDesc, &lvData, vertexBuffer.GetAddressOf());

	D3D11_SUBRESOURCE_DATA avData = { actorData->vertices.data(), 0, 0 };
	CD3D11_BUFFER_DESC avDesc(sizeof(H2B::Vertex) * actorData->vertices.size(), D3D11_BIND_VERTEX_BUFFER);
	creator->CreateBuffer(&avDesc, &avData, actorVertexBuffer.GetAddressOf());

	D3D11_SUBRESOURCE_DATA mvData = { mapQuad.face.data(), 0, 0 };
	CD3D11_BUFFER_DESC mvDesc(sizeof(H2B::Vertex) * 4, D3D11_BIND_VERTEX_BUFFER);
	creator->CreateBuffer(&mvDesc, &mvData, mapVertexBuffer.GetAddressOf());

	D3D11_SUBRESOURCE_DATA liData = { levelData->indices.data(), 0, 0 };
	CD3D11_BUFFER_DESC liDesc(sizeof(unsigned int) * levelData->indices.size(), D3D11_BIND_INDEX_BUFFER);
	creator->CreateBuffer(&liDesc, &liData, indexBuffer.GetAddressOf());

	D3D11_SUBRESOURCE_DATA aiData = { actorData->indices.data(), 0, 0 };
	CD3D11_BUFFER_DESC aiDesc(sizeof(unsigned int) * actorData->indices.size(), D3D11_BIND_INDEX_BUFFER);
	creator->CreateBuffer(&aiDesc, &aiData, actorIndexBuffer.GetAddressOf());

	D3D11_SUBRESOURCE_DATA miData = { mapQuad.indices.data(), 0, 0 };
	CD3D11_BUFFER_DESC miDesc(sizeof(unsigned int) * 6, D3D11_BIND_INDEX_BUFFER);
	creator->CreateBuffer(&miDesc, &miData, mapIndexBuffer.GetAddressOf());

	creator->Release();
	return true;
}
#pragma endregion

#pragma region 2D Assets
bool GOG::DirectX11Renderer::Load2D()
{
	ID3D11Device* device{};
	d3d.GetDevice((void**)&device);

	GOG::PipelineHandles handles{};
	d3d.GetImmediateContext((void**)&handles.context);

	ocraExtended = std::make_unique<DirectX::SpriteFont>(device, L"../Fonts/OCR_A_Extended.spritefont");
	ocraExtendedBold = std::make_unique<DirectX::SpriteFont>(device, L"../Fonts/OCR_A_ExtendedBold.spritefont");
	courierNew = std::make_unique<DirectX::SpriteFont>(device, L"../Fonts/CourierNew.spritefont");

	m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(handles.context);

	//Logo Screen
	Microsoft::WRL::ComPtr<ID3D11Resource> teamLogoResource;
	DirectX::CreateWICTextureFromFile(device, L"../Sprites/BeesKneesLogo.png", teamLogoResource.GetAddressOf(), teamLogoView.ReleaseAndGetAddressOf());
	teamLogoResource.As(&teamLogoTex);
	CD3D11_TEXTURE2D_DESC teamLogoDesc;
	teamLogoTex->GetDesc(&teamLogoDesc);
	teamLogoOrigin.x = float(teamLogoDesc.Width / 2);
	teamLogoOrigin.y = float(teamLogoDesc.Height / 2);
	teamLogoPos.x = screenWidth / 2;
	teamLogoPos.y = screenHeight / 2;

	//DirectX Screen
	Microsoft::WRL::ComPtr<ID3D11Resource> directSplashResource;
	DirectX::CreateWICTextureFromFile(device, L"../Sprites/DirectX11Logo.png", directSplashResource.GetAddressOf(), directSplashView.ReleaseAndGetAddressOf());
	directSplashResource.As(&directSplashTex);
	CD3D11_TEXTURE2D_DESC directSplashDesc;
	directSplashTex->GetDesc(&directSplashDesc);
	directSplashOrigin.x = float(directSplashDesc.Width / 2);
	directSplashOrigin.y = float(directSplashDesc.Height / 2);
	directSplashPos.x = screenWidth / 2;
	directSplashPos.y = screenHeight / 2;

	//Gateware Screen
	Microsoft::WRL::ComPtr<ID3D11Resource> gateSplashResource;
	DirectX::CreateWICTextureFromFile(device, L"../Sprites/GatewareLogo.png", gateSplashResource.GetAddressOf(), gateSplashView.ReleaseAndGetAddressOf());
	gateSplashResource.As(&gateSplashTex);
	CD3D11_TEXTURE2D_DESC gateSplashDesc;
	gateSplashTex->GetDesc(&gateSplashDesc);
	gateSplashOrigin.x = float(gateSplashDesc.Width / 2);
	gateSplashOrigin.y = float(gateSplashDesc.Height / 2);
	gateSplashPos.x = screenWidth / 2;
	gateSplashPos.y = screenHeight / 2;

	//Flecs Screen
	Microsoft::WRL::ComPtr<ID3D11Resource> flecsSplashResource;
	DirectX::CreateWICTextureFromFile(device, L"../Sprites/FlecsLogo.png", flecsSplashResource.GetAddressOf(), flecsSplashView.ReleaseAndGetAddressOf());
	flecsSplashResource.As(&flecsSplashTex);
	CD3D11_TEXTURE2D_DESC flecsSplashDesc;
	flecsSplashTex->GetDesc(&flecsSplashDesc);
	flecsSplashOrigin.x = float(flecsSplashDesc.Width / 2);
	flecsSplashOrigin.y = float(flecsSplashDesc.Height / 2);
	flecsSplashPos.x = screenWidth / 2;
	flecsSplashPos.y = screenHeight / 2;

	//Title Screen
	Microsoft::WRL::ComPtr<ID3D11Resource> titleResource;
	DirectX::CreateWICTextureFromFile(device, L"../Sprites/GOGTitle.png", titleResource.GetAddressOf(), titleView.ReleaseAndGetAddressOf());
	titleResource.As(&titleTex);
	CD3D11_TEXTURE2D_DESC titleDesc;
	titleTex->GetDesc(&titleDesc);
	titleSpriteOrigin.x = float(titleDesc.Width / 2);
	titleSpriteOrigin.y = float(titleDesc.Height / 2);
	titleSpritePos.x = screenWidth / 2;
	titleSpritePos.y = screenHeight / 2;

	//Main Menu
	DirectX::SimpleMath::Vector2 menuTitleOrigin = ocraExtendedBold->MeasureString(L"GALLEONS\n OF THE \n GALAXY");
	menuTitleOrigin.x /= 2;
	menuTitleOrigin.y /= 2;

	DirectX::SimpleMath::Vector2 menuPlayOrigin = ocraExtendedBold->MeasureString(L" Play\n[Enter]");
	menuPlayOrigin.x /= 2;
	menuPlayOrigin.y /= 2;

	DirectX::SimpleMath::Vector2 menuCreditsOrigin = ocraExtendedBold->MeasureString(L"Credits\n  [C]");
	menuCreditsOrigin.x /= 2;
	menuCreditsOrigin.y /= 2;

	DirectX::SimpleMath::Vector2 menuHSOrigin = ocraExtendedBold->MeasureString(L"High Scores\n    [Tab]");
	menuHSOrigin.x /= 2;
	menuHSOrigin.y /= 2;

	DirectX::SimpleMath::Vector2 menuExitOrigin = ocraExtendedBold->MeasureString(L" Exit\n[Esc]");
	menuExitOrigin.x /= 2;
	menuExitOrigin.y /= 2;

	Microsoft::WRL::ComPtr<ID3D11Resource> mainMenuResource;
	DirectX::CreateWICTextureFromFile(device, L"../Sprites/MainMenu.png", mainMenuResource.GetAddressOf(), mainMenuView.ReleaseAndGetAddressOf());
	mainMenuResource.As(&mainMenuTex);
	CD3D11_TEXTURE2D_DESC mainMenuDesc;
	mainMenuTex->GetDesc(&mainMenuDesc);
	mainMenuSpriteOrigin.x = float(mainMenuDesc.Width / 2);
	mainMenuSpriteOrigin.y = float(mainMenuDesc.Height / 2);
	mainMenuSpritePos.x = screenWidth / 2;
	mainMenuSpritePos.y = screenHeight / 2;

	//Play Game
	Microsoft::WRL::ComPtr<ID3D11Resource> shipResource;
	DirectX::CreateWICTextureFromFile(device, L"../Sprites/Spaceship.png", shipResource.GetAddressOf(), spaceshipView.ReleaseAndGetAddressOf());
	shipResource.As(&spaceshipTex);
	CD3D11_TEXTURE2D_DESC shipDesc;
	spaceshipTex->GetDesc(&shipDesc);
	shipSpriteOrigin.x = float(shipDesc.Width / 2);
	shipSpriteOrigin.y = float(shipDesc.Height / 2);
	shipSpritePos.x = screenWidth * (1 / 12.0f);
	shipSpritePos.y = screenHeight * (1 / 30.0f);

	Microsoft::WRL::ComPtr<ID3D11Resource> bombResource;
	DirectX::CreateWICTextureFromFile(device, L"../Sprites/SmartBomb.png", bombResource.GetAddressOf(), bombSpriteView.ReleaseAndGetAddressOf());
	bombResource.As(&bombSpriteTex);
	CD3D11_TEXTURE2D_DESC bombSpriteDesc;
	bombSpriteTex->GetDesc(&bombSpriteDesc);
	bombSpriteOrigin.x = float(bombSpriteDesc.Width / 2);
	bombSpriteOrigin.y = float(bombSpriteDesc.Height / 2);
	bombSpritePos.x = screenWidth * (1 / 8.0f);
	bombSpritePos.y = screenHeight * (1 / 30.0f);

	DirectX::SimpleMath::Vector2 scoreOrigin = ocraExtendedBold->MeasureString(L"0");
	scoreOrigin.y /= 2;

	DirectX::SimpleMath::Vector2 wavesOrigin = ocraExtendedBold->MeasureString(L"Wave 1");
	wavesOrigin.x -= wavesOrigin.x;
	wavesOrigin.y /= 2;

	DirectX::SimpleMath::Vector2 wavesNumOrigin = ocraExtendedBold->MeasureString(L"1");
	wavesNumOrigin.x -= wavesOrigin.x;
	wavesNumOrigin.y /= 2;

	//Pause Game
	DirectX::SimpleMath::Vector2 pauseOrigin = ocraExtendedBold->MeasureString(L"Pause");
	pauseOrigin.x /= 2;
	pauseOrigin.y /= 2;

	DirectX::SimpleMath::Vector2 exitOrigin = ocraExtendedBold->MeasureString(L" Exit\n[Esc]");
	exitOrigin.x /= 2;
	exitOrigin.y /= 2;

	DirectX::SimpleMath::Vector2 resumeOrigin = ocraExtendedBold->MeasureString(L"Resume\n[Enter]");
	resumeOrigin.x /= 2;
	resumeOrigin.y /= 2;

	//Game Over
	Microsoft::WRL::ComPtr<ID3D11Resource> gameOverResource;
	DirectX::CreateWICTextureFromFile(device, L"../Sprites/GameOver.png", gameOverResource.GetAddressOf(), gameOverView.ReleaseAndGetAddressOf());
	gameOverResource.As(&gameOverTex);
	CD3D11_TEXTURE2D_DESC gameOverDesc;
	gameOverTex->GetDesc(&gameOverDesc);
	gameOverOrigin.x = float(gameOverDesc.Width / 2);
	gameOverOrigin.y = float(gameOverDesc.Height / 2);
	gameOverPos.x = screenWidth / 2;
	gameOverPos.y = screenHeight / 2;

	DirectX::SimpleMath::Vector2 retryOrigin = ocraExtendedBold->MeasureString(L"Retry\n[Enter]");
	retryOrigin.x /= 2;
	retryOrigin.y /= 2;

	DirectX::SimpleMath::Vector2 quitOrigin = ocraExtendedBold->MeasureString(L"Quit\n[Esc]");
	quitOrigin.x /= 2;
	quitOrigin.y /= 2;

	//Credits
	DirectX::SimpleMath::Vector2 creditsOrigin = ocraExtended->MeasureString(credits.text.c_str());
	creditsOrigin.x /= 2;
	creditsOrigin.y /= 2;

	//Entities
	uiWorldLock.LockSyncWrite();
	std::shared_ptr<const GameConfig> readCfg = gameConfig.lock();

	uiWorldAsync.entity("TeamLogo")
		.add<LogoScreen>()
		.set<Sprite>({ teamLogoView })
		.set<SpriteCount>({ 1 })
		.add<Center>()
		.set<Position>({})
		.set<Origin>({ teamLogoOrigin })
		.set<Scale>({ 0.7f });

	uiWorldAsync.entity("DirectSplash")
		.add<DirectXScreen>()
		.set<Sprite>({ directSplashView })
		.set<SpriteCount>({ 1 })
		.add<Center>()
		.set<Position>({})
		.set<Origin>({ directSplashOrigin })
		.set<Scale>({ 2.0f });

	uiWorldAsync.entity("GateSplash")
		.add<GatewareScreen>()
		.set<Sprite>({ gateSplashView })
		.set<SpriteCount>({ 1 })
		.add<Center>()
		.set<Position>({})
		.set<Origin>({ gateSplashOrigin })
		.set<Scale>({ 0.75f });

	uiWorldAsync.entity("FlecsSplash")
		.add<FlecsScreen>()
		.set<Sprite>({ flecsSplashView })
		.set<SpriteCount>({ 1 })
		.add<Center>()
		.set<Position>({})
		.set<Origin>({ flecsSplashOrigin })
		.set<Scale>({ 0.82f });

	uiWorldAsync.entity("Title")
		.add<TitleScreen>()
		.set<Sprite>({ titleView })
		.set<SpriteCount>({ 1 })
		.add<Center>()
		.set<Position>({})
		.set<Origin>({ titleSpriteOrigin })
		.set<Scale>({ 0.8f });

	uiWorldAsync.entity("MainMenu")
		.add<MainMenu>()
		.set<Sprite>({ mainMenuView })
		.set<SpriteCount>({ 1 })
		.add<Center>()
		.set<Position>({})
		.set<Origin>({ mainMenuSpriteOrigin })
		.set<Scale>({ 1.5f });

	uiWorldAsync.entity("MenuTitle")
		.add<MainMenu>()
		.set<Text>({ L"GALLEONS\n OF THE \n GALAXY" })
		.set<TextColor>({ DirectX::Colors::Azure })
		.add<Top>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(1.0f, 0.5f) })
		.set<Position>({})
		.set<Origin>({ menuTitleOrigin })
		.set<Scale>({ 2.0f });

	uiWorldAsync.entity("MenuPlay")
		.add<MainMenu>()
		.set<Text>({ L" Play\n[Enter]" })
		.set<TextColor>({ DirectX::Colors::Azure })
		.add<Left>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(0.35f, 0.9f) })
		.set<Position>({})
		.set<Origin>({ menuPlayOrigin })
		.set<Scale>({ 0.5f });

	uiWorldAsync.entity("MenuCredits")
		.add<MainMenu>()
		.set<Text>({ L"Credits\n  [C]" })
		.set<TextColor>({ DirectX::Colors::Azure })
		.add<Right>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(0.35f, 0.9f) })
		.set<Position>({})
		.set<Origin>({ menuCreditsOrigin })
		.set<Scale>({ 0.5f });

	/*uiWorldAsync.entity("MenuHS")
		.add<MainMenu>()
		.set<Text>({ L"High Scores\n    [Tab]" })
		.set<TextColor>({ DirectX::Colors::Azure })
		.add<Left>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(0.3f, 0.9f) })
		.set<Position>({})
		.set<Origin>({ menuHSOrigin })
		.set<Scale>({ 0.5f });	*/

	/*uiWorldAsync.entity("MenuEsc")
		.add<MainMenu>()
		.set<Text>({ L" Exit\n[Esc]" })
		.set<TextColor>({ DirectX::Colors::Azure })
		.add<Left>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(0.05f, 0.05f) })
		.set<Position>({})
		.set<Origin>({ menuExitOrigin })
		.set<Scale>({ 0.5f });*/

	uiWorldAsync.entity("Score")
		.add<PlayGame>()
		.set<Text>({ L"0" })
		.set<TextColor>({DirectX::Colors::White})
		.add<Left>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(1 / 6.0f, 1 / 12.0f) })
		.set<Position>({})
		.set<Origin>({scoreOrigin })
		.set<Scale>({ 0.7f });

	uiWorldAsync.entity("Lives")
		.add<PlayGame>()
		.set<Sprite>({ spaceshipView, })
		.set<SpriteCount>({ readCfg->at("PlayerPrefab_1").at("lives").as<unsigned int>() })
		.set<SpriteOffsets>({ DirectX::SimpleMath::Vector2(30.0f, 0.0f) })
		.add<Left>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(1 / 30.0f, 1 / 34.0f) })
		.set<Position>({})
		.set<Origin>({ shipSpriteOrigin })
		.set<Scale>({ 0.06f });

	uiWorldAsync.entity("SmartBombs")
		.add<PlayGame>()
		.set<Sprite>({ bombSpriteView, })
		.set<SpriteCount>({ 0 })
		.set<SpriteOffsets>({ DirectX::SimpleMath::Vector2(0.0f, 25.0f) })
		.add<Left>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(1 / 4.5f, 1 / 36.0f) })
		.set<Position>({})
		.set<Origin>({ bombSpriteOrigin })
		.set<Scale>({ 0.06f });

	uiWorldAsync.entity("Waves")
		.add<PlayGame>()
		.set<Text>({ L"Wave " })
		.set<TextColor>({ DirectX::Colors::White })
		.add<Right>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(1 / 4.5f, 1 / 18.0f) })
		.set<Position>({})
		.set<Origin>({ wavesOrigin })
		.set<Scale>({ 0.9f });

	uiWorldAsync.entity("WaveNumber")
		.add<PlayGame>()
		.set<Text>({ L"1" })
		.set<TextColor>({ DirectX::Colors::Red })
		.add<Right>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(1 / 9.0f, 1 / 18.0f) })
		.set<Position>({})
		.set<Origin>({ wavesOrigin })
		.set<Scale>({ 0.9f });

	uiWorldAsync.entity("Pause")
		.add<PauseGame>()
		.set<Text>({ L"Pause" })
		.set<TextColor>({ DirectX::Colors::White })
		.add<Center>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(1.0f, 1.0f) })
		.set<Position>({})
		.set<Origin>({ pauseOrigin })
		.set<Scale>({ 1.5f });

	uiWorldAsync.entity("ExitGame")
		.add<PauseGame>()
		.set<Text>({ L" Exit\n[Esc]" })
		.set<TextColor>({ DirectX::Colors::White })
		.add<Left>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(0.3f, 0.8f) })
		.set<Position>({})
		.set<Origin>({ pauseOrigin })
		.set<Scale>({ 0.7f });

	uiWorldAsync.entity("ResumeGame")
		.add<PauseGame>()
		.set<Text>({ L"Resume\n[Enter]" })
		.set<TextColor>({ DirectX::Colors::White })
		.add<Right>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(0.3f, 0.8f) })
		.set<Position>({})
		.set<Origin>({ pauseOrigin })
		.set<Scale>({ 0.7f });

	uiWorldAsync.entity("GameOver")
		.add<GameOverScreen>()
		.set<Sprite>({ gameOverView })
		.set<SpriteCount>({ 1 })
		.add<Center>()
		.set<Position>({})
		.set<Origin>({ gameOverOrigin })
		.set<Scale>({ 0.3f });

	uiWorldAsync.entity("QuitGame")
		.add<GameOverScreen>()
		.set<Text>({ L"Quit\n[Esc]" })
		.set<TextColor>({ DirectX::Colors::White })
		.add<Left>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(0.3f, 0.8f) })
		.set<Position>({})
		.set<Origin>({ pauseOrigin })
		.set<Scale>({ 0.7f });

	uiWorldAsync.entity("RetryGame")
		.add<GameOverScreen>()
		.set<Text>({ L"Retry\n[Enter]" })
		.set<TextColor>({ DirectX::Colors::White })
		.add<Right>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(0.3f, 0.8f) })
		.set<Position>({})
		.set<Origin>({ pauseOrigin })
		.set<Scale>({ 0.7f });

	uiWorldAsync.entity("Credits")
		.add<Credits>()
		.set<Text>({ credits.text.c_str()})
		.set<TextColor>({ DirectX::Colors::White })
		.add<Center>()
		.set<ResizeOffset>({ DirectX::SimpleMath::Vector2(1.0f, 1.0f) })
		.set<Position>({})
		.set<Origin>({ creditsOrigin })
		.set<Scale>({ 0.325f });

	uiWorldAsync.merge();
	uiWorldLock.UnlockSyncWrite();

	//Queries
	leftResizeQuery = uiWorld->query<const Left, const ResizeOffset, Position, Scale>();
	rightResizeQuery = uiWorld->query<const Right, const ResizeOffset, Position, Scale>();
	centerResizeQuery = uiWorld->query<const Center, Position, Scale>();
	topResizeQuery = uiWorld->query<const Top, const ResizeOffset, Position, Scale>();

	logoSpriteQuery = uiWorld->query<const LogoScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale>();
	directSpriteQuery = uiWorld->query<const DirectXScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale>();
	gateSpriteQuery = uiWorld->query<const GatewareScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale>();
	flecsSpriteQuery = uiWorld->query<const FlecsScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale>();
	titleSpriteQuery = uiWorld->query<const TitleScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale>();

	mainMenuSpriteQuery = uiWorld->query<const MainMenu, const Sprite, const SpriteCount, const Position, const Origin, const Scale>();
	mainMenuTextQuery = uiWorld->query<const MainMenu, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset>();

	playGameSpriteQuery = uiWorld->query<const PlayGame, const Sprite, const SpriteCount, const SpriteOffsets, const Position, const Origin, const Scale, const ResizeOffset>();
	playGameTextQuery = uiWorld->query<const PlayGame, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset>();

	pauseGameTextQuery = uiWorld->query<const PauseGame, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset>();

	gameOverSpriteQuery = uiWorld->query<const GameOverScreen, const Sprite, const SpriteCount, const Position, const Origin, const Scale>();
	gameOverTextQuery = uiWorld->query<const GameOverScreen, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset>();

	creditsTextQuery = uiWorld->query<const Credits, const Text, const TextColor, const Position, const Origin, const Scale, const ResizeOffset>();

	Resize(screenHeight, screenWidth);
	handles.context->Release();
	device->Release();
	return true;
}

#pragma endregion

#pragma region Pipeline
bool GOG::DirectX11Renderer::SetupPipeline()
{
	ID3D11Device* device{};
	d3d.GetDevice((void**)&device);

	D3D11_INPUT_ELEMENT_DESC attributes[3]{};

	attributes[0].SemanticName = "POSITION";
	attributes[0].SemanticIndex = 0;
	attributes[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	attributes[0].InputSlot = 0;
	attributes[0].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	attributes[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	attributes[0].InstanceDataStepRate = 0;

	attributes[1].SemanticName = "UV";
	attributes[1].SemanticIndex = 0;
	attributes[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	attributes[1].InputSlot = 0;
	attributes[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	attributes[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	attributes[1].InstanceDataStepRate = 0;

	attributes[2].SemanticName = "NORM";
	attributes[2].SemanticIndex = 0;
	attributes[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	attributes[2].InputSlot = 0;
	attributes[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	attributes[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	attributes[2].InstanceDataStepRate = 0;

	device->CreateInputLayout(attributes,
		ARRAYSIZE(attributes),
		vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(),
		vertexFormat.GetAddressOf());

	GOG::PipelineHandles handles{};
	d3d.GetImmediateContext((void**)&handles.context);
	d3d.GetRenderTargetView((void**)&handles.targetView);
	d3d.GetDepthStencilView((void**)&handles.depthStencil);

	ID3D11RenderTargetView* const views[] = { handles.targetView };
	handles.context->OMSetRenderTargets(ARRAYSIZE(views), views, handles.depthStencil);

	const UINT strides[] = { sizeof(H2B::Vertex) };
	const UINT offsets[] = { 0 };
	ID3D11Buffer* const buffs[] = { vertexBuffer.Get() };
	handles.context->IASetVertexBuffers(0, ARRAYSIZE(buffs), buffs, strides, offsets);

	handles.context->VSSetShader(vertexShader.Get(), nullptr, 0);
	handles.context->PSSetShader(pixelShader.Get(), nullptr, 0);

	UINT startSlot = 0;
	UINT numBuffers = 3;
	ID3D11Buffer* const cBuffs[]{ cMeshBuffer.Get(), cSceneBuffer.Get(), cInstanceBuffer.Get() };
	ID3D11ShaderResourceView* vsViews[]{ transformView.Get() };
	ID3D11ShaderResourceView* psViews[]
	{
		lightView.Get(),
		levelView.Get(),
		shipsView.Get(),
		bombView.Get()
	};
	ID3D11SamplerState* psSamples[]{ texSampler.Get() };

	handles.context->VSSetConstantBuffers(startSlot, numBuffers, cBuffs);
	handles.context->PSSetConstantBuffers(startSlot, numBuffers, cBuffs);
	handles.context->VSSetShaderResources(0, 1, vsViews);
	handles.context->PSSetShaderResources(1, 4, psViews);
	handles.context->PSSetSamplers(0, 1, psSamples);
	handles.context->IASetInputLayout(vertexFormat.Get());
	handles.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	handles.context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	handles.depthStencil->Release();
	handles.targetView->Release();
	handles.context->Release();
	device->Release();
	return true;
}

#pragma endregion

#pragma endregion

#pragma region Drawing Logic
void GOG::DirectX11Renderer::InitRendererSystems()
{
	flecsWorld->entity("Rendering System").add<RenderingSystem>();

	startDraw = flecsWorld->system<RenderingSystem>().kind(flecs::PreUpdate)
		.each([this](flecs::entity e, RenderingSystem& s) 
		{
			GOG::PipelineHandles handles{};
			d3d.GetImmediateContext((void**)&handles.context);
			d3d.GetRenderTargetView((void**)&handles.targetView);
			d3d.GetDepthStencilView((void**)&handles.depthStencil);

			handles.context->ClearRenderTargetView(handles.targetView, &bgColor.x);
			handles.context->ClearDepthStencilView(handles.depthStencil, D3D11_CLEAR_DEPTH, 1, 0);
			drawCounter = 0;

			handles.context->Release();
			handles.targetView->Release();
			handles.depthStencil->Release();
		});


	updateDraw = flecsWorld->system<GOG::Transform, GOG::ModelIndex>().kind(flecs::OnUpdate)
		.each([this](GOG::Transform& pos, GOG::ModelIndex& ndx) 
		{
			int i = drawCounter;

			instanceTransforms.transforms[i] = pos.value;
			instanceTransforms.modelNdxs[i] = ndx.id;

			// increment the shared draw counter but don't go over (branchless) 
			int v = static_cast<int>(instanceMax) - static_cast<int>(drawCounter + 2);
			// if v < 0 then 0, else 1
			int sign = 1 ^ ((unsigned int)v >> (sizeof(int) * CHAR_BIT - 1));
			drawCounter += sign;

			scaledMapModels.modelNdxs[i] = ndx.id;
			GW::MATH::GMatrix::ScaleLocalF(pos.value, mapModelScalar, scaledMapModels.transforms[i]);
		});


	completeDraw = flecsWorld->system<RenderingSystem>().kind(flecs::PostUpdate)
		.each([this](flecs::entity e, RenderingSystem& s) 
		{
			//Grab Pipeline Resources
			GOG::PipelineHandles handles{};
			d3d.GetImmediateContext((void**)&handles.context);
			d3d.GetRenderTargetView((void**)&handles.targetView);
			d3d.GetDepthStencilView((void**)&handles.depthStencil);

			float deltaTime;
			auto currTime = std::chrono::steady_clock::now();
			std::chrono::duration<float> _deltaTime = currTime - prevTime;
			prevTime = currTime;
			deltaTime = _deltaTime.count();

			const UINT strides[]{ sizeof(H2B::Vertex) };
			const UINT offsets[]{ 0 };

			D3D11_MAPPED_SUBRESOURCE sceneSubRes{};
			D3D11_MAPPED_SUBRESOURCE instanceSubRes{};
			D3D11_MAPPED_SUBRESOURCE meshSubRes{};
			D3D11_MAPPED_SUBRESOURCE actorTransSubRes{};
			D3D11_MAPPED_SUBRESOURCE actMeshSubRes{};
			D3D11_MAPPED_SUBRESOURCE instSubRes{};
			D3D11_MAPPED_SUBRESOURCE mapModelSubRes{};

			ID3D11Buffer* const levelVerts[]{ vertexBuffer.Get() };
			ID3D11Buffer* const actorVerts[]{ actorVertexBuffer.Get() };
			ID3D11Buffer* const mapVerts[] = { mapVertexBuffer.Get() };
			ID3D11Buffer* const cLevelBuffs[]{ cMeshBuffer.Get(), cSceneBuffer.Get(), cInstanceBuffer.Get() };
			ID3D11Buffer* const cActorBuffs[]{ cActorMeshBuffer.Get(), cSceneBuffer.Get() };
			ID3D11Buffer* const cMapModelBuffs[]{ cMapModelBuffer.Get() };

			ID3D11ShaderResourceView* vsLevelViews[]{ transformView.Get() };
			ID3D11ShaderResourceView* vsActorViews[]{ actorTransformView.Get() };
			ID3D11ShaderResourceView* psLevelViews[]
			{
					lightView.Get(),
					levelView.Get(),
					shipsView.Get(),
					bombView.Get()
			};
			ID3D11ShaderResourceView* psMapViews[]{ mapView.Get() };
			ID3D11RenderTargetView* const targetViews[] = { handles.targetView };
			D3D11_VIEWPORT prevViewport;
			UINT numViews = 1;
		
			handles.context->RSGetViewports(&numViews, &prevViewport);


			//----------Play/Pause----------

			if (currState == GAME_STATES::PLAY_GAME || currState == GAME_STATES::PAUSE_GAME || currState == GAME_STATES::GAME_OVER_SCREEN)
			{
				const FLOAT mapColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
				bgColor = currentBgColorData;

				handles.context->ClearRenderTargetView(handles.targetView, &bgColor.x);
				handles.context->ClearDepthStencilView(handles.depthStencil, D3D11_CLEAR_DEPTH, 1, 0);

				ID3D11RasterizerState* prevRasterState;
				handles.context->RSGetState(&prevRasterState);

				handles.context->OMSetRenderTargets(1, targetViewMap.GetAddressOf(), mapDepthStencil.Get());
				handles.context->RSSetViewports(1, &mapViewPort);
				handles.context->ClearRenderTargetView(targetViewMap.Get(), mapColor);
				handles.context->ClearDepthStencilView(mapDepthStencil.Get(), D3D11_CLEAR_DEPTH, 1, 0);

				handles.context->Map(cSceneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sceneSubRes);
				sceneData.viewMatrix = mapViewMatrix;
				sceneData.projectionMatrix = mapProjMatrix;
				memcpy(sceneSubRes.pData, &sceneData, sizeof(sceneData));
				handles.context->Unmap(cSceneBuffer.Get(), 0);

				handles.context->Map(sActorTransformBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &actorTransSubRes);
				memcpy(actorTransSubRes.pData, &scaledMapModels, sizeof(TransformData) * instanceMax);
				handles.context->Unmap(sActorTransformBuffer.Get(), 0);

				handles.context->PSSetShader(mapModelsPixelShader.Get(), nullptr, 0);
				handles.context->IASetVertexBuffers(0, ARRAYSIZE(actorVerts), actorVerts, strides, offsets);
				handles.context->VSSetConstantBuffers(0, 2, cActorBuffs);
				handles.context->PSSetConstantBuffers(0, 1, cMapModelBuffs);
				handles.context->VSSetShaderResources(0, 1, vsActorViews);
				handles.context->IASetIndexBuffer(actorIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

				bool isPlayer = false;
				std::string player = "Player";
				std::string enemy = "Enemy";
				std::string projectile = "Projectile";
				std::string pickup = "Pickup";

				for (int i = 0; i < drawCounter; i++)
				{
					auto& model = actorData->models[instanceTransforms.modelNdxs[i]];

					std::string recieved = model.fileName;
					size_t foundPlayer = recieved.find(player);
					size_t foundEnemy = recieved.find(enemy);
					size_t foundProjectile = recieved.find(projectile);
					size_t foundPickup = recieved.find(pickup);

					if (foundPlayer != std::string::npos)
					{
						mapModelData.modelType = 0;
					}
					else if (foundEnemy != std::string::npos)
					{
						mapModelData.modelType = 1;
					}
					else if (foundProjectile != std::string::npos)
					{
						mapModelData.modelType = 2;
					}
					else if (foundPickup != std::string::npos)
					{
						mapModelData.modelType = 3;
					}
					else
					{
						continue;
					}

					handles.context->Map(cMapModelBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapModelSubRes);
					memcpy(mapModelSubRes.pData, &mapModelData, sizeof(mapModelData));
					handles.context->Unmap(cMapModelBuffer.Get(), 0);

					handles.context->Map(cInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &instSubRes);
					instanceData.transformStart = i;
					memcpy(instSubRes.pData, &instanceData, sizeof(PerInstanceData));
					handles.context->Unmap(cInstanceBuffer.Get(), 0);

					for (int msh = 0; msh < model.meshCount; msh++)
					{
						auto& material = actorData->materials[msh + model.materialStart];
						actorMeshData.attribute = material.attrib;
						auto& mesh = actorData->meshes[msh + model.meshStart];

						handles.context->DrawIndexedInstanced(mesh.drawInfo.indexCount, 1, mesh.drawInfo.indexOffset + model.indexStart, model.vertexStart, 0);
					}
				}

				handles.context->Map(sActorTransformBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &actorTransSubRes);
				memcpy(actorTransSubRes.pData, &instanceTransforms, sizeof(TransformData) * instanceMax);
				handles.context->Unmap(sActorTransformBuffer.Get(), 0);

				handles.context->RSSetViewports(numViews, &prevViewport);
				handles.context->OMSetRenderTargets(1, targetViews, handles.depthStencil);
				handles.context->VSSetShader(levelVertexShader.Get(), nullptr, 0);
				handles.context->PSSetShader(pixelShader.Get(), nullptr, 0);
				handles.context->IASetVertexBuffers(0, ARRAYSIZE(levelVerts), levelVerts, strides, offsets);
				handles.context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
				handles.context->VSSetConstantBuffers(0, 3, cLevelBuffs);
				handles.context->PSSetConstantBuffers(0, 3, cLevelBuffs);
				handles.context->VSSetShaderResources(0, 1, vsLevelViews);
				handles.context->PSSetShaderResources(1, 4, psLevelViews);

				handles.context->ClearDepthStencilView(handles.depthStencil, D3D11_CLEAR_DEPTH, 1, 0);

				handles.context->Map(cSceneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sceneSubRes);

				sceneData = currentLevelSceneData;
				sceneData.viewMatrix = viewMatrix;
				sceneData.projectionMatrix = projectionMatrix;
				sceneData.camPos = cameraMatrix.row4;

				memcpy(sceneSubRes.pData, &sceneData, sizeof(sceneData));
				handles.context->Unmap(cSceneBuffer.Get(), 0);


				float levelSegmentOffset = (int)((cameraMatrix.row4.x + levelSegmentWidth / 2) / levelSegmentWidth);

				for (int j = 0; j < 3; j++)
				{
					meshData.offset = (levelSegmentOffset + j - 1) * levelSegmentWidth;
					handles.context->Map(cMeshBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &meshSubRes);

					memcpy(meshSubRes.pData, &meshData, sizeof(meshData));
					handles.context->Unmap(cMeshBuffer.Get(), 0);

					for (auto& i : levelData->levelInstances)
					{
						auto& model = levelData->levelModels[i.modelIndex];

						handles.context->Map(cInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &instanceSubRes);
						instanceData.transformStart = i.transformStart;
						memcpy(instanceSubRes.pData, &instanceData, sizeof(PerInstanceData));
						handles.context->Unmap(cInstanceBuffer.Get(), 0);

						for (int msh = 0; msh < model.meshCount; msh++)
						{
							handles.context->Map(cMeshBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &meshSubRes);

							auto& material = levelData->materials[msh + model.materialStart];
							meshData.attribute = material.attrib;
							auto& mesh = levelData->levelMeshes[msh + model.meshStart];
							std::string level = "Atlas_Space.dds";
							std::string ships = "Atlas_Pirate.dds";
							std::string bomb = "SmartBomb.dds";

							if (material.mapKd != NULL)
							{
								std::string recieved = material.mapKd;
								if (recieved.compare(level) == 0)
								{
									meshData.texID = 1;
								}

								if (recieved.compare(ships) == 0)
								{
									meshData.texID = 2;
								}

								if (recieved.compare(bomb) == 0)
								{
									meshData.texID = 3;
								}

							}

							memcpy(meshSubRes.pData, &meshData, sizeof(meshData));
							handles.context->Unmap(cMeshBuffer.Get(), 0);

							handles.context->DrawIndexedInstanced(mesh.drawInfo.indexCount, i.transformCount, mesh.drawInfo.indexOffset + model.indexStart, model.vertexStart, 0);
						}
					}
				}

				meshData.offset = 0;
				handles.context->Map(cMeshBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &meshSubRes);
				memcpy(meshSubRes.pData, &meshData, sizeof(meshData));
				handles.context->Unmap(cMeshBuffer.Get(), 0);

				handles.context->Map(cSceneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sceneSubRes);
				
				sceneData = currentActorSceneData;
				sceneData.viewMatrix = viewMatrix;
				sceneData.projectionMatrix = projectionMatrix;
				sceneData.camPos = cameraMatrix.row4;

				memcpy(sceneSubRes.pData, &sceneData, sizeof(sceneData));
				handles.context->Unmap(cSceneBuffer.Get(), 0);

				handles.context->VSSetShader(vertexShader.Get(), nullptr, 0);
				handles.context->IASetVertexBuffers(0, ARRAYSIZE(actorVerts), actorVerts, strides, offsets);
				handles.context->VSSetConstantBuffers(0, 2, cActorBuffs);
				handles.context->PSSetConstantBuffers(0, 2, cActorBuffs);
				handles.context->VSSetShaderResources(0, 1, vsActorViews);
				handles.context->IASetIndexBuffer(actorIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

				for (int i = 0; i < drawCounter; i++)
				{
					auto& model = actorData->models[instanceTransforms.modelNdxs[i]];

					handles.context->Map(cInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &instSubRes);
					instanceData.transformStart = i;
					memcpy(instSubRes.pData, &instanceData, sizeof(PerInstanceData));
					handles.context->Unmap(cInstanceBuffer.Get(), 0);

					for (int msh = 0; msh < model.meshCount; msh++)
					{
						handles.context->Map(cActorMeshBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &actMeshSubRes);
						auto& material = actorData->materials[msh + model.materialStart];
						actorMeshData.attribute = material.attrib;
						auto& mesh = actorData->meshes[msh + model.meshStart];
						std::string level = "Atlas_Space.dds";
						std::string ships = "Atlas_Pirate.dds";
						std::string bomb = "SmartBomb.dds";


						if (material.mapKd != NULL)
						{
							std::string recieved = material.mapKd;
							if (recieved.compare(level) == 0)
							{
								actorMeshData.texID = 1;
							}
							else if (recieved.compare(ships) == 0)
							{
								actorMeshData.texID = 2;
							}
							else if (recieved.compare(bomb) == 0)
							{
								actorMeshData.texID = 3;
							}
						}

						memcpy(actMeshSubRes.pData, &actorMeshData, sizeof(actorMeshData));
						handles.context->Unmap(cActorMeshBuffer.Get(), 0);

						handles.context->DrawIndexedInstanced(mesh.drawInfo.indexCount, 1, mesh.drawInfo.indexOffset + model.indexStart, model.vertexStart, 0);
					}
				}

				handles.context->VSSetShader(mapVertexShader.Get(), nullptr, 0);
				handles.context->PSSetShader(mapPixelShader.Get(), nullptr, 0);
				handles.context->IASetVertexBuffers(0, 1, mapVerts, strides, offsets);
				handles.context->IASetIndexBuffer(mapIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
				handles.context->PSSetShaderResources(2, 1, psMapViews);
				handles.context->RSSetState(cullModeState.Get());
				handles.context->DrawIndexed(6, 0, 0);

				handles.context->ClearRenderTargetView(targetViewMap.Get(), &bgColor.x);

				RenderGameUI();

				if (currState == GAME_STATES::PLAY_GAME || currState == GAME_STATES::PAUSE_GAME)
				{					
					handles.targetView->Release();
					handles.depthStencil->Release();
					handles.context->Release();
					return;
				}
				
			}

			ID3D11BlendState* prevBlendState;
			ID3D11DepthStencilState* prevDepthState;
			unsigned int prevStencilRef;
			D3D11_VIEWPORT previousViewport;
			unsigned int numViewports = 1;
			FLOAT prevBlendFactor[4];
			UINT prevSampleMask;
			ID3D11RasterizerState* prevRasterState;

			handles.context->RSGetState(&prevRasterState);
			handles.context->OMGetDepthStencilState(&prevDepthState, &prevStencilRef);
			handles.context->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);
			handles.context->RSGetViewports(&numViewports, &previousViewport);

			float fadeInSpeed = 1.1f;
			float scrollSpeed = 60.0f;
			DirectX::XMVECTOR whiteCopy = { 1.0f, 1.0f, 1.0f, 0.0f };
			DirectX::XMVECTOR textColorCopy;
			DirectX::XMVECTOR outlineColorCopy = { 0.53f, 0.8f, 0.9f, 0.0f };
			

			if (currState == GAME_STATES::CREDITS)
			{
				creditsOffset -= (deltaTime * scrollSpeed);
			}
			
			if (splashAlpha >= 1.0f)
			{
				fadeInDone = true;				
			}
			else
			{
				fadeInDone = false;
			}

			if (fadeInDone == false)
			{
				splashAlpha += (deltaTime * fadeInSpeed);
			}

			switch (currState)
			{
				case GAME_STATES::LOGO_SCREEN:
				{
					m_spriteBatch->Begin(DirectX::DX11::SpriteSortMode_Deferred, alphaBlend.Get());
					logoSpriteQuery.each([this, &whiteCopy](const LogoScreen&, const Sprite& _sprite, const SpriteCount& _spriteCount, const Position& _pos, const Origin& _origin, const Scale& _scale)
						{					
							whiteCopy.m128_f32[3] = splashAlpha;
							m_spriteBatch->Draw(_sprite.view.Get(), _pos.value, nullptr, whiteCopy, 0.0f, _origin.value, _scale.value * uiScalar);
						});
					break;
				}
				case GAME_STATES::DIRECTX_SCREEN:
				{
					m_spriteBatch->Begin(DirectX::DX11::SpriteSortMode_Deferred, alphaBlend.Get());
					directSpriteQuery.each([this, &whiteCopy](const DirectXScreen&, const Sprite& _sprite, const SpriteCount& _spriteCount, const Position& _pos, const Origin& _origin, const Scale& _scale)
						{							
							bgColor = black;
							whiteCopy.m128_f32[3] = splashAlpha;
							m_spriteBatch->Draw(_sprite.view.Get(), _pos.value, nullptr, whiteCopy, 0.0f, _origin.value, _scale.value * uiScalar);
						});
					break;
				}
				case GAME_STATES::GATEWARE_SCREEN:
				{
					m_spriteBatch->Begin(DirectX::DX11::SpriteSortMode_Deferred, alphaBlend.Get());
					gateSpriteQuery.each([this, &whiteCopy](const GatewareScreen&, const Sprite& _sprite, const SpriteCount& _spriteCount, const Position& _pos, const Origin& _origin, const Scale& _scale)
						{
							whiteCopy.m128_f32[3] = splashAlpha;
							m_spriteBatch->Draw(_sprite.view.Get(), _pos.value, nullptr, whiteCopy, 0.0f, _origin.value, _scale.value * uiScalar);
						});
					break;
				}
				case GAME_STATES::FLECS_SCREEN:
				{
					m_spriteBatch->Begin(DirectX::DX11::SpriteSortMode_Deferred, alphaBlend.Get());
					flecsSpriteQuery.each([this, &whiteCopy](const FlecsScreen&, const Sprite& _sprite, const SpriteCount& _spriteCount, const Position& _pos, const Origin& _origin, const Scale& _scale)
						{
							whiteCopy.m128_f32[3] = splashAlpha;
							m_spriteBatch->Draw(_sprite.view.Get(), _pos.value, nullptr, whiteCopy, 0.0f, _origin.value, _scale.value * uiScalar);
						});
					break;
				}
				case GAME_STATES::TITLE_SCREEN:
				{
					m_spriteBatch->Begin(DirectX::DX11::SpriteSortMode_Deferred, alphaBlend.Get());
					titleSpriteQuery.each([this, &whiteCopy](const TitleScreen&, const Sprite& _sprite, const SpriteCount& _spriteCount, const Position& _pos, const Origin& _origin, const Scale& _scale)
						{
							whiteCopy.m128_f32[3] = splashAlpha;
							m_spriteBatch->Draw(_sprite.view.Get(), _pos.value, nullptr, whiteCopy, 0.0f, _origin.value, _scale.value * uiScalar);
						});
					break;
				}
				case GAME_STATES::MAIN_MENU:
				{
					handles.context->ClearRenderTargetView(handles.targetView, &black.x);
					handles.context->ClearDepthStencilView(handles.depthStencil, D3D11_CLEAR_DEPTH, 1, 0);
					m_spriteBatch->Begin(DirectX::DX11::SpriteSortMode_Deferred, alphaBlend.Get());
					mainMenuSpriteQuery.each([this, &whiteCopy](const MainMenu&, const Sprite& _sprite, const SpriteCount& _spriteCount, const Position& _pos, const Origin& _origin, const Scale& _scale)
						{
							whiteCopy.m128_f32[3] = splashAlpha;
							m_spriteBatch->Draw(_sprite.view.Get(), _pos.value, nullptr, whiteCopy, 0.0f, _origin.value, _scale.value* uiScalar);
						});

					mainMenuTextQuery.each([this, &textColorCopy, &outlineColorCopy](const MainMenu&, const Text& _txt, const TextColor& _color, const Position& _pos, const Origin& _origin, const Scale& _scale, const ResizeOffset& _offset)
						{
							outlineColorCopy.m128_f32[3] = splashAlpha;
							textColorCopy = _color.value;
							textColorCopy.m128_f32[3] = splashAlpha;
							ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value + DirectX::SimpleMath::Vector2(1.0f, 1.0f), outlineColorCopy, 0.0f, _origin.value, _scale.value * uiScalar);
							ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value + DirectX::SimpleMath::Vector2(-1.0f, 1.0f), outlineColorCopy, 0.0f, _origin.value, _scale.value * uiScalar);

							ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value, textColorCopy, 0.0f, _origin.value, _scale.value * uiScalar);
						});		
					break;
				}
				case GAME_STATES::GAME_OVER_SCREEN:
				{
					m_spriteBatch->Begin(DirectX::DX11::SpriteSortMode_Deferred, alphaBlend.Get());
					gameOverSpriteQuery.each([this](const GameOverScreen&, const Sprite& _sprite, const SpriteCount& _spriteCount, const Position& _pos, const Origin& _origin, const Scale& _scale)
						{
							m_spriteBatch->Draw(_sprite.view.Get(), _pos.value, nullptr, DirectX::Colors::White, 0.0f, _origin.value, _scale.value * uiScalar);
						});

					gameOverTextQuery.each([this](const GameOverScreen&, const Text& _txt, const TextColor& _color, const Position& _pos, const Origin& _origin, const Scale& _scale, const ResizeOffset& _offset)
						{
							ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value, _color.value, 0.0f, _origin.value, _scale.value * uiScalar);

						});

					break;
				}
				case GAME_STATES::HIGH_SCORES:
				{
					break;
				}
				case GAME_STATES::CREDITS:
				{
					handles.context->ClearRenderTargetView(handles.targetView, &black.x);
					handles.context->ClearDepthStencilView(handles.depthStencil, D3D11_CLEAR_DEPTH, 1, 0);
					m_spriteBatch->Begin(DirectX::DX11::SpriteSortMode_Deferred, alphaBlend.Get());
					creditsTextQuery.each([this](const Credits&, const Text& _txt, const TextColor& _color, const Position& _pos, const Origin& _origin, const Scale& _scale, const ResizeOffset& _offset)
						{		
							creditsPosCopy = _pos.value;
							creditsPosCopy.m128_f32[1] = creditsOffset;
							ocraExtended->DrawString(m_spriteBatch.get(), _txt.value.c_str(), creditsPosCopy, DirectX::Colors::White, 0.0f, _origin.value, _scale.value * uiScalar);

						});
					break;
				}
				default:
					break;
			}

			m_spriteBatch->End();

			handles.context->OMSetRenderTargets(numViewports, targetViews, handles.depthStencil);
			handles.context->RSSetState(prevRasterState);
			handles.context->OMSetDepthStencilState(prevDepthState, prevStencilRef);
			handles.context->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);
			handles.context->RSSetViewports(numViewports, &previousViewport);

			Restore3DStates(handles);

			// Release references to the previous state
			if (prevDepthState) prevDepthState->Release();
			if (prevBlendState) prevBlendState->Release();

			handles.depthStencil->Release();
			handles.targetView->Release();
			handles.context->Release();
		});
		
}

void GOG::DirectX11Renderer::RenderGameUI()
{
	ID3D11Device* device{};
	d3d.GetDevice((void**)&device);

	GOG::PipelineHandles handles{};
	d3d.GetImmediateContext((void**)&handles.context);
	d3d.GetRenderTargetView((void**)&handles.targetView);
	d3d.GetDepthStencilView((void**)&handles.depthStencil);

	float flickerSpeed = 5.0f;
	float deltaTime;
	auto currTime = std::chrono::steady_clock::now();
	std::chrono::duration<float> _deltaTime = currTime - prevTime;
	prevTime = currTime;
	deltaTime = _deltaTime.count();
	
	// Save the current states
	ID3D11BlendState* prevBlendState; 

	ID3D11DepthStencilState* prevDepthState;
	unsigned int prevStencilRef;
	D3D11_VIEWPORT previousViewport;
	unsigned int numViewports = 1;
	FLOAT prevBlendFactor[4];
	UINT prevSampleMask;
	ID3D11RasterizerState* prevRasterState;

	handles.context->RSGetState(&prevRasterState);
	handles.context->OMGetDepthStencilState(&prevDepthState, &prevStencilRef);
	handles.context->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);
	handles.context->RSGetViewports(&numViewports, &previousViewport);

	m_spriteBatch->SetViewport(previousViewport);

	m_spriteBatch->Begin(DirectX::DX11::SpriteSortMode_Deferred, alphaBlend.Get());

	playGameTextQuery.each([this](const PlayGame&, const Text& _txt, const TextColor& _color, const Position& _pos, const Origin& _origin, const Scale& _scale, const ResizeOffset&)
		{
			ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value, _color.value, 0.0f, _origin.value, _scale.value * uiScalar);
			
		});

	playGameSpriteQuery.each([this](const PlayGame&, const Sprite& _sprite, const SpriteCount& _spriteCount, const SpriteOffsets& _offsets, const Position& _pos, const Origin& _origin, const Scale& _scale, const ResizeOffset&)
		{
			DirectX::SimpleMath::Vector2 spritePosCopy = _pos.value;

			for (int i = 0; i < _spriteCount.numSprites; i++)
			{
				m_spriteBatch->Draw(_sprite.view.Get(), spritePosCopy, nullptr, DirectX::Colors::White, 0.0f, _origin.value, _scale.value * uiScalar);
				spritePosCopy.x += (_offsets.values.x * uiScalar);
				spritePosCopy.y += (_offsets.values.y * uiScalar);
			}
		});
	
	if (currState == GAME_STATES::PAUSE_GAME)
	{
		DirectX::XMVECTOR colorCopy;
		DirectX::XMVECTOR outlineCopy = {0.1f, 0.1f, 0.1f, 0.0f};

		if (pauseAlpha >= 1.0f)
		{
			maxReached = true;
			minReached = false;
		}

		if (pauseAlpha <= 0.2f)
		{
			maxReached = false;
			minReached = true;
		}

		if (maxReached)
		{
			pauseAlpha -= (deltaTime * flickerSpeed);
		}

		if (minReached)
		{
			pauseAlpha += (deltaTime * flickerSpeed);
		}
		
		pauseGameTextQuery.each([this, &colorCopy, &outlineCopy](const PauseGame&, const Text& _txt, const TextColor& _color, const Position& _pos, const Origin& _origin, const Scale& _scale, const ResizeOffset&)
			{
				
				if (_txt.value.compare(L"Pause") == 0)
				{
					colorCopy = _color.value;
					colorCopy.m128_f32[3] = pauseAlpha;
					outlineCopy.m128_f32[3] = pauseAlpha;

					ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value + DirectX::SimpleMath::Vector2(1.0f, 1.0f), outlineCopy, 0.0f, _origin.value, _scale.value * uiScalar);
					ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value + DirectX::SimpleMath::Vector2(-1.0f, 1.0f), outlineCopy, 0.0f, _origin.value, _scale.value * uiScalar);
					ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value + DirectX::SimpleMath::Vector2(-1.0f, -1.0f), outlineCopy, 0.0f, _origin.value, _scale.value * uiScalar);
					ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value + DirectX::SimpleMath::Vector2(1.0f, -1.0f), outlineCopy, 0.0f, _origin.value, _scale.value * uiScalar);

					ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value, colorCopy, 0.0f, _origin.value, _scale.value * uiScalar);
				}
				else
				{
					ocraExtendedBold->DrawString(m_spriteBatch.get(), _txt.value.c_str(), _pos.value, _color.value, 0.0f, _origin.value, _scale.value * uiScalar);
				}
				
			});
	}

	m_spriteBatch->End();

	// Restore the previous state
	
	handles.context->RSSetState(prevRasterState);
	handles.context->OMSetDepthStencilState(prevDepthState, prevStencilRef);
	handles.context->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);
	handles.context->RSSetViewports(numViewports, &previousViewport);

	Restore3DStates(handles);

	// Release references to the previous state
	if (prevDepthState) prevDepthState->Release();
	if (prevBlendState) prevBlendState->Release();

	handles.depthStencil->Release();
	handles.targetView->Release();
	handles.context->Release();

	device->Release();
}

void GOG::DirectX11Renderer::Restore3DStates(PipelineHandles& handles)
{
	const UINT strides[] = { sizeof(H2B::Vertex) };
	const UINT offsets[] = { 0 };
	ID3D11Buffer* const buffs[] = { vertexBuffer.Get() };

	UINT startSlot = 0;
	UINT numBuffers = 3;
	ID3D11Buffer* const cBuffs[]{ cMeshBuffer.Get(), cSceneBuffer.Get(), cInstanceBuffer.Get() };
	ID3D11ShaderResourceView* vsViews[]{ transformView.Get() };
	ID3D11ShaderResourceView* psViews[]
	{
			lightView.Get(),
			levelView.Get(),
			shipsView.Get(),
			bombView.Get()
	};

	ID3D11SamplerState* psSamples[]{ texSampler.Get() };

	handles.context->PSSetSamplers(0, 1, psSamples);
	handles.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	handles.context->IASetInputLayout(vertexFormat.Get());
	handles.context->VSSetShader(vertexShader.Get(), nullptr, 0);
	handles.context->PSSetShader(pixelShader.Get(), nullptr, 0);
	handles.context->IASetVertexBuffers(0, ARRAYSIZE(buffs), buffs, strides, offsets);
	handles.context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	handles.context->VSSetConstantBuffers(startSlot, numBuffers, cBuffs);
	handles.context->PSSetConstantBuffers(startSlot, numBuffers, cBuffs);
	handles.context->VSSetShaderResources(0, 1, vsViews);
	handles.context->PSSetShaderResources(1, 4, psViews);

}

#pragma endregion

#pragma region Flecs Systems
bool GOG::DirectX11Renderer::Activate(bool _runSystems)
{
	if (startDraw.is_alive() && updateDraw.is_alive() && completeDraw.is_alive())
	{
		if (_runSystems)
		{
			startDraw.enable();
			updateDraw.enable();
			completeDraw.enable();
		}
		else
		{
			startDraw.disable();
			updateDraw.disable();
			completeDraw.disable();
		}

		return true;
	}

	return false;
}

bool GOG::DirectX11Renderer::Shutdown()
{
	startDraw.destruct();
	updateDraw.destruct();
	completeDraw.destruct();
	bombEffect = nullptr;

	leftResizeQuery.destruct();
	rightResizeQuery.destruct();
	centerResizeQuery.destruct();
	topResizeQuery.destruct();

	logoSpriteQuery.destruct();
	directSpriteQuery.destruct();
	gateSpriteQuery.destruct();
	flecsSpriteQuery.destruct();
	titleSpriteQuery.destruct();

	mainMenuSpriteQuery.destruct();
	mainMenuTextQuery.destruct();

	playGameSpriteQuery.destruct();
	playGameTextQuery.destruct();
	pauseGameTextQuery.destruct();

	flecsWorld->entity("QueryPlayerTransforms").destruct();
	ocraExtended.reset();
	ocraExtendedBold.reset();
	courierNew.reset();
	m_spriteBatch.reset();
	return true;
}

#pragma endregion

#pragma region Update

void GOG::DirectX11Renderer::UpdateGameState(GAME_STATES state)
{
	currState = state;
}

void GOG::DirectX11Renderer::UpdateCamera()
{
	float deltaTime;
	auto currTime = std::chrono::steady_clock::now();
	std::chrono::duration<float> _deltaTime = currTime - prevTime;
	prevTime = currTime;
	deltaTime = _deltaTime.count();

	const float camSpeed = 2.0f;
	float deltaX;
	float deltaY;
	float deltaZ;

	float upValue;
	float downValue;
	float forwardValue;
	float backValue;
	float leftValue;
	float rightValue;

	float mouseX;
	float mouseY;

	unsigned int windowHeight;
	window.GetHeight(windowHeight);

	unsigned int windowWidth;
	window.GetWidth(windowWidth);

	float aspect;
	d3d.GetAspectRatio(aspect);

	float fov = 65.0f * 3.14f / 180.0f;
	float pitch;
	float yaw;

	GW::GReturn mouseCheck;

	input.GetState(G_KEY_SPACE, upValue);
	input.GetState(G_KEY_LEFTSHIFT, downValue);
	input.GetState(G_KEY_W, forwardValue);
	input.GetState(G_KEY_A, leftValue);
	input.GetState(G_KEY_S, backValue);
	input.GetState(G_KEY_D, rightValue);
	mouseCheck = input.GetMouseDelta(mouseX, mouseY);

	if (mouseCheck == GW::GReturn::REDUNDANT)
	{
		mouseX = 0;
		mouseY = 0;
	}

	GW::MATH::GMatrix::InverseF(viewMatrix, viewMatrix);
	cameraMatrix = viewMatrix;

	deltaX = rightValue - leftValue;
	deltaY = upValue - downValue;
	deltaZ = forwardValue - backValue;

	pitch = fov * mouseY / windowHeight;
	yaw = fov * aspect * mouseX / windowWidth;

	GW::MATH::GVECTORF yTrans = { 0.0f, (deltaY * camSpeed * deltaTime), 0.0f, 0.0f };
	GW::MATH::GMatrix::TranslateGlobalF(cameraMatrix, yTrans, cameraMatrix);

	GW::MATH::GVECTORF transXZ = { (deltaX * camSpeed * deltaTime), 0, (deltaZ * camSpeed * deltaTime) };
	GW::MATH::GMatrix::TranslateLocalF(cameraMatrix, transXZ, cameraMatrix);

	GW::MATH::GMatrix::RotateXLocalF(cameraMatrix, pitch, cameraMatrix);
	GW::MATH::GMatrix::RotateYGlobalF(cameraMatrix, yaw, cameraMatrix);

	GW::MATH::GMatrix::InverseF(cameraMatrix, viewMatrix);
}

void GOG::DirectX11Renderer::UpdateCamera(GW::MATH::GMATRIXF camWorld)
{
	cameraMatrix = camWorld;
	GW::MATH::GMatrix::InverseF(camWorld, viewMatrix);
}

void GOG::DirectX11Renderer::UpdateStats(unsigned int _lives, unsigned int _score, unsigned int _smartbombs,
	unsigned int _waves)
{
	UpdateLives(_lives);
	UpdateScore(_score);
	UpdateSmartBombs(_smartbombs);
	UpdateWaves(_waves);
}

void GOG::DirectX11Renderer::UpdateLives(unsigned int _lives)
{
	uiWorldLock.LockSyncWrite();
	uiWorldAsync.entity("Lives")
		.set<SpriteCount>({ _lives });
	uiWorldLock.UnlockSyncWrite();
}

void GOG::DirectX11Renderer::UpdateScore(unsigned int _score)
{
	std::wstring newScore = std::to_wstring(_score);
	DirectX::SimpleMath::Vector2 scoreOrigin = ocraExtendedBold->MeasureString(newScore.c_str());

	scoreOrigin.y /= 2;

	uiWorldLock.LockSyncWrite();
	uiWorldAsync.entity("Score")
		.set<Text>({ newScore })
		.set<Origin>({ scoreOrigin });
	uiWorldLock.UnlockSyncWrite();
}

void GOG::DirectX11Renderer::UpdateSmartBombs(unsigned int _smartBombs)
{
	uiWorldLock.LockSyncWrite();
	uiWorldAsync.entity("SmartBombs")
		.set<SpriteCount>({ _smartBombs });
	uiWorldLock.UnlockSyncWrite();
}

void GOG::DirectX11Renderer::UpdateWaves(unsigned int _waves)
{
	std::wstring newWave = std::to_wstring(_waves);
	DirectX::SimpleMath::Vector2 waveOrigin = ocraExtendedBold->MeasureString(newWave.c_str());
	waveOrigin.x -= waveOrigin.x;
	waveOrigin.y /= 2;

	uiWorldLock.LockSyncWrite();
	uiWorldAsync.entity("WaveNumber")
		.set<Text>({ newWave })
		.set<Origin>({ waveOrigin });
	uiWorldLock.UnlockSyncWrite();
}

void GOG::DirectX11Renderer::UpdateMiniMap()
{
	mapViewMatrix = viewMatrix;
}

void GOG::DirectX11Renderer::Resize(unsigned int height, unsigned int width)
{

	leftResizeQuery.each([height, width](const Left&, const ResizeOffset& _offset, Position& _pos, Scale& _scale)
	{
			_pos.value.x = width * _offset.value.x;
			_pos.value.y = height * _offset.value.y;
			
	});

	rightResizeQuery.each([height, width](const Right&, const ResizeOffset& _offset, Position& _pos, Scale& _scale)
	{
			_pos.value.x = width - (width * _offset.value.x);
			_pos.value.y = height * _offset.value.y;
	});

	centerResizeQuery.each([height, width](const Center&, Position& _pos, Scale& _scale)
		{
			_pos.value.x = width / 2;
			_pos.value.y = height / 2;
		});

	topResizeQuery.each([height, width](const Top&, const ResizeOffset& _offset, Position& _pos, Scale& _scale)
		{
			_pos.value.x = width / 2;
			_pos.value.y = height * _offset.value.y;
		});
}

#pragma endregion

#pragma region Helper Functions
Quad GOG::DirectX11Renderer::CreateQuad()
{
	Quad quad;
	//Create the vertex buffer
	std::vector<H2B::Vertex> v =
	{
		{{-0.5f, 0.75f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f }},
		{{-0.5f,  1.0f, 0.0f}, { 0.0f, 0.0f, 1.0f} ,{0.0f,  0.0f, 0.0f}},
		{{0.5f,  0.75f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f,  0.0f, 0.0f}},
		{{0.5f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}},
	};

	std::vector<unsigned int> indices = {
		{0},  {1},  {2},
		{2},  {1},  {3},
	};

	quad.face = v;
	quad.indices = indices;

	return quad;

}

std::string GOG::DirectX11Renderer::ReadFileIntoString(const char* _filePath)
{
	std::string output;
	unsigned int stringLength = 0;
	GW::SYSTEM::GFile file;

	file.Create();
	file.GetFileSize(_filePath, stringLength);

	if (stringLength > 0 && +file.OpenBinaryRead(_filePath))
	{
		output.resize(stringLength);
		file.Read(&output[0], stringLength);
	}
	else
		std::cout << "ERROR: File \"" << _filePath << "\" Not Found!" << std::endl;

	return output;
}

void GOG::DirectX11Renderer::PrintLabeledDebugString(const char* _label, const char* _toPrint)
{
	std::cout << _label << _toPrint << std::endl;
#if defined WIN32 //OutputDebugStringA is a windows-only function 
	OutputDebugStringA(_label);
	OutputDebugStringA(_toPrint);
#endif
}

void GOG::DirectX11Renderer::InitCredits()
{
	std::wstring creditsPart1 =
	{
		LR"(      ----------GALLEONS OF THE GALAXY----------
      
      Lead Programmer              Keith Babcock

      Gameplay Programmer          Clayton Jetton

      Graphics Programmer          Antonio Roldan

      Generalist Programmer        Mason Miller



      Assets:

           PlayerLazer- Laser: Pixabay
           https://pixabay.com/sound-effects/laser-14729/

           BaiterMissle- a shot: Pixbay
           https://pixabay.com/sound-effects/a-shot-106498/
           
           BomberTrap- Lasergun: Pixabay
           https://pixabay.com/sound-effects/lasergun-100580/
           
           BaiterDeath- Impact: Finntastico
           https://pixabay.com/sound-effects/impact-152508/
           
           BomberDeath- Explosion: Pixabay
           https://pixabay.com/sound-effects/explosion-47163/
           
           EnemyThreeDeath- Small Explosion: Pixabay
           https://pixabay.com/sound-effects/small-explosion-106769/
           
           PlayerDeathJingle- Bass Drop: Shrawangaikwad
           https://pixabay.com/sound-effects/bass-drop-186087/
           
           PlayerAccel- Machine Static: Pixabay
           https://pixabay.com/sound-effects/machine-static-45995/
           
           PickupBomb- Game Start: Pixabay
           https://pixabay.com/sound-effects/game-start-6104/
           
           Pause- Interface: UNIVERSFIELD
           https://pixabay.com/sound-effects/interface-124464/
           
           Menu- Button: UNIVERSFIELD
           https://pixabay.com/sound-effects/button-124476/
           
           SmartBomb_Explosion- A.I
           Generated with MyEdit https://myedit.online/en/audio-editor/ai-sound-effect-generator
           
           GameOver- A.I
           Generated with MyEdit https://myedit.online/en/audio-editor/ai-sound-effect-generator
           
           CivilianSaved- Game Start: Pixabay
           https://pixabay.com/sound-effects/game-start-6104/
           
           CivilianSaved2- success_bell: Pixabay
           https://pixabay.com/sound-effects/success-bell-6776/
           
           CivilianLost- Gam FX #9: Pixabay
           https://pixabay.com/sound-effects/game-fx-9-40197/
           
           CivilianPicked- Retro Game sfx_jump bump: Pixabay
           https://pixabay.com/sound-effects/retro-game-sfx-jump-bumpwav-14853/
           
           CivilianPicked2- 90s Game IO 9: floraphonic
           https://pixabay.com/sound-effects/90s-game-ui-9-185102/
           
           CivilianVoice- Game Character: UNIVERSFIELD
           https://pixabay.com/sound-effects/game-character-140506/
           
           CivilianVoice2- Cute Character Wee 1: floraphonic
           https://pixabay.com/sound-effects/cute-character-wee-1-188162/
           
           NewHishScore- Good!: Pixabay
           https://pixabay.com/sound-effects/good-6081/
           
           HighScoreJingle WinFantasia: Pixabay
           https://pixabay.com/sound-effects/winfantasia-6912/
           
           PlayerAccel2- A.I
           Generated with MyEdit https://myedit.online/en/audio-editor/ai-sound-effect-generator
           
           ScoreBlip- Rising Funny Game Effect: UNIVERSFIELD
           https://pixabay.com/sound-effects/rising-funny-game-effect-132474/
           
           BombExplode- large explosion 1: Pixabay
           https://pixabay.com/sound-effects/large-explosion-1-43636/
           
           BombExplode2- explosion: Pixabay
           https://pixabay.com/sound-effects/explosion-107629/
           
           BaiterDeath2- Impact: Finnastico
           https://pixabay.com/sound-effects/impact-152508/
           
           BaiterMissle2- weapon01: Pixabay
           https://pixabay.com/sound-effects/weapon01-47681/
           
           LanderShoot- Gun Shot: Pixabay
           https://pixabay.com/sound-effects/gun-shot-56254/
           
           LanderShoot2- pam pam: Pixabay
           https://pixabay.com/sound-effects/pam-pam-45500/
           
           LanderDeath: Musket Explosion: Pixabay
           https://pixabay.com/sound-effects/musket-explosion-6383/
           
           PlayerShoot2- shoot 5: Pixabay
           https://pixabay.com/sound-effects/shoot-5-102360/


      )"
	};

	std::wstring creditsPart2 = {
		LR"(Music:
      
           PirateSynth- Pirate: FluffyPanda755
           https://pixabay.com/music/synthwave-pirate-153391/
           
           PirateUpbeat- Winter White - Canadian Folk Fiddle Accordion Original Composition
           https://pixabay.com/music/folk-winter-white-canadian-folk-fiddle-accordion-original-composition-177387/
           
           Water- Water: 3rdEyeSage
           https://pixabay.com/music/upbeat-water-161101/
           
           
           GameplayMusic2- Buccaneer -- Swashbuckler Pirate Instrumental High Seas Adventure: melodyayresgriffiths
           https://pixabay.com/music/upbeat-buccaneer-swashbuckler-pirate-instrumental-high-seas-adventure-132858/
           
           CreditsMusic- Angelical Synth: lucadialessandro
           https://pixabay.com/sound-effects/angelical-synth-194316/
           
           PirateMusic- Dread Pirate Roberts - sea shanty dance EDM soundtrack: melodyayresgriffiths
           https://pixabay.com/music/folk-dread-pirate-roberts-sea-shanty-dance-edm-soundtrack-153022/
      
      
      
      2D Assets:
      
           DirectX11 Logo:
           
           https://forums.daybreakgames.com/ps2/index.php?threads/directx-11-why-not.162542/
           
           Gateware Logo:
           
           https://gitlab.com/gateware-development
           
           Flecs Logo:
           
           https://ajmmertens.medium.com/flecs-3-0-is-out-5ca1ac92e8a4
           
           Space Background
           
           https://www.gamedevmarket.net/asset/full-hd-space-background
           
           Game Over Sprite
           
           https://pngtree.com/freepng/pixelate-game-over-word-text-effect_5982580.html
           
           Smart Bomb  Sprite
           
           https://www.pngwing.com/en/search?q=explosion+Sprite
           
           Spaceship Sprite
           
           https://dribbble.com/shots/16278019-spaceships
           
           Team Logo/ GOG Title
           
           AI generated by DALL-E
      
      
      
      3D Assets:
      
           Pirate Kit- quaternius
           https://sketchfab.com/3d-models/pirate-kit-70-models-52af2bc5ac5846ff84a0d671b897c0a2
           
           Ultimate Space Kit- quaternius
           https://sketchfab.com/3d-models/ultimate-space-kit-84c108ff2bcf4d4cbf2adff74a942822
           
           Low Poly World earth with clouds Free low-poly 3D model: jurassicdinomax123
           https://cgtrader.com/free-3d-models/space/planet/low-poly-world-fe3c5327-7635-4e35-b07d-8da07cd2610b
           
           Low poly stone path Free low-poly 3D model: kacarskigt
           https://www.cgtrader.com/free-3d-models/exterior/street-exterior/low-poly-stone-path
           
           Starry Sky: Francesco Ungaro
           https://www.pexels.com/photo/starry-sky-998641/
           
           a close up of a rock surface with small cracks: Colin Watts
           https://unsplash.com/photos/a-close-up-of-a-rock-surface-with-small-cracks-u4ijcCaprRc
      
      
      
      Special thanks to:                    Jennifer Jospeh
                                            Ryan Brown
                                            Dan Fernandez
                                            Bradley Leffler
                                            Justin Edwards
      )"
	};

	credits.text = creditsPart1 + creditsPart2;
}

#pragma endregion