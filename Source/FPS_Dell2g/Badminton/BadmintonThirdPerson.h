#pragma once

#include "BadmintonPlayerFlight.h"

namespace Badminton
{
	constexpr float ThirdPersonTimingWindow = .32f;
	constexpr float ThirdPersonPerfectWindow = .09f;
	inline float ThirdPersonTimingQuality(float Seconds)
	{
		if (!FMath::IsFinite(Seconds)) { return 0.f; }
		// A reachable, on-time return remains useful; precision still improves power and placement.
		const float Error = FMath::Clamp((FMath::Abs(Seconds) - ThirdPersonPerfectWindow)
			/ (ThirdPersonTimingWindow - ThirdPersonPerfectWindow), 0.f, 1.f);
		return FMath::Lerp(1.f, .35f, Error);
	}
	inline bool IsDropFamily(EBadmintonShot Shot)
	{
		return Shot == EBadmintonShot::Drop || Shot == EBadmintonShot::Hairpin;
	}
	inline EBadmintonShot ResolveDropShot(int32 Side, const FVector& PlayerPosition)
	{
		if ((Side != 0 && Side != 1) || PlayerPosition.ContainsNaN()) { return EBadmintonShot::Drop; }
		const float Depth = -PlayerPosition.X * ForwardSign(Side);
		// The short service line is the visible boundary; the line itself belongs to the front court.
		return Depth >= 0.f && Depth <= ShortServiceLine ? EBadmintonShot::Hairpin : EBadmintonShot::Drop;
	}
	inline float ContactHeight(EBadmintonShot Shot, const FBadmintonShotParameters& Params)
	{
		const float Height = Shot == EBadmintonShot::Smash ? 265.f : Shot == EBadmintonShot::Hairpin ? 115.f
			: Shot == EBadmintonShot::Receive ? 150.f : 205.f;
		return FMath::Clamp(Height, Params.MinimumHeight + 5.f, Params.MaximumHeight - 5.f);
	}
	// Signed future time to the descending contact plane, using the shuttle's drag/gravity model.
	inline bool DescendingContactTime(const FVector& Position, const FVector& Velocity, float Height, float& Seconds)
	{
		Seconds = 0.f;
		if (Position.ContainsNaN() || Velocity.ContainsNaN() || !FMath::IsFinite(Height)) { return false; }
		const double ApexArgument = 1. + .15 * Velocity.Z / 980.;
		const float Apex = ApexArgument > 0. ? static_cast<float>(FMath::Loge(ApexArgument) / .15) : -.35f;
		float Low = FMath::Max(-.35f, Apex), High = 3.f;
		auto HeightAt = [&](float Time)
		{
			FVector Point = Position, Speed = Velocity;
			ABadmintonShuttle::AdvanceFlight(Point, Speed, Time);
			return Point.Z;
		};
		if (Low >= High || HeightAt(Low) < Height || HeightAt(High) > Height) { return false; }
		for (int32 Index = 0; Index < 16; ++Index)
		{
			const float Mid = (Low + High) * .5f;
			if (HeightAt(Mid) > Height) { Low = Mid; } else { High = Mid; }
		}
		Seconds = (Low + High) * .5f;
		return true;
	}
	inline EContactHint ThirdPersonContact(int32 Side, EBadmintonShot Shot, const FBadmintonShotParameters& Params,
		const FVector& Player, const FVector& Position, const FVector& Velocity, float& Quality, float& Seconds)
	{
		Quality = 0.f;
		Seconds = 0.f;
		if (Velocity.ContainsNaN()) { return EContactHint::WaitReturn; }
		const EContactHint Geometry = ContactGeometry(Side, Player, Position, Params);
		if (Geometry != EContactHint::Ready) { return Geometry; }
		if (Velocity.Z >= -1.) { return EContactHint::TooHigh; }
		if (!DescendingContactTime(Position, Velocity, ContactHeight(Shot, Params), Seconds)) { return EContactHint::TooLate; }
		if (Seconds > ThirdPersonTimingWindow) { return EContactHint::TooHigh; }
		if (Seconds < -ThirdPersonTimingWindow) { return EContactHint::TooLate; }
		Quality = ThirdPersonTimingQuality(Seconds);
		return EContactHint::Ready;
	}
	inline FVector ThirdPersonTarget(EBadmintonShot Shot, int32 Side, int32 Score, const FVector2D& Aim)
	{
		FVector Target = InstantPlacementTarget(Shot, Side, Score, Aim);
		return Shot == EBadmintonShot::Receive ? DefensiveTarget(Side, Target) : Target;
	}
	inline FVector PrepareThirdPersonFlight(EBadmintonShot Shot, int32 Side, const FVector& Position, FVector& Target,
		float BaseTime, float Quality, int32 Seed, float& Duration)
	{
		const float ClampedQuality = FMath::Clamp(Quality, 0.f, 1.f);
		// First find a net-safe arc for the selected stroke, then apply timing power and dispersion.
		PreparePlayerFlight(Shot, Side, Position, Target, BaseTime, 1.f, Seed, Duration);
		if (Shot != EBadmintonShot::Serve)
		{
			Duration *= FMath::Lerp(1.28f, 1.f, ClampedQuality);
			FRandomStream Random(Seed);
			const float Error = (1.f - ClampedQuality) * 95.f;
			Target += FVector(ForwardSign(Side) * Random.FRandRange(-Error, Error), Random.FRandRange(-Error, Error), 0.);
			// Do not clamp dispersion back into the court; weak timing can go out.
		}
		return ABadmintonShuttle::SolveVelocity(Position, Target, Duration);
	}
}
