#pragma once
#include "CoreMinimal.h"
#include "BadmintonTypes.h"
namespace Badminton
{
	constexpr float MinimapHalfWidth = 475.f;
	constexpr float MinimapHalfLength = 760.f;
	// A fixed court view with the local player below the net; altitude does not move the ground marker.
	inline FVector2D MinimapPoint(const FVector& Position, int32 LocalSide)
	{
		const float Sign = ForwardSign(LocalSide);
		return FVector2D(.5 + Position.Y * Sign / (2. * MinimapHalfWidth), .5 - Position.X * Sign / (2. * MinimapHalfLength));
	}
}
