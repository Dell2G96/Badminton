#include "BadmintonAIController.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "BadmintonAttributeSet.h"
#include "BadmintonPlayerController.h"
#include "BadmintonPlayFeel.h"
#include "BadmintonClearAbility.h"
#include "BadmintonGameMode.h"
#include "BadmintonGameState.h"
#include "BadmintonPlayerState.h"
#include "BadmintonShotData.h"
#include "BadmintonShuttle.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

ABadmintonAIController::ABadmintonAIController()
{
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
}

FVector ABadmintonAIController::FindReceivePosition(const ABadmintonShuttle* InShuttle, int32 Side, double WorldTime)
{
	const FBadmintonFlightState& Flight = InShuttle->GetFlight();
	FVector Position = Flight.Position;
	FVector Velocity = Flight.Velocity;
	ABadmintonShuttle::AdvanceFlight(Position, Velocity, FMath::Clamp(static_cast<float>(WorldTime - Flight.ServerTime), 0.f, .1f));
	// Predict a descending return height using the game's flight model, then walk there.
	for (int32 Step = 0; Step < 120; ++Step)
	{
		if (Position.X * Badminton::ForwardSign(Side) < 0.f && Velocity.Z < 0.f && Position.Z <= 250.f) { break; }
		if (Position.Z < 10.f) { break; }
		ABadmintonShuttle::AdvanceFlight(Position, Velocity, .025f);
	}
	const float Forward = Badminton::ForwardSign(Side);
	Position.X = Forward * FMath::Clamp(Position.X * Forward, -Badminton::HalfLength + 40.f, -70.f);
	Position.Y = FMath::Clamp(Position.Y, -Badminton::HalfWidth + 40.f, Badminton::HalfWidth - 40.f);
	Position.Z = Badminton::PlayerHeight;
	return Position;
}

void ABadmintonAIController::PlayShot(EBadmintonShot Shot, const FVector2D& Aim)
{
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (!State || !Match || !GetPawn()) { return; }
	FGameplayEventData Payload;
	Payload.EventTag = Shot == EBadmintonShot::Serve ? TAG_BadmintonServeEvent : Shot == EBadmintonShot::Drop ? TAG_BadmintonDropEvent
		: Shot == EBadmintonShot::Smash ? TAG_BadmintonSmashEvent : TAG_BadmintonClearEvent;
	Payload.EventMagnitude = Match->RallyId;
	Payload.Instigator = GetPawn();
	auto* Target = new FGameplayAbilityTargetData_LocationInfo();
	Target->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Target->TargetLocation.LiteralTransform = FTransform(FVector(Aim.X, Aim.Y, 0));
	Payload.TargetData.Add(Target);
	State->GetAbilitySystemComponent()->HandleGameplayEvent(Payload.EventTag, &Payload);
}

void ABadmintonAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	ACharacter* ControlledCharacter = GetCharacter();
	if (!HasAuthority() || !State || !Match || !ControlledCharacter) { return; }
	if (!Shuttle)
	{
		for (TActorIterator<ABadmintonShuttle> It(GetWorld()); It; ++It) { Shuttle = *It; break; }
	}
	if (!Shuttle) { return; }
	if (Match->Phase == EBadmintonPhase::WaitingForReady || Match->Phase == EBadmintonPhase::MatchFinished)
	{
		if (!State->bReady)
		{
			State->bReady = true;
			State->ForceNetUpdate();
			GetWorld()->GetAuthGameMode<ABadmintonGameMode>()->RefreshLobby();
		}
		return;
	}
	FVector OpponentPosition = Badminton::SpawnTransform(1 - State->CourtSide).GetLocation();
	for (TActorIterator<ABadmintonPlayerController> It(GetWorld()); It; ++It)
	{
		if (It->GetPawn()) { OpponentPosition = It->GetPawn()->GetActorLocation(); Difficulty = It->GetAIDifficulty(); break; }
	}
	const float Speeds[] = {320.f, 400.f, 450.f};
	ControlledCharacter->GetCharacterMovement()->MaxWalkSpeed = Speeds[FMath::Clamp(Difficulty, 0, 2)];
	const double Now = GetWorld()->GetTimeSeconds();
	const FBadmintonFlightState& Flight = Shuttle->GetFlight();
	if (ObservedRally != Match->RallyId)
	{
		ObservedRally = Match->RallyId;
		ObservedSequence = INDEX_NONE;
		NextShotAt = Now + ServeDelay;
		MoveTarget = ControlledCharacter->GetActorLocation();
	}
	if (Match->Phase == EBadmintonPhase::ReadyToServe)
	{
		if (Match->ServingSide == State->CourtSide && Now >= NextShotAt)
		{
			NextShotAt = Now + .5;
			PlayShot(EBadmintonShot::Serve, FVector2D::ZeroVector);
		}
		return;
	}
	if (Match->Phase != EBadmintonPhase::Rally || !Flight.bFlying) { return; }
	if (ObservedSequence != Flight.ShotSequence)
	{
		ObservedSequence = Flight.ShotSequence;
		const float Delays[] = {.34f, .22f, .13f};
		const float Errors[] = {.30f, .17f, .07f};
		ReactAt = Now + Delays[Difficulty] + Decisions.FRandRange(0.f, .08f);
		ShotError = FVector2D(Decisions.FRandRange(-Errors[Difficulty], Errors[Difficulty]), Decisions.FRandRange(-Errors[Difficulty], Errors[Difficulty]));
		// Stop the previous pursuit until the reaction interval has elapsed.
		MoveTarget = ControlledCharacter->GetActorLocation();
	}
	if (Now < ReactAt) { return; }
	const bool bReceiving = Flight.LastHitterSide != State->CourtSide;
	MoveTarget = bReceiving ? FindReceivePosition(Shuttle, State->CourtSide, Now)
		: Badminton::SpawnTransform(State->CourtSide).GetLocation();
	const FVector Offset = MoveTarget - ControlledCharacter->GetActorLocation();
	const float Distance = Offset.Size2D();
	if (Distance > 15.f) { ControlledCharacter->AddMovementInput(Offset.GetSafeNormal2D(), FMath::Clamp(Distance / 80.f, 0.f, 1.f)); }
	const FVector Contact = Shuttle->GetActorLocation();
	const UBadmintonShotData* Data = Match->GetShotData();
	if (!bReceiving || !Flight.bLegalCrossing || Contact.X * Badminton::ForwardSign(State->CourtSide) > 0.f
		|| Now < NextShotAt || Contact.Z < Data->Clear.MinimumHeight || Contact.Z > 280.f
		|| FVector::Dist2D(ControlledCharacter->GetActorLocation(), Contact) > Data->Clear.Reach * .85f) { return; }
	const float Stamina = State->GetAttributes()->GetStamina();
	EBadmintonShot Shot = Badminton::ChooseTacticalShot(FMath::Abs(OpponentPosition.X), Contact.Z, Stamina, *Data);
	if (Difficulty == 0 && Shot == EBadmintonShot::Smash && ReturnCount % 3 != 2) { Shot = EBadmintonShot::Clear; }
	const FBadmintonShotParameters& Params = Data->Get(Shot);
	if (Contact.Z > Params.MaximumHeight || FVector::Dist2D(ControlledCharacter->GetActorLocation(), Contact) > Params.Reach) { return; }
	// Attack the open space, with difficulty-dependent placement error.
	const float Sign = Badminton::ForwardSign(State->CourtSide);
	const float Away = OpponentPosition.Y * Sign >= 0.f ? -.7f : .7f;
	const float Depth = Shot == EBadmintonShot::Drop ? -.8f : Shot == EBadmintonShot::Clear ? 1.f : .2f;
	const FVector2D Aim(FMath::Clamp(Away + ShotError.X, -1., 1.), FMath::Clamp(Depth + ShotError.Y, -1., 1.));
	NextShotAt = Now + Params.Recovery;
	PlayShot(Shot, Aim);
	++ReturnCount;
}
