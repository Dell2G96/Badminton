#include "BadmintonGameMode.h"

#include "BadmintonCharacter.h"
#include "BadmintonAIController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "BadmintonCourt.h"
#include "BadmintonGameState.h"
#include "BadmintonHUD.h"
#include "BadmintonPlayerController.h"
#include "BadmintonRacketPhysics.h"
#include "BadmintonPlayerFlight.h"
#include "BadmintonThirdPerson.h"
#include "BadmintonPlayerState.h"
#include "BadmintonShuttle.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "BadmintonRules.h"
#include "BadmintonShotData.h"
#include "BadmintonTiming.h"
#include "BadmintonPlayFeel.h"
#include "BadmintonAttributeSet.h"
#include "BadmintonNetMetrics.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABadmintonGameMode::ABadmintonGameMode()
{
	DefaultPawnClass = ABadmintonCharacter::StaticClass();
	PlayerControllerClass = ABadmintonPlayerController::StaticClass();
	PlayerStateClass = ABadmintonPlayerState::StaticClass();
	GameStateClass = ABadmintonGameState::StaticClass();
	HUDClass = ABadmintonHUD::StaticClass();
}

void ABadmintonGameMode::InitGameState()
{
	Super::InitGameState();
	if (!TActorIterator<ABadmintonCourt>(GetWorld()))
	{
		GetWorld()->SpawnActor<ABadmintonCourt>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	Shuttle = GetWorld()->SpawnActor<ABadmintonShuttle>(FVector(-390, 0, 160), FRotator::ZeroRotator);
	Shuttle->ResetForServe(FVector(-390, 0, 160), 0);
}

void ABadmintonGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty() && GetNumPlayers() >= Badminton::MaxPlayers)
	{
		ErrorMessage = TEXT("BadmintonRoomFull");
	}
}

FString ABadmintonGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!Error.IsEmpty())
	{
		return Error;
	}
	bool bOccupied[Badminton::MaxPlayers] = {false, false};
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		const ABadmintonPlayerState* State = Cast<ABadmintonPlayerState>(Entry);
		if (State && State != NewPlayerController->PlayerState && State->CourtSide >= 0 && State->CourtSide < Badminton::MaxPlayers)
		{
			bOccupied[State->CourtSide] = true;
		}
	}
	ABadmintonPlayerState* NewState = NewPlayerController->GetPlayerState<ABadmintonPlayerState>();
	for (int32 Side = 0; Side < Badminton::MaxPlayers; ++Side)
	{
		if (NewState && !bOccupied[Side])
		{
			NewState->CourtSide = Side;
			return FString();
		}
	}
	return TEXT("BadmintonRoomFull");
}

void ABadmintonGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	const ABadmintonPlayerState* State = NewPlayer->GetPlayerState<ABadmintonPlayerState>();
	if (State && State->CourtSide != INDEX_NONE && !NewPlayer->GetPawn())
	{
		RestartPlayerAtTransform(NewPlayer, Badminton::SpawnTransform(State->CourtSide));
	}
	SpawnSoloOpponent();
	RefreshLobby();
}

void ABadmintonGameMode::SpawnSoloOpponent()
{
	if (SoloOpponent || GetNetMode() != NM_Standalone || UGameplayStatics::HasOption(OptionsString, TEXT("NoAI"))
		|| FParse::Param(FCommandLine::Get(), TEXT("BadmintonEOS"))) { return; }
	// A local human has already reserved side zero through InitNewPlayer.
	if (GameState->PlayerArray.Num() != 1) { return; }
	SoloOpponent = GetWorld()->SpawnActor<ABadmintonAIController>();
	ABadmintonPlayerState* State = SoloOpponent ? SoloOpponent->GetPlayerState<ABadmintonPlayerState>() : nullptr;
	if (!State)
	{
		UE_LOG(LogTemp, Error, TEXT("BADMINTON_AI_SPAWN_FAILED missing PlayerState"));
		if (SoloOpponent) { SoloOpponent->Destroy(); SoloOpponent = nullptr; }
		return;
	}
	State->CourtSide = 1;
	State->SetIsABot(true);
	State->SetPlayerName(TEXT("연습 상대"));
	State->bReady = true;
	RestartPlayerAtTransform(SoloOpponent, Badminton::SpawnTransform(1));
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_AI_SPAWNED side=1 pawn=%d"), SoloOpponent->GetPawn() != nullptr);
}

