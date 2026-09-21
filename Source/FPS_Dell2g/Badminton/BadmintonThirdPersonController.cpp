#include "BadmintonPlayerController.h"

#include "AbilitySystemComponent.h"
#include "BadmintonAttributeSet.h"
#include "BadmintonCamera.h"
#include "BadmintonRules.h"
#include "BadmintonClearAbility.h"
#include "BadmintonGameState.h"
#include "BadmintonPlayerState.h"
#include "BadmintonThirdPerson.h"
#include "EngineUtils.h"
#include "UnrealClient.h"

EBadmintonShot ABadmintonPlayerController::GetAutomaticDropShot() const
{
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	return State && GetPawn() ? Badminton::ResolveDropShot(State->CourtSide, GetPawn()->GetActorLocation()) : EBadmintonShot::Drop;
}

EBadmintonShot ABadmintonPlayerController::GetSelectedShot() const
{
	if (bThirdPersonControl && Badminton::IsDropFamily(SelectedStroke)) { return GetAutomaticDropShot(); }
	return bTimingArmed ? TimingShot : SelectedStroke;
}

void ABadmintonPlayerController::SampleThirdPersonFlight(FVector& Position, FVector& Velocity) const
{
	Position = FVector::ZeroVector;
	Velocity = FVector::ZeroVector;
	if (!TimingShuttle) { return; }
	Position = TimingShuttle->GetActorLocation();
	Velocity = TimingShuttle->GetSimulationVelocity();
	if (!HasAuthority())
	{
		const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
		const auto& Flight = TimingShuttle->GetFlight();
		Position = Flight.Position;
		double Now = Match ? Match->GetServerWorldTimeSeconds() : Flight.ServerTime, RTT = 0.;
		GetShuttleServerTime(Now, RTT);
		ABadmintonShuttle::AdvanceFlight(Position, Velocity, FMath::Clamp(static_cast<float>(Now - Flight.ServerTime), 0.f, .25f));
	}
}

Badminton::EContactHint ABadmintonPlayerController::GetThirdPersonContact(float& Quality, float& Seconds, FVector& ContactPosition) const
{
	return GetThirdPersonContactForShot(GetSelectedShot(), Quality, Seconds, ContactPosition);
}

Badminton::EContactHint ABadmintonPlayerController::GetThirdPersonContactForShot(EBadmintonShot RequestedShot, float& Quality, float& Seconds, FVector& ContactPosition) const
{
	using namespace Badminton;
	Quality = 0.f;
	Seconds = 0.f;
	ContactPosition = FVector::ZeroVector;
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || !GetPawn() || !TimingShuttle || !State->bReady || Match->ConnectedPlayers != 2) { return EContactHint::WaitReturn; }
	const auto* ASC = State->GetAbilitySystemComponent();
	if (ASC->HasMatchingGameplayTag(TAG_BadmintonSwinging) || ASC->HasMatchingGameplayTag(TAG_BadmintonDashing)) { return EContactHint::Recovering; }
	const bool bServe = Match->Phase == EBadmintonPhase::ReadyToServe && Match->ServingSide == State->CourtSide;
	const EBadmintonShot Shot = bServe ? EBadmintonShot::Serve : IsDropFamily(RequestedShot) ? GetAutomaticDropShot() : RequestedShot;
	const auto& Params = Match->GetShotData()->Get(Shot);
	if (State->GetAttributes()->GetStamina() < Params.StaminaCost) { return EContactHint::LowStamina; }
	if (bServe)
	{
		const FVector PlayerPosition = GetPawn()->GetActorLocation();
		Quality = 1.f;
		return PlayerPosition.X * ForwardSign(State->CourtSide) <= -ShortServiceLine && PlayerPosition.Y * ServeY(State->CourtSide, Match->GetScore(State->CourtSide)) >= 0.
			&& IsInCourt(PlayerPosition) ? EContactHint::Ready : EContactHint::ServicePosition;
	}
	const auto& Flight = TimingShuttle->GetFlight();
	if (Match->Phase != EBadmintonPhase::Rally || !Flight.bFlying || !Flight.bLegalCrossing || Flight.RallyId != Match->RallyId
		|| Flight.LastHitterSide == State->CourtSide) { return EContactHint::WaitReturn; }
	FVector Position, Velocity;
	SampleThirdPersonFlight(Position, Velocity);
	const float Height = ContactHeight(Shot, Params);
	if (DescendingContactTime(Position, Velocity, Height, Seconds))
	{
		ContactPosition = Position;
		FVector FutureVelocity = Velocity;
		ABadmintonShuttle::AdvanceFlight(ContactPosition, FutureVelocity, Seconds);
	}
	float ContactSeconds;
	return ThirdPersonContact(State->CourtSide, Shot, Params, GetPawn()->GetActorLocation(), Position, Velocity, Quality, ContactSeconds);
}

