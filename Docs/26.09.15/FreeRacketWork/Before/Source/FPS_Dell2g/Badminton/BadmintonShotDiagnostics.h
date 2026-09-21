#pragma once

#include "BadmintonNetMetrics.h"
#include "BadmintonGameState.h"
#include "BadmintonPlayerState.h"
#include "BadmintonShuttle.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/Pawn.h"

namespace Badminton
{
	// Local logs only. Prediction keys correlate an accepted activation across peers;
	// QPC timestamps can be compared only for processes on the same Windows machine.
	inline void TraceShot(const TCHAR* Stage, const UAbilitySystemComponent* ASC, const APawn* Pawn,
		EBadmintonShot Shot, int32 Key = 0, int32 Result = -1)
	{
		if (!NetMetricsEnabled() || !Pawn) { return; }
		const UWorld* World = Pawn->GetWorld();
		const auto* Match = World->GetGameState<ABadmintonGameState>();
		const auto* Player = Pawn->GetPlayerState<ABadmintonPlayerState>();
		const ABadmintonShuttle* Shuttle = nullptr;
		for (TActorIterator<ABadmintonShuttle> It(World); It; ++It) { Shuttle = *It; break; }
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_SHOT_FLOW stage=%s qpc=%.9f auth=%d side=%d rally=%d sequence=%d shot=%d key=%d result=%d phase=%d distance_cm=%.3f height_cm=%.3f tags=%s"),
			Stage, FPlatformTime::Seconds(), Pawn->HasAuthority(), Player ? Player->CourtSide : -1,
			Match ? Match->RallyId : -1, Shuttle ? Shuttle->GetFlight().ShotSequence : -1,
			static_cast<int32>(Shot), Key, Result, Match ? static_cast<int32>(Match->Phase) : -1,
			Shuttle ? FVector::Dist2D(Pawn->GetActorLocation(), Shuttle->GetActorLocation()) : -1.,
			Shuttle ? Shuttle->GetActorLocation().Z : -1., ASC ? *ASC->GetOwnedGameplayTags().ToStringSimple() : TEXT("None"));
	}
}