void ABadmintonGameMode::Logout(AController* Exiting)
{
	GetWorldTimerManager().ClearTimer(ServeResetTimer);
	if (APawn* Pawn = Exiting->GetPawn())
	{
		Pawn->Destroy();
	}
	if (Exiting->PlayerState)
	{
		GameState->RemovePlayerState(Exiting->PlayerState);
	}
	Super::Logout(Exiting);
	ABadmintonGameState* Match = GetGameState<ABadmintonGameState>();
	++Match->RallyId;
	Match->Score0 = Match->Score1 = 0;
	Match->WinnerSide = INDEX_NONE;
	Match->LastPointReason = EBadmintonPointReason::None;
	Match->Phase = EBadmintonPhase::WaitingForPlayers;
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		if (ABadmintonPlayerState* State = Cast<ABadmintonPlayerState>(Entry))
		{
			State->bReady = false;
			State->GetAbilitySystemComponent()->CancelAllAbilities();
		}
	}
	RefreshLobby();
	Shuttle->ResetForServe(FVector(-390, 0, 160), GetGameState<ABadmintonGameState>()->RallyId);
}

void ABadmintonGameMode::RefreshLobby()
{
	ABadmintonGameState* State = GetGameState<ABadmintonGameState>();
	const EBadmintonPhase PreviousPhase = State->Phase;
	State->ConnectedPlayers = 0;
	bool bAllReady = true;
	for (APlayerState* Entry : State->PlayerArray)
	{
		if (const ABadmintonPlayerState* Player = Cast<ABadmintonPlayerState>(Entry); Player && Player->CourtSide != INDEX_NONE)
		{
			++State->ConnectedPlayers;
			bAllReady &= Player->bReady;
		}
	}
	if (State->ConnectedPlayers < Badminton::MaxPlayers)
	{
		State->Phase = EBadmintonPhase::WaitingForPlayers;
	}
	else if (PreviousPhase == EBadmintonPhase::MatchFinished)
	{
		if (bAllReady) { StartMatch(); }
	}
	else if (PreviousPhase == EBadmintonPhase::WaitingForPlayers || PreviousPhase == EBadmintonPhase::WaitingForReady)
	{
		State->Phase = EBadmintonPhase::WaitingForReady;
		if (bAllReady) { StartMatch(); }
	}
	State->ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("BADMINTON Lobby players=%d phase=%d"), State->ConnectedPlayers, static_cast<int32>(State->Phase));
}

void ABadmintonGameMode::StartMatch()
{
	ABadmintonGameState* State = GetGameState<ABadmintonGameState>();
	GetWorldTimerManager().ClearTimer(ServeResetTimer);
	++State->MatchId;
	State->Score0 = State->Score1 = 0;
	State->WinnerSide = INDEX_NONE;
	State->ServingSide = 0;
	State->LastPointReason = EBadmintonPointReason::None;
	for (APlayerState* Entry : State->PlayerArray)
	{
		if (ABadmintonPlayerState* Player = Cast<ABadmintonPlayerState>(Entry))
		{
			Player->GetAbilitySystemComponent()->SetNumericAttributeBase(UBadmintonAttributeSet::GetStaminaAttribute(), Player->GetAttributes()->GetMaxStamina());
		}
	}
	PreparePracticeServe();
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_MATCH_STARTED id=%d"), State->MatchId);
}