void ABadmintonPlayerController::ServerThirdPersonShot_Implementation(EBadmintonShot Shot, FVector2D Aim, int32 RallyId, int32 Sequence)
{
	if (!bThirdPersonControl || Aim.ContainsNaN() || FMath::Abs(Aim.X) > 1. || FMath::Abs(Aim.Y) > 1.) { return; }
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!TimingShuttle) { for (TActorIterator<ABadmintonShuttle> It(GetWorld()); It; ++It) { TimingShuttle = *It; break; } }
	if (!Match || !State || !TimingShuttle || !State->bReady || !GetPawn()) { return; }
	const auto& Flight = TimingShuttle->GetFlight();
	if (Match->RallyId != RallyId || Flight.ShotSequence != Sequence)
	{ ClientTimingResult(TEXT("랠리가 바뀌었습니다 / 상대 타격을 기다리세요")); return; }
	const bool bServe = Match->Phase == EBadmintonPhase::ReadyToServe && Match->ServingSide == State->CourtSide;
	if (!bServe && (Match->Phase != EBadmintonPhase::Rally || !Flight.bFlying || Flight.LastHitterSide == State->CourtSide)) { return; }
	if (Shot != EBadmintonShot::Clear && Shot != EBadmintonShot::Drop && Shot != EBadmintonShot::Smash
		&& Shot != EBadmintonShot::Receive && Shot != EBadmintonShot::Hairpin) { return; }
	if (Badminton::IsDropFamily(Shot)) { Shot = EBadmintonShot::Drop; }
	if (!bServe && Shot != SelectedStroke) { return; }
	// Resolve before GAS activation so stamina, recovery, contact height and trajectory all use the same shot.
	if (!bServe && Shot == EBadmintonShot::Drop) { Shot = GetAutomaticDropShot(); }
	float Quality, Seconds;
	FVector ContactPosition;
	const auto Hint = GetThirdPersonContact(Quality, Seconds, ContactPosition);
	ShotAim = Aim;
	// GAS owns recovery and costs, including an accepted swing that misses its contact window.
	SendShot(bServe ? EBadmintonShot::Serve : Shot);
	const bool bContact = TimingShuttle->GetFlight().ShotSequence != Sequence;
	ClientTimingResult(bContact ? FString::Printf(TEXT("%s / %s"), bServe ? TEXT("서브") : Quality >= .99f ? TEXT("완벽") : Quality >= .6f ? TEXT("좋음") : TEXT("약함"), Badminton::ShotLabel(Shot))
		: Hint == Badminton::EContactHint::TooHigh ? TEXT("너무 이릅니다 / 내려올 때까지 기다리세요") : Hint == Badminton::EContactHint::Ready ? TEXT("동작 회복 중 / 기다리세요") : Badminton::ContactHintText(Hint));
	if (bContact) { SelectedStroke = EBadmintonShot::Receive; AimPreviewShot = EBadmintonShot::Receive; ClientResetStroke(); }
}

