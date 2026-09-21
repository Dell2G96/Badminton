#pragma once

#include "CoreMinimal.h"
#include "BadmintonTypes.generated.h"

UENUM(BlueprintType)
enum class EBadmintonPhase : uint8
{
	WaitingForPlayers,
	WaitingForReady,
	ReadyToServe,
	Rally,
	RallyComplete,
	MatchFinished
};

namespace Badminton
{
	constexpr int32 PointsToWin = 11;
	constexpr float ShortServiceLine = 198.f;
	constexpr int32 MaxPlayers = 2;
	constexpr float HalfLength = 670.f;
	constexpr float HalfWidth = 259.f;
	constexpr float SpawnDistance = 440.f;
	constexpr float PlayerHeight = 96.f;

	inline float ForwardSign(const int32 Side)
	{
		return Side == 1 ? -1.f : 1.f;
	}

	inline FTransform SpawnTransform(const int32 Side)
	{
		return FTransform(FRotator(0.f, Side == 1 ? 180.f : 0.f, 0.f),
			FVector(-ForwardSign(Side) * SpawnDistance, 0.f, PlayerHeight));
	}

	inline float ServeY(int32 Side, int32 Score)
	{
		return ForwardSign(Side) * (Score % 2 == 0 ? 1.f : -1.f) * HalfWidth * .5f;
	}
}

UENUM(BlueprintType)
enum class EBadmintonShot : uint8 { Serve, Clear, Drop, Smash, Receive };

UENUM(BlueprintType)
enum class EBadmintonPointReason : uint8 { None, LandedIn, Out, ServiceFault, InvalidCrossing };
