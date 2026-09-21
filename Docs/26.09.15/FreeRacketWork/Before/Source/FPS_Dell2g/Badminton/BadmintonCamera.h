#pragma once

#include "CoreMinimal.h"
#include "BadmintonTiming.h"

namespace Badminton
{
	constexpr double CameraYawLimit = 65.;
	constexpr double CameraPitchMin = -35.;
	constexpr double CameraPitchMax = 75.;
	constexpr double CameraDefaultPitch = -12.;

	inline FVector2D ApplyMouseLook(FVector2D Look, FVector2D Delta, float Sensitivity)
	{
		if (Delta.ContainsNaN() || !FMath::IsFinite(Sensitivity)) { return Look; }
		// Mouse delta already measures movement this frame, so no DeltaTime multiplier.
		return FVector2D(FMath::Clamp(Look.X + Delta.X * Sensitivity, -CameraYawLimit, CameraYawLimit),
			FMath::Clamp(Look.Y + Delta.Y * Sensitivity, CameraPitchMin, CameraPitchMax));
	}

	inline FRotator CourtCameraRotation(int32 Side, FVector2D Look)
	{
		return FRotator(Look.Y, (Side == 1 ? 180. : 0.) + Look.X, 0.);
	}

	inline FVector2D AimFromCamera(int32 Side, int32 Score, bool bServe, const FVector& Eye, const FRotator& View)
	{
		const FVector Direction = View.Vector();
		const double Sign = ForwardSign(Side);
		// Intersect the gaze with court height; above the horizon, use the far landing boundary.
		const double FarTime = FMath::Max(0., (Sign * (HalfLength - 90.) - Eye.X) / Direction.X);
		const double GroundTime = Direction.Z < -.001 ? FMath::Max(0., (5. - Eye.Z) / Direction.Z) : FarTime;
		const FVector Point = Eye + Direction * FMath::Min(GroundTime, FarTime);
		const double NearDepth = bServe ? ShortServiceLine + 65. : 140.;
		const double Depth = FMath::Clamp(Point.X * Sign, NearDepth, HalfLength - 90.);
		const double Lateral = bServe ? (Point.Y + ServeY(Side, Score)) / (Sign * 64.) : Point.Y / (Sign * (HalfWidth - 40.));
		return FVector2D(FMath::Clamp(Lateral, -1., 1.), 2. * (Depth - NearDepth) / (HalfLength - 90. - NearDepth) - 1.);
	}
}
