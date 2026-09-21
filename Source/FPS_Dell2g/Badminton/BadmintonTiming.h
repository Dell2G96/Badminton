#pragma once
#include "CoreMinimal.h"
#include "BadmintonTypes.h"

namespace Badminton
{
	inline float TimingQuality(double Offset)
	{
		if (!FMath::IsFinite(Offset)) { return 0.f; }
		return 1.f - FMath::Clamp(static_cast<float>((FMath::Abs(Offset) - .045) / .20), 0.f, 1.f);
	}
	inline FVector PlacementTarget(int32 Side, int32 Score, bool bServe, const FVector2D& Aim)
	{
		const float Sign = ForwardSign(Side);
		const float Depth = FMath::Lerp(bServe ? ShortServiceLine + 65.f : 140.f, HalfLength - 90.f, static_cast<float>((Aim.Y + 1.) * .5));
		const float Lateral = bServe ? -ServeY(Side, Score) + Sign * Aim.X * 64.f : Sign * Aim.X * (HalfWidth - 40.f);
		return FVector(Sign * Depth, Lateral, 5.f);
	}
	inline FVector InstantPlacementTarget(EBadmintonShot Shot, int32 Side, int32 Score, const FVector2D& Aim)
	{
		FVector Target = PlacementTarget(Side, Score, Shot == EBadmintonShot::Serve, Aim);
		const float Alpha = FMath::Clamp(static_cast<float>((Aim.Y + 1.) * .5), 0.f, 1.f);
		if (Shot == EBadmintonShot::Drop) { Target.X = ForwardSign(Side) * FMath::Lerp(160.f, 285.f, Alpha); }
		if (Shot == EBadmintonShot::Hairpin) { Target.X = ForwardSign(Side) * FMath::Lerp(65.f, 135.f, Alpha); }
		return Target;
	}
	inline FVector ScatterTarget(const FVector& Target, int32 Side, bool bServe, float Quality, int32 Seed)
	{
		FRandomStream Random(Seed);
		const float Error = (1.f - FMath::Clamp(Quality, 0.f, 1.f)) * (bServe ? 30.f : 85.f);
		FVector Result = Target + FVector(Random.FRandRange(-Error, Error), Random.FRandRange(-Error, Error), 0);
		Result.X = ForwardSign(Side) * FMath::Clamp(Result.X * ForwardSign(Side), bServe ? ShortServiceLine + 10.f : 40.f, HalfLength - 10.f);
		Result.Y = FMath::Clamp(Result.Y, -HalfWidth + 10.f, HalfWidth - 10.f);
		return Result;
	}
	inline float TimingFlightScale(float Quality) { return FMath::Lerp(1.12f, .88f, FMath::Clamp(Quality, 0.f, 1.f)); }
	inline const TCHAR* ShotLabel(EBadmintonShot Shot)
	{
		switch (Shot)
		{
		case EBadmintonShot::Serve: return TEXT("서브");
		case EBadmintonShot::Drop: return TEXT("드롭");
		case EBadmintonShot::Smash: return TEXT("스매시");
		case EBadmintonShot::Receive: return TEXT("리시브");
		case EBadmintonShot::Hairpin: return TEXT("헤어핀");
		default: return TEXT("클리어");
		}
	}
}
