#pragma once
#include "CoreMinimal.h"
#include "BadmintonTypes.h"
namespace Badminton
{
	constexpr float RacketTiltLimit = 30.f;
	constexpr float RacketTiltStep = 3.f;
	constexpr double RacketRestitution = .75;
	constexpr double RacketTangentialRetention = .2;
	inline float AdjustRacketTilt(float Current, float WheelSteps)
	{
		return FMath::IsFinite(WheelSteps) ? FMath::Clamp(Current + WheelSteps * RacketTiltStep, -RacketTiltLimit, RacketTiltLimit) : Current;
	}
	inline FVector RacketTiltAxis(const FVector& FaceNormal)
	{
		FVector Axis = FVector::CrossProduct(FaceNormal, FVector::UpVector).GetSafeNormal();
		return Axis.IsNearlyZero() ? FVector(0,-1,0) : Axis;
	}
	inline FVector TiltedRacketNormal(const FVector& FaceNormal, float Degrees)
	{
		const FVector Normal = FaceNormal.GetSafeNormal();
		return Normal.RotateAngleAxis(FMath::Clamp(Degrees, -RacketTiltLimit, RacketTiltLimit), RacketTiltAxis(Normal));
	}
	// A moving racket surface: restitution along its normal and friction along its face.
	// Calibrate the base swing velocity so zero wheel angle preserves each existing stroke.
	inline FVector RacketSurfaceVelocity(const FVector& Incoming, const FVector& Desired, const FVector& Normal)
	{
		const double InNormal = FVector::DotProduct(Incoming, Normal), OutNormal = FVector::DotProduct(Desired, Normal);
		return Normal * ((OutNormal + RacketRestitution * InNormal)/(1. + RacketRestitution))
			+ ((Desired - Normal * OutNormal) - RacketTangentialRetention * (Incoming - Normal * InNormal))/(1. - RacketTangentialRetention);
	}
	inline FVector ResolveRacketImpact(const FVector& Incoming, const FVector& Desired, const FVector& FaceNormal, float Degrees, EBadmintonShot Shot)
	{
		if (Shot == EBadmintonShot::Smash || FMath::IsNearlyZero(Degrees)) { return Desired; }
		if (!FMath::IsFinite(Degrees) || Incoming.ContainsNaN() || Desired.ContainsNaN() || FaceNormal.ContainsNaN() || FaceNormal.IsNearlyZero()) { return Desired; }
		const FVector BaseNormal = FaceNormal.GetSafeNormal(), Normal = TiltedRacketNormal(BaseNormal, Degrees);
		const FVector Surface = RacketSurfaceVelocity(Incoming, Desired, BaseNormal);
		const FVector Relative = Incoming - Surface;
		const double NormalSpeed = FVector::DotProduct(Relative, Normal);
		if (NormalSpeed >= 0.) { return Incoming; } // A separating face applies no new impulse.
		return Surface + RacketTangentialRetention * (Relative - Normal * NormalSpeed) - RacketRestitution * NormalSpeed * Normal;
	}
}
