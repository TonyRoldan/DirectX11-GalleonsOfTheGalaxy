// define all ECS components related to drawing
#ifndef VISUALS_H
#define VISUALS_H

// example space game (avoid name collisions)
namespace GOG
{
	struct ModelIndex { unsigned int id; };

	struct Color { GW::MATH2D::GVECTOR3F value; };

	struct Material 
	{
		Color diffuse = { 1, 1, 1 };
	};
};

#endif