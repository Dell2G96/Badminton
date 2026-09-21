#pragma once
#include "BadmintonTypes.h"
#include "BadmintonShotData.h"
namespace Badminton
{
	enum class EContactHint : uint8 { Ready, SelectShot, WaitReturn, Recovering, LowStamina, TooFar, TooHigh, TooLow, AimMiss, ServicePosition, TooLate };
	const TCHAR* ContactHintText(EContactHint Hint);
	EContactHint ContactGeometry(int32 Side, const FVector& Player, const FVector& Shuttle, const FBadmintonShotParameters& Params);
	bool RacketAimMatches(EBadmintonShot Shot, const FVector& Direction, const FVector& ToShuttle);
	bool PredictContact(int32 Side, EBadmintonShot Shot, const FBadmintonShotParameters& Params,
		FVector Position, FVector Velocity, const FVector& Player, const FVector& PlayerVelocity, float& Seconds);
	FVector DefensiveTarget(int32 Side, const FVector& Target);
	EBadmintonShot ChooseTacticalShot(float OpponentDepth, float Height, float Stamina, const UBadmintonShotData& Data);
	struct FPracticeProgress
	{
		int32 Rally = INDEX_NONE, Sequence = INDEX_NONE, FinishedRally = INDEX_NONE;
		int32 RallyHits = 0, BestRally = 0, Targets = 0, ComboPoints = 0;
		EBadmintonShot LastOwnShot = EBadmintonShot::Serve;
		bool bCombo = false;
		void Contact(int32 RallyId, int32 ShotSequence, int32 Hitter, int32 LocalSide, EBadmintonShot Shot);
		bool Landing(int32 RallyId, int32 Winner, int32 Hitter, int32 LocalSide, EBadmintonShot Shot, const FVector& Position);
		FVector Target(int32 Side) const;
	};
}