void ABadmintonGameMode::ResetPlayerPositions()
{
	const ABadmintonGameState* State = GetGameState<ABadmintonGameState>();
	const float ServiceY = Badminton::ServeY(State->ServingSide, State->GetScore(State->ServingSide));
	for (APlayerState* Entry : State->PlayerArray)
	{
		ABadmintonPlayerState* Player = Cast<ABadmintonPlayerState>(Entry);
		if (!Player) { continue; }
		Player->GetAbilitySystemComponent()->CancelAllAbilities();
		if (ABadmintonCharacter* Pawn = Cast<ABadmintonCharacter>(Player->GetPawn()))
		{
			FVector Position = Badminton::SpawnTransform(Player->CourtSide).GetLocation();
			Position.Y = Player->CourtSide == State->ServingSide ? ServiceY : -ServiceY;
			Pawn->GetCharacterMovement()->StopMovementImmediately();
			Pawn->TeleportTo(Position, Pawn->GetActorRotation(), false, true);
			Pawn->ForceNetUpdate();
		}
	}
}

void ABadmintonGameMode::PreparePracticeServe()
{
	ABadmintonGameState* State = GetGameState<ABadmintonGameState>();
	if (State->ConnectedPlayers != Badminton::MaxPlayers || State->WinnerSide != INDEX_NONE)
	{
		return;
	}
	++State->RallyId;
	State->Phase = EBadmintonPhase::ReadyToServe;
	ResetPlayerPositions();
	const FVector ServePosition(-Badminton::ForwardSign(State->ServingSide) * 390.f,
		Badminton::ServeY(State->ServingSide, State->GetScore(State->ServingSide)), 160);
	Shuttle->ResetForServe(ServePosition, State->RallyId);
	State->ForceNetUpdate();
}

bool ABadmintonGameMode::TryBasicShot(ABadmintonCharacter* Player, int32 RequestedRallyId)
{
	return TryShot(Player, RequestedRallyId, EBadmintonShot::Clear);
}

