// define all ECS components related to movement & collision
#ifndef PHYSICS_H
#define PHYSICS_H

// example space game (avoid name collisions)
namespace GOG 
{
	// ECS component types should be *strongly* typed for proper queries
	// typedef is tempting but it does not help templates/functions resolve type
	struct Transform { GW::MATH::GMATRIXF value; };
	struct Offset { float value; };
	struct BoundBox { GW::MATH::GOBBF collider; };
	struct Velocity { GW::MATH::GVECTORF value; };
	struct Acceleration { GW::MATH::GVECTORF value; };
	struct Speed { float value; };

	// Individual TAGs
	struct Collidable {};
	
	// ECS Relationship tags
	struct CollidedWith {};
};

#endif