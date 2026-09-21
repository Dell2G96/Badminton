#pragma once
#include "BadmintonShuttle.h"
#include "BadmintonTiming.h"
#include "BadmintonPlayFeel.h"
namespace Badminton
{
	// Shared by the authoritative hit and the live direction preview.
	inline FVector PreparePlayerFlight(EBadmintonShot Shot, int32 Side, const FVector& Start, FVector& Target,
		float BaseDuration, float Quality, int32 Seed, float& Duration)
	{
		Duration = Shot == EBadmintonShot::Clear ? 2.1f : BaseDuration;
		if (Shot == EBadmintonShot::Receive)
		{
			Duration = FMath::Max(1.5f, BaseDuration);
			Target = DefensiveTarget(Side, Target);
		}
		else if (Shot == EBadmintonShot::Smash)
		{
			Target = ScatterTarget(Target, Side, false, Quality, Seed);
			Duration *= TimingFlightScale(Quality);
		}
		for (int32 Attempt = 0; Attempt < 60; ++Attempt)
		{
			FVector Position = Start, Velocity = ABadmintonShuttle::SolveVelocity(Start, Target, Duration);
			const double Decay = 1. + .15 * Start.X / Velocity.X;
			const float CrossTime = Decay > 0. && Decay < 1. ? static_cast<float>(-FMath::Loge(Decay) / .15) : -1.f;
			if (CrossTime > 0.f)
			{
				ABadmintonShuttle::AdvanceFlight(Position, Velocity, CrossTime);
				if (Position.Z >= (Shot == EBadmintonShot::Hairpin ? 162.f : 175.f)) { break; }
			}
			Duration += .025f;
		}
		return ABadmintonShuttle::SolveVelocity(Start, Target, Duration);
	}
}
