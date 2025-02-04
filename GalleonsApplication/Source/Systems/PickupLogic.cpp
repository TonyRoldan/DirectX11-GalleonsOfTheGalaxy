#include "PickupLogic.h"

#include "../Components/Identification.h"
#include "../Components/Gameplay.h"
#include "../Components/Physics.h"

#include "../Utils/SharedActorMethods.h"

#include <random>

using namespace GOG;
using namespace flecs;
using namespace GW;
using namespace CORE;
using namespace MATH;

bool PickupLogic::Init(	std::shared_ptr<world> _flecsWorld, 
						std::weak_ptr<const GameConfig> _gameConfig, 
						GEventGenerator _eventPusher)
{
	flecsWorld = _flecsWorld;
	gameConfig = _gameConfig;
	eventPusher = _eventPusher;

	std::shared_ptr<const GameConfig> readCfg = gameConfig.lock();
	float worldBottom = readCfg->at("Game").at("worldBottomBoundry").as<float>();

	civiSystem = flecsWorld->system<const Civilian, CaptureInfo, Transform, FlipInfo, CiviMovementStats, const Offset>().each(
		[worldBottom](entity _civilian, const Civilian&, CaptureInfo& _captureInfo, Transform& _transform, 
			FlipInfo& _flipInfo, CiviMovementStats& _movement, const Offset& _offset)
		{
			switch (_captureInfo.captured)
			{
				case false:
				{
					// Gravity
					_transform.value.row4.y -= _civilian.delta_time() * _movement.speed;

					// Walk on the ground
					if (_transform.value.row4.y <= worldBottom)
					{
						// Check if we need to change directions, otherwise keep walking.
						auto timeDifNano = std::chrono::high_resolution_clock::now() - _movement.lastDirChange;
						auto timeDifMilli = std::chrono::duration_cast<std::chrono::milliseconds>(timeDifNano);
						if (timeDifMilli >= _movement.curDirChangeInterval)
						{
							_movement.lastDirChange = std::chrono::high_resolution_clock::now();
							switch (_movement.isWalkingRight)
							{
								case false:
								{
									_movement.isWalkingRight = true;
									break;
								}
								case true:
								{
									_movement.isWalkingRight = false;
									break;
								}
							}

							std::random_device civiSeed;
							std::mt19937 civiGen(civiSeed());
							unsigned int intervalMin = _movement.dirChangIntervalMin;
							unsigned int intervalMax = _movement.dirChangIntervalMax;
							std::uniform_int_distribution<unsigned int> intervalDist(intervalMin, intervalMax);
							std::chrono::milliseconds curInterval(intervalDist(civiGen));
							_movement.curDirChangeInterval = curInterval;
						}
						else
						{
							// Walk left or right
							switch (_movement.isWalkingRight)
							{
								case false:
								{
									_transform.value.row4.x -= _civilian.delta_time() * _movement.speed;
									SharedActorMethods::FlipEntity(_civilian.delta_time(),
										1,
										_transform,
										_flipInfo);
									break;
								}
								case true:
								{
									_transform.value.row4.x += _civilian.delta_time() * _movement.speed;
									SharedActorMethods::FlipEntity(_civilian.delta_time(),
										-1,
										_transform,
										_flipInfo);
									break;
								}
							}
						}
					}

					break;
				}
				case true:
				{
					// If the captor has been destroyed, then release the civilian.
					if (_captureInfo.captor.is_valid() == false)
					{
						_captureInfo.captured = false;
						return;
					}

					// Otherwise, make the civilain hang off its captor's ship.
					GVECTORF hangPos = _captureInfo.captor.get<Transform>()->value.row4;
					hangPos.y -= _offset.value;
					_transform.value.row4 = hangPos;

					break;
				}
			}
		});

	return true;
}

bool PickupLogic::Activate(bool _runSystem)
{
	if (_runSystem)
	{
		civiSystem.enable();
	}
	else
	{
		civiSystem.disable();
	}

	return true;
}

bool PickupLogic::Shutdown()
{
	civiSystem.destruct();

	return true;
}
