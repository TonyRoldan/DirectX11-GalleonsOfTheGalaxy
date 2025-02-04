#include "CameraLogic.h"
#include "../Utils/Macros.h"

bool GOG::CameraLogic::Init(std::shared_ptr<flecs::world> _flecsWorld,
	std::weak_ptr<const GameConfig> _gameConfig, DirectX11Renderer* _renderer)
{
	flecsWorld = _flecsWorld;
	renderer = _renderer;

	std::shared_ptr<const GameConfig> readCfg = _gameConfig.lock();

	// Camera entity creation
	isFacingRight = true;
	smoothing = readCfg->at("Camera").at("smoothing").as<float>();
	leadDistance = readCfg->at("Camera").at("leadDistance").as<float>();
	switchDirTime = readCfg->at("Camera").at("switchDirTime").as<int>();
	float rotX = readCfg->at("Camera").at("rotX").as<float>();
	float rotY = readCfg->at("Camera").at("rotY").as<float>();
	float rotZ = readCfg->at("Camera").at("rotZ").as<float>();
	float posX = readCfg->at("Camera").at("posX").as<float>();
	float posY = readCfg->at("Camera").at("posY").as<float>();
	float posZ = readCfg->at("Camera").at("posZ").as<float>();

	defaultPosition = { GW::MATH::GIdentityMatrixF };
	GW::MATH::GMatrix::RotateXLocalF(defaultPosition, G_DEGREE_TO_RADIAN_F(rotX), defaultPosition);
	GW::MATH::GMatrix::RotateYLocalF(defaultPosition, G_DEGREE_TO_RADIAN_F(rotY), defaultPosition);
	GW::MATH::GMatrix::RotateZLocalF(defaultPosition, G_DEGREE_TO_RADIAN_F(rotZ), defaultPosition);
	defaultPosition.row4.x = posX;
	defaultPosition.row4.y = posY;
	defaultPosition.row4.z = posZ;

	zOffset = posZ;

	flecsWorld->entity("Camera")
		.add<Camera>()
		.set_override<Transform>({ defaultPosition });

	// Camera movement system creation
	queryCache = flecsWorld->query<Player, Transform, FlipInfo>();

	movementSystem = flecsWorld->system<Camera, Transform>("CameraMovementSystem").each([this](
		flecs::entity _entity, Camera, Transform& _transform)
		{

			unsigned int test = queryCache.count();
			if (queryCache.count() > 0) // lead the player
			{
				Transform targetTransform = *queryCache.first().get<Transform>();
				FlipInfo targetFlipInfo = *queryCache.first().get<FlipInfo>();
				GW::MATH::GVECTORF targetPosition = targetTransform.value.row4;
				targetPosition.z += zOffset;
				

				// Switching lead distance
				float targetLeadDistance = leadDistance;

				if (isFacingRight != targetFlipInfo.isFacingRight)
				{
					auto systemTime = std::chrono::system_clock::now().time_since_epoch();
					int now = std::chrono::duration_cast<std::chrono::milliseconds>(systemTime).count();

					if (!isSwitchingDir)
					{
						switchDirStart = now;
						isSwitchingDir = true;
					}

					float ratio = (now - switchDirStart) / (float)switchDirTime;

					if (ratio < 1)
					{
						targetLeadDistance = G_LERP(-leadDistance, leadDistance, ratio);
					}
					else
					{
						isFacingRight = targetFlipInfo.isFacingRight;
					}
				}
				else
					isSwitchingDir = false;


				// Lead distance
				targetPosition.x += targetLeadDistance * (targetFlipInfo.isFacingRight ? 1 : -1);
				targetPosition.y = _transform.value.row4.y;

				GW::MATH::GVector::LerpF(
					_transform.value.row4,
					targetPosition,
					smoothing * _entity.delta_time(),
					_transform.value.row4);

				renderer->UpdateCamera(_transform.value);
			}

		});

	return true;
}

void GOG::CameraLogic::Reset()
{
	flecsWorld->entity("Camera")
		.set<Transform>({ defaultPosition });
}

bool GOG::CameraLogic::Activate(bool runSystem)
{
	if (movementSystem.is_alive()) {
		(runSystem) ?
			movementSystem.enable()
			: movementSystem.disable();
		return true;
	}
	return false;
}

bool GOG::CameraLogic::Shutdown()
{

	movementSystem.destruct();
	queryCache.destruct();
	flecsWorld.reset();
	gameConfig.reset();

	return true;
}