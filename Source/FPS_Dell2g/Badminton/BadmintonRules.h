#pragma once

#include "BadmintonTypes.h"

struct FBadmintonPointDecision
{
	int32 Winner = INDEX_NONE;
	EBadmintonPointReason Reason = EBadmintonPointReason::None;
};

namespace Badminton
{
	inline bool IsInCourt(const FVector& Position)
	{
		return !Position.ContainsNaN() && FMath::Abs(Position.X) <= HalfLength && FMath::Abs(Position.Y) <= HalfWidth;
	}

	inline bool IsInServiceBox(const FVector& Position, int32 ServerSide, int32 ServerScore)
	{
		return IsInCourt(Position) && Position.X * ForwardSign(ServerSide) >= ShortServiceLine
			&& Position.Y * ServeY(ServerSide, ServerScore) <= 0.f;
	}

	inline FBadmintonPointDecision JudgeLanding(const FVector& Position, int32 LastHitter, bool bServe, int32 ServerScore, bool bLegalCrossing)
	{
		if (LastHitter != 0 && LastHitter != 1) { return {}; }
		if (!bLegalCrossing) { return {1 - LastHitter, EBadmintonPointReason::InvalidCrossing}; }
		if (!IsInCourt(Position)) { return {1 - LastHitter, EBadmintonPointReason::Out}; }
		if (bServe && !IsInServiceBox(Position, LastHitter, ServerScore)) { return {1 - LastHitter, EBadmintonPointReason::ServiceFault}; }
		return {Position.X < 0.f ? 1 : 0, EBadmintonPointReason::LandedIn};
	}
}