void ABadmintonPlayerController::RunThirdPersonProbe(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	// Keep returning shots after this peer passes so the other peer can finish its own checks.
	if ((bThirdPersonProbeDone && GetNetMode() == NM_Standalone) || !TimingShuttle || !GetPawn()) { return; }
	ThirdPersonProbeTime += DeltaTime;
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State) { return; }
	if (!bThirdPersonProbeDone && ThirdPersonProbeTime > 110.f)
	{ UE_LOG(LogTemp, Error, TEXT("BADMINTON_THIRD_PERSON_TEST FAIL timeout shots=%d mask=%d"), ThirdPersonProbeShots, ThirdPersonProbeMask); bThirdPersonProbeDone = true; return; }
	if (Match->Phase == EBadmintonPhase::WaitingForReady || Match->Phase == EBadmintonPhase::MatchFinished) { if (!State->bReady) { BadmintonReady(); } return; }
	const EBadmintonShot Shots[] = {EBadmintonShot::Clear, EBadmintonShot::Drop, EBadmintonShot::Receive, EBadmintonShot::Smash, EBadmintonShot::Hairpin};
	EBadmintonShot DesiredShot = EBadmintonShot::Receive;
	for (const auto Candidate : Shots)
	{
		if (!(ThirdPersonProbeMask & (1 << static_cast<int32>(Candidate)))) { DesiredShot = Candidate; break; }
	}
	const EBadmintonShot InputShot = Badminton::IsDropFamily(DesiredShot) ? EBadmintonShot::Drop : DesiredShot;
	if (SelectedStroke != InputShot) { SelectStroke(InputShot); }
	const auto& Flight = TimingShuttle->GetFlight();
	if (Flight.ShotSequence > ThirdPersonProbeSequence)
	{
		ThirdPersonProbeSequence = Flight.ShotSequence;
		if (Flight.LastHitterSide == State->CourtSide && Flight.Shot != EBadmintonShot::Serve)
		{ ThirdPersonProbeMask |= 1 << static_cast<int32>(Flight.Shot); ++ThirdPersonProbeShots; }
	}
	if (Match->Phase == EBadmintonPhase::Rally && Flight.LastHitterSide != State->CourtSide)
	{
		FVector Position, Velocity;
		SampleThirdPersonFlight(Position, Velocity);
		float Seconds;
		if (Badminton::DescendingContactTime(Position, Velocity, Badminton::ContactHeight(DesiredShot, Match->GetShotData()->Get(DesiredShot)), Seconds) && Seconds > 0.f)
		{
			ABadmintonShuttle::AdvanceFlight(Position, Velocity, Seconds);
			const float Sign = Badminton::ForwardSign(State->CourtSide);
			if (DesiredShot == EBadmintonShot::Hairpin
				&& -Sign * Position.X > Badminton::ShortServiceLine - 25.f + Match->GetShotData()->Hairpin.Reach - 25.f)
			{
				// Return deep feeds normally; never force a hairpin outside its court zone or reach.
				DesiredShot = EBadmintonShot::Receive;
				SelectStroke(DesiredShot);
			}
			if (DesiredShot == EBadmintonShot::Drop)
			{ Position.X = -Sign * FMath::Max(-Sign * Position.X, Badminton::ShortServiceLine + 35.f); }
			else if (DesiredShot == EBadmintonShot::Hairpin)
			{ Position.X = -Sign * FMath::Clamp(-Sign * Position.X, 65.f, Badminton::ShortServiceLine - 25.f); }
			const FVector Offset = Position - GetPawn()->GetActorLocation();
			if (Offset.Size2D() > 25.f) { GetPawn()->AddMovementInput(Offset.GetSafeNormal2D()); }
		}
	}
	if (Match->Phase == EBadmintonPhase::Rally && Flight.LastHitterSide == State->CourtSide && DesiredShot == EBadmintonShot::Hairpin)
	{
		// Recover to the back court after a defensive return, giving the opponent a short-shot opportunity.
		const FVector Back(-Badminton::ForwardSign(State->CourtSide) * 550.f, GetPawn()->GetActorLocation().Y, 96.f);
		const FVector Offset = Back - GetPawn()->GetActorLocation();
		if (Offset.Size2D() > 20.f) { GetPawn()->AddMovementInput(Offset.GetSafeNormal2D()); }
	}
	float Quality, Seconds;
	FVector Contact;
	const auto Hint = GetThirdPersonContact(Quality, Seconds, Contact);
	if (Hint == Badminton::EContactHint::Ready && (Match->Phase == EBadmintonPhase::ReadyToServe || (Quality > .85f && GetSelectedShot() == DesiredShot)))
	{
		if (!bThirdPersonReadyCaptured && Match->Phase == EBadmintonPhase::Rally)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/BadmintonThirdPersonReady.png"), true, false);
			bThirdPersonReadyCaptured = true;
			return;
		}
		const int32 Before = Flight.ShotSequence;
		const int32 Rally = Match->RallyId;
		// Deliberately look away: third-person contact must be independent of racket aim.
		CameraLook = FVector2D(90., -45.);
		// Exercise the actual keyboard handlers: only serve and smash use a separate click.
		if (Match->Phase == EBadmintonPhase::ReadyToServe) { BadmintonStrike(); }
		else
		{
			switch (DesiredShot)
			{
			case EBadmintonShot::Clear: BadmintonSelectClear(); break;
			case EBadmintonShot::Drop: BadmintonSelectDrop(); break;
			case EBadmintonShot::Receive: BadmintonSelectReceive(); break;
			case EBadmintonShot::Hairpin: BadmintonSelectDrop(); break;
			case EBadmintonShot::Smash:
				BadmintonSelectSmash();
				if (Flight.ShotSequence != Before)
				{ UE_LOG(LogTemp, Error, TEXT("BADMINTON_THIRD_PERSON_TEST FAIL smash fired on selection")); bThirdPersonProbeDone = true; return; }
				BadmintonStrike();
				break;
			default: break;
			}
		}
		if (HasAuthority() && TimingShuttle->GetFlight().ShotSequence != Before)
		{
			ServerThirdPersonShot(DesiredShot, ShotAim, Rally, Before);
			if (TimingShuttle->GetFlight().ShotSequence != Before + 1)
			{ UE_LOG(LogTemp, Error, TEXT("BADMINTON_THIRD_PERSON_TEST FAIL duplicate")); bThirdPersonProbeDone = true; return; }
		}
	}
	if (!bThirdPersonProbeDone && ThirdPersonProbeShots >= 5 && ThirdPersonProbeMask == 62)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/BadmintonThirdPerson.png"), true, false);
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_THIRD_PERSON_TEST PASS contacts=%d mask=%d aimIndependent=1 duplicateGuard=%d keyboardShots=1"), ThirdPersonProbeShots, ThirdPersonProbeMask, HasAuthority() ? 1 : 0);
		bThirdPersonProbeDone = true;
		CameraLook = FVector2D(0., Badminton::CameraDefaultPitch);
	}
#endif
}