bool ABadmintonGameMode::TryShot(ABadmintonCharacter* Player, int32 RequestedRallyId, EBadmintonShot Shot, const FVector2D& Aim)
{
	ABadmintonGameState* State = GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* PlayerState = Player ? Player->GetPlayerState<ABadmintonPlayerState>() : nullptr;
	const auto Result = [&](const TCHAR* Reason, bool bAccepted = false)
	{
		if (Badminton::NetMetricsEnabled())
		{
			UE_LOG(LogTemp, Display, TEXT("BADMINTON_METRIC_CONTACT side=%d rally=%d shot=%d accepted=%d reason=%s distance_cm=%.3f height_cm=%.3f"),
				PlayerState ? PlayerState->CourtSide : INDEX_NONE, RequestedRallyId, static_cast<int32>(Shot), bAccepted, Reason,
				Player && Shuttle ? FVector::Dist2D(Player->GetActorLocation(), Shuttle->GetActorLocation()) : -1.,
				Shuttle ? Shuttle->GetActorLocation().Z : -1.);
		}
		return bAccepted;
	};
	if (!HasAuthority() || !PlayerState || PlayerState->CourtSide < 0 || PlayerState->CourtSide >= Badminton::MaxPlayers
		|| !PlayerState->bReady || State->ConnectedPlayers != 2 || !Shuttle || State->RallyId != RequestedRallyId)
	{
		return Result(TEXT("InvalidContextOrRally"));
	}
	const int32 Side = PlayerState->CourtSide;
	const FVector PlayerPosition = Player->GetActorLocation();
	FVector HitPosition = Shuttle->GetActorLocation();
	const bool bServe = State->Phase == EBadmintonPhase::ReadyToServe;
	if (bServe)
	{
		if (Shot != EBadmintonShot::Clear && Shot != EBadmintonShot::Serve) { return Result(TEXT("ServeShotType")); }
		Shot = EBadmintonShot::Serve;
	}
	else if (Shot == EBadmintonShot::Serve) { return Result(TEXT("ServeOutsideServePhase")); }
	FVector Target;
	if (!State->GetShotData()->ResolveAimTarget(Shot, Side, State->GetScore(Side), Aim, Target)) { return Result(TEXT("InvalidAimTarget")); }
	const FBadmintonShotParameters& Params = State->GetShotData()->Get(Shot);
	if (State->Phase == EBadmintonPhase::ReadyToServe)
	{
		const float Forward = Badminton::ForwardSign(Side);
		if (Side != State->ServingSide || PlayerPosition.X * Forward > -Badminton::ShortServiceLine
			|| PlayerPosition.Y * Badminton::ServeY(Side, State->GetScore(Side)) < 0.f || !Badminton::IsInCourt(PlayerPosition))
		{
			return Result(TEXT("ServicePositionOrOwner"));
		}
		HitPosition = FVector(PlayerPosition.X + Badminton::ForwardSign(Side) * 50.f, PlayerPosition.Y, 160.f);
	}
	else if (State->Phase == EBadmintonPhase::Rally)
	{
		if (!Shuttle->GetFlight().bFlying || Shuttle->GetFlight().RallyId != State->RallyId) { return Result(TEXT("InactiveFlight")); }
		if (Shuttle->GetFlight().LastHitterSide == Side) { return Result(TEXT("SameHitter")); }
		if (!Shuttle->GetFlight().bLegalCrossing) { return Result(TEXT("NoLegalCrossing")); }
		const auto Geometry = Badminton::ContactGeometry(Side, PlayerPosition, HitPosition, Params);
		if (Geometry != Badminton::EContactHint::Ready) { return Result(Badminton::ContactHintText(Geometry)); }
	}
	else
	{
		return Result(TEXT("InactivePhase"));
	}
	float FlightTime = Params.FlightTime;
	float Quality = 0.f;
	bool bAligned = false;
	bool bPlayerContact = false;
	const ABadmintonPlayerController* Controller = Cast<ABadmintonPlayerController>(Player->GetController());
	const bool bThirdPerson = Controller && Controller->UsesThirdPersonControl();
	if (bThirdPerson)
	{
		if (!bServe && Badminton::IsDropFamily(Shot) && Shot != Badminton::ResolveDropShot(Side, PlayerPosition))
		{ return Result(TEXT("AutomaticDropPositionMismatch")); }
		float Seconds = 0.f;
		Quality = 1.f;
		if (!bServe)
		{
			const auto Timing = Badminton::ThirdPersonContact(Side, Shot, Params, PlayerPosition, HitPosition, Shuttle->GetSimulationVelocity(), Quality, Seconds);
			if (Timing != Badminton::EContactHint::Ready) { return Result(TEXT("ThirdPersonTimingMiss")); }
		}
		Target = Badminton::ThirdPersonTarget(Shot, Side, State->GetScore(Side), Aim);
		Badminton::PrepareThirdPersonFlight(Shot, Side, HitPosition, Target, Params.FlightTime, Quality,
			State->RallyId * 7919 + Shuttle->GetFlight().ShotSequence, FlightTime);
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_THIRD_PERSON_HIT side=%d shot=%d quality=%.3f timing=%.3f duration=%.3f"), Side, static_cast<int32>(Shot), Quality, Seconds, FlightTime);
		if (Badminton::IsDropFamily(Shot))
		{ UE_LOG(LogTemp, Display, TEXT("BADMINTON_AUTO_DROP side=%d depth=%.1f shot=%d cost=%.1f"), Side, -PlayerPosition.X * Badminton::ForwardSign(Side), static_cast<int32>(Shot), Params.StaminaCost); }
	}
	else if (Controller && Controller->ResolveTimedContact(Target, Quality, bAligned))
	{
		bPlayerContact = true;
		if (!bAligned) { return Result(TEXT("RacketAimMiss")); }
		Badminton::PreparePlayerFlight(Shot, Side, HitPosition, Target, Params.FlightTime, Quality,
			State->RallyId * 7919 + Shuttle->GetFlight().ShotSequence, FlightTime);
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_TIMING_FLIGHT quality=%.3f duration=%.3f target=%s"), Quality, FlightTime, *Target.ToString());
	}
	const FVector BaseVelocity = ABadmintonShuttle::SolveVelocity(HitPosition, Target, FlightTime);
	FVector LaunchVelocity = BaseVelocity;
	if (bPlayerContact && Shot != EBadmintonShot::Smash)
	{
		const float FaceTilt = Controller->GetDispatchedRacketTilt();
		LaunchVelocity = Badminton::ResolveRacketImpact(Shuttle->GetSimulationVelocity(), BaseVelocity, Controller->GetDispatchedFaceNormal(), FaceTilt, Shot);
		if (!FMath::IsNearlyZero(FaceTilt))
		{
			UE_LOG(LogTemp,Display,TEXT("BADMINTON_RACKET_IMPACT shot=%d angle=%.1f deltaVelocity=%.3f incoming=%s outgoing=%s"),
				static_cast<int32>(Shot), FaceTilt, FVector::Dist(LaunchVelocity,BaseVelocity), *Shuttle->GetSimulationVelocity().ToString(), *LaunchVelocity.ToString());
		}
	}
	// Do not re-aim after the physical impact: its changed path may hit the net or go out.
	Shuttle->Launch(HitPosition, Target, Side, State->RallyId, FlightTime, Shot, Aim, &LaunchVelocity);
	State->Phase = EBadmintonPhase::Rally;
	State->ForceNetUpdate();
	for (TActorIterator<ABadmintonPlayerController> It(GetWorld()); It; ++It)
	{
		It->ClientPracticeContact(State->RallyId, Shuttle->GetFlight().ShotSequence, Side, Shot);
	}
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_SHOT_ACCEPTED side=%d rally=%d sequence=%d type=%d"), Side, State->RallyId, Shuttle->GetFlight().ShotSequence, static_cast<int32>(Shot));
	return Result(TEXT("Contact"), true);
}

