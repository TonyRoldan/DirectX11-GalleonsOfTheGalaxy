#ifndef LIGHTS_H
#define LIGHTS_H

namespace GOG
{
	enum LIGHT_TYPE { 
		point = 1,
		spot = 2
	};

	struct Light {};

	struct LightType { LIGHT_TYPE value; };
	struct LightColor { GW::MATH::GVECTORF value; };
	struct LightOffset { GW::MATH::GVECTORF value; };
	struct LightRadius { float value; };
}

#endif
#pragma once
