#pragma once


namespace GOG
{
	class SharedActorMethods
	{
	public:
		static void FlipEntity(float _deltaTime, float _dirMoving, GOG::Transform& _transform, GOG::FlipInfo& _flipInfo)
		{
			// If the dir we want to move (_dirMoving) is different from isFacingRight, start a flip
			if ((_flipInfo.isFacingRight && _dirMoving < 0)
				|| (!_flipInfo.isFacingRight && _dirMoving > 0))
			{
				_flipInfo.isFacingRight = _dirMoving > 0;

				if (_flipInfo.flipDir == 0) // If we're not flipping (0) start flipping left or right
					_flipInfo.flipDir = _dirMoving > 0 ? 1 : -1;
			}

			if (_flipInfo.flipDir != 0)
			{
				// Get the amount to rotate this frame w/o a direction
				float rotAmount = 180
					* (1000.0f / _flipInfo.flipTime)
					* _deltaTime;

				if (_flipInfo.flipDir == 1) // If rotating towards the right
				{
					rotAmount = -rotAmount; // - rotAmount because we're going from 180 -> 0
					_flipInfo.degreesFlipped += rotAmount;

					if (_flipInfo.degreesFlipped < 0) // if we've overshot 0 degrees, correct the difference
					{
						rotAmount -= _flipInfo.degreesFlipped;
						_flipInfo.degreesFlipped = 0;

						if (_flipInfo.isFacingRight)	// if we're currently wanting to go right after flip is done, stop flipping
							_flipInfo.flipDir = 0;
						else							// else start flipping back left
							_flipInfo.flipDir = -1;
					}
				}
				else // Else rotating towards the left
				{
					_flipInfo.degreesFlipped += rotAmount; // + rotAmount because we're going 0 -> 180

					if (_flipInfo.degreesFlipped > 180) // if we've overshot 180 degrees, correct the difference
					{
						rotAmount -= _flipInfo.degreesFlipped - 180;
						_flipInfo.degreesFlipped = 180;

						if (!_flipInfo.isFacingRight)	// if we're currently wanting to go left after flip is done, stop flipping
							_flipInfo.flipDir = 0;
						else							// else start flipping back right
							_flipInfo.flipDir = 1;
					}
				}

				GW::MATH::GMatrix::RotateYLocalF(_transform.value, G_DEGREE_TO_RADIAN(rotAmount), _transform.value);
			}
		}
	};
}