void ABadmintonGameMode::FinishPracticeRally(const FVector& Position, int32 RallyId, int32 LastHitterSide)
{
	ABadmintonGameState* State = GetGameState<ABadmintonGameState>();
	if (State->Phase != EBadmintonPhase::Rally || State->RallyId != RallyId)
	{
		return;
	}
	const FBadmintonPointDecision Decision = Badminton::JudgeLanding(Position, LastHitterSide,
		Shuttle->GetFlight().Shot == EBadmintonShot::Serve, State->GetScore(LastHitterSide), Shuttle->GetFlight().bLegalCrossing);
	if (Decision.Winner == INDEX_NONE) { return; }
	for (TActorIterator<ABadmintonPlayerController> It(GetWorld()); It; ++It)
	{
		It->ClientPracticeLanding(RallyId, Decision.Winner, LastHitterSide, Shuttle->GetFlight().Shot, Position);
	}
	State->Phase = EBadmintonPhase::RallyComplete;
	++State->CompletedRallies;
	State->LastPointReason = Decision.Reason;
	State->ServingSide = Decision.Winner;
	if (Decision.Winner == 0) { ++State->Score0; } else { ++State->Score1; }
	for (APlayerState* Entry : State->PlayerArray)
	{
		if (ABadmintonPlayerState* Player = Cast<ABadmintonPlayerState>(Entry)) { Player->GetAbilitySystemComponent()->CancelAllAbilities(); }
	}
	if (State->GetScore(Decision.Winner) >= Badminton::PointsToWin)
	{
		State->WinnerSide = Decision.Winner;
		State->Phase = EBadmintonPhase::MatchFinished;
		for (APlayerState* Entry : State->PlayerArray)
		{
			if (ABadmintonPlayerState* Player = Cast<ABadmintonPlayerState>(Entry)) { Player->bReady = false; Player->ForceNetUpdate(); }
		}
	}
	State->ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_POINT rally=%d score=%d:%d server=%d winner=%d reason=%d"), RallyId, State->Score0, State->Score1, State->ServingSide, State->WinnerSide, static_cast<int32>(Decision.Reason));
	if (State->Phase != EBadmintonPhase::MatchFinished) { GetWorldTimerManager().SetTimer(ServeResetTimer, this, &ThisClass::PreparePracticeServe, 1.5f, false); }
}
