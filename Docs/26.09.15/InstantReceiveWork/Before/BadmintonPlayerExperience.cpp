#include "BadmintonPlayerController.h"
#include "BadmintonAttributeSet.h"
#include "BadmintonClearAbility.h"
#include "BadmintonDashAbility.h"
#include "BadmintonGameState.h"
#include "BadmintonPlayerState.h"
#include "BadmintonShuttle.h"
#include "BadmintonRules.h"
#include "AbilitySystemComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

Badminton::EContactHint ABadmintonPlayerController::EvaluateContact(const FVector& Direction) const
{
	using namespace Badminton;
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!bTimingArmed) { return EContactHint::SelectShot; }
	if (!Match || !State || !GetPawn() || !TimingShuttle || !State->bReady || Match->ConnectedPlayers != 2
		|| Match->RallyId != TimingRally || TimingShuttle->GetFlight().ShotSequence != TimingSequence) { return EContactHint::WaitReturn; }
	const auto* ASC = State->GetAbilitySystemComponent();
	if (ASC->HasMatchingGameplayTag(TAG_BadmintonSwinging) || ASC->HasMatchingGameplayTag(TAG_BadmintonDashing)) { return EContactHint::Recovering; }
	const auto& Params = Match->GetShotData()->Get(TimingShot);
	if (State->GetAttributes()->GetStamina() < Params.StaminaCost) { return EContactHint::LowStamina; }
	if (TimingShot != EBadmintonShot::Receive && bContactForecast && Match->GetServerWorldTimeSeconds() > TimingPerfectAt + .30) { return EContactHint::TooLate; }
	if (Match->Phase == EBadmintonPhase::ReadyToServe && TimingShot == EBadmintonShot::Serve && Match->ServingSide == State->CourtSide)
	{
		const FVector Position = GetPawn()->GetActorLocation();
		return Position.X * ForwardSign(State->CourtSide) <= -ShortServiceLine && Position.Y * ServeY(State->CourtSide, Match->GetScore(State->CourtSide)) >= 0.
			&& IsInCourt(Position) ? EContactHint::Ready : EContactHint::ServicePosition;
	}
	const auto& Flight = TimingShuttle->GetFlight();
	if (Match->Phase != EBadmintonPhase::Rally || !Flight.bFlying || Flight.RallyId != Match->RallyId || !Flight.bLegalCrossing || Flight.LastHitterSide == State->CourtSide) { return EContactHint::WaitReturn; }
	const auto Geometry = ContactGeometry(State->CourtSide, GetPawn()->GetActorLocation(), TimingShuttle->GetActorLocation(), Params);
	if (Geometry != EContactHint::Ready) { return Geometry; }
	return RacketAimMatches(TimingShot, Direction, TimingShuttle->GetActorLocation() - GetEyePosition()) ? EContactHint::Ready : EContactHint::AimMiss;
}
Badminton::EContactHint ABadmintonPlayerController::GetContactHint() const { return EvaluateContact(GetCourtViewRotation().Vector()); }

void ABadmintonPlayerController::RefreshContactForecast()
{
	if (!bTimingArmed || TimingShot == EBadmintonShot::Serve || TimingShot == EBadmintonShot::Receive || !TimingShuttle || !GetPawn()) { return; }
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || Match->Phase != EBadmintonPhase::Rally || TimingRally != Match->RallyId || TimingSequence != TimingShuttle->GetFlight().ShotSequence) { return; }
	const double Now = Match->GetServerWorldTimeSeconds();
	if (Now < NextForecastAt) { return; }
	NextForecastAt = Now + .1;
	// Freeze the last 120 ms so the green zone cannot slide away from an imminent click.
	const auto& Flight = TimingShuttle->GetFlight();
	FVector Position = Flight.Position, Velocity = Flight.Velocity;
	ABadmintonShuttle::AdvanceFlight(Position, Velocity, FMath::Clamp(static_cast<float>(Now - Flight.ServerTime), 0.f, .1f));
	if (bContactForecast && Now >= TimingPerfectAt - .12)
	{
		FVector Contact = Position, ContactVelocity = Velocity;
		ABadmintonShuttle::AdvanceFlight(Contact, ContactVelocity, FMath::Max(0.f, static_cast<float>(TimingPerfectAt - Now)));
		if (Now > TimingPerfectAt + .1 || Badminton::ContactGeometry(State->CourtSide, GetPawn()->GetActorLocation(), Contact, Match->GetShotData()->Get(TimingShot)) == Badminton::EContactHint::Ready) { return; }
		bContactForecast = false;
		ClientContactForecast(false, TimingPerfectAt, TimingRally, TimingSequence);
	}
	float Seconds = 0.f;
	const bool bFound = Badminton::PredictContact(State->CourtSide, TimingShot, Match->GetShotData()->Get(TimingShot), Position, Velocity,
		GetPawn()->GetActorLocation(), GetPawn()->GetVelocity(), Seconds);
	const double PerfectAt = Now + Seconds;
	if (bFound != bContactForecast || (bFound && FMath::Abs(PerfectAt - TimingPerfectAt) > .025))
	{
		bContactForecast = bFound;
		TimingPerfectAt = PerfectAt;
		ClientContactForecast(bFound, PerfectAt, TimingRally, TimingSequence);
	}
}
void ABadmintonPlayerController::ClientContactForecast_Implementation(bool bFound, double PerfectAt, int32 RallyId, int32 Sequence)
{
	if (!bTimingArmed || RallyId != TimingRally || Sequence != TimingSequence) { return; }
	bContactForecast = bFound;
	TimingPerfectAt = PerfectAt;
}

void ABadmintonPlayerController::LoadPracticeSettings()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("BadmintonTimingTest")) || FParse::Param(FCommandLine::Get(), TEXT("BadmintonAITest"))) { return; }
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Config/BadmintonPractice.ini");
	float Value = MouseLookSensitivity;
	GConfig->GetFloat(TEXT("Practice"), TEXT("Sensitivity"), Value, Path);
	MouseLookSensitivity = FMath::IsFinite(Value) ? FMath::Clamp(Value, .1f, 1.5f) : .72f;
	GConfig->GetInt(TEXT("Practice"), TEXT("BestRally"), Practice.BestRally, Path);
	Practice.BestRally = FMath::Max(0, Practice.BestRally);
	GConfig->GetInt(TEXT("Practice"), TEXT("Difficulty"), AIDifficulty, Path);
	AIDifficulty = FMath::Clamp(AIDifficulty, 0, 2);
}
void ABadmintonPlayerController::SavePracticeSettings()
{
	if (bTimingProbe || bAIProbe) { return; }
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Config/BadmintonPractice.ini");
	GConfig->SetFloat(TEXT("Practice"), TEXT("Sensitivity"), MouseLookSensitivity, Path);
	GConfig->SetInt(TEXT("Practice"), TEXT("BestRally"), Practice.BestRally, Path);
	GConfig->SetInt(TEXT("Practice"), TEXT("Difficulty"), AIDifficulty, Path);
	GConfig->Flush(false, Path);
}
void ABadmintonPlayerController::BadmintonSensitivity(float Value)
{
	if (!IsLocalController() || !FMath::IsFinite(Value)) { return; }
	MouseLookSensitivity = FMath::Clamp(Value, .1f, 1.5f);
	SavePracticeSettings();
	ClientTimingResult(FString::Printf(TEXT("MOUSE SENSITIVITY %.2f"), MouseLookSensitivity));
}
void ABadmintonPlayerController::SensitivityUp() { BadmintonSensitivity(MouseLookSensitivity + .1f); }
void ABadmintonPlayerController::SensitivityDown() { BadmintonSensitivity(MouseLookSensitivity - .1f); }
void ABadmintonPlayerController::CycleDifficulty() { BadmintonDifficulty((AIDifficulty + 1) % 3); }
void ABadmintonPlayerController::BadmintonDifficulty(int32 Level)
{
	if (GetNetMode() != NM_Standalone) { return; }
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (Match && Match->Phase == EBadmintonPhase::Rally) { ClientTimingResult(TEXT("CHANGE AI LEVEL BETWEEN RALLIES")); return; }
	AIDifficulty = FMath::Clamp(Level, 0, 2);
	SavePracticeSettings();
	ClientTimingResult(FString::Printf(TEXT("AI LEVEL: %s"), AIDifficulty == 0 ? TEXT("EASY") : AIDifficulty == 1 ? TEXT("NORMAL") : TEXT("HARD")));
}

void ABadmintonPlayerController::ClientPracticeContact_Implementation(int32 RallyId, int32 Sequence, int32 Hitter, EBadmintonShot Shot)
{
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!State) { return; }
	const int32 Previous = Practice.Rally == RallyId ? Practice.RallyHits : 0;
	Practice.Contact(RallyId, Sequence, Hitter, State->CourtSide, Shot);
	if (Previous < 10 && Practice.RallyHits >= 10) { ChallengeMessage = TEXT("10 SHOT RALLY COMPLETE!"); ChallengeMessageUntil = GetWorld()->GetTimeSeconds() + 3.; }
}
void ABadmintonPlayerController::ClientPracticeLanding_Implementation(int32 RallyId, int32 Winner, int32 Hitter, EBadmintonShot Shot, FVector Position)
{
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!State) { return; }
	const int32 Before = Practice.ComboPoints;
	if (Practice.Landing(RallyId, Winner, Hitter, State->CourtSide, Shot, Position))
	{
		ChallengeMessage = Practice.ComboPoints > Before ? TEXT("DROP + SMASH POINT!") : TEXT("TARGET HIT! / AIM FOR THE OTHER SIDE");
		ChallengeMessageUntil = GetWorld()->GetTimeSeconds() + 3.;
	}
	SavePracticeSettings();
}

void ABadmintonPlayerController::PlayContactSound(EBadmintonShot Shot)
{
	if (!IsLocalController()) { return; }
	if (ContactAudio) { ContactAudio->Stop(); ContactAudio->DestroyComponent(); }
	auto* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(24000);
	Wave->NumChannels = 1;
	Wave->Duration = .16f;
	Wave->bLooping = false;
	TArray<int16> PCM;
	PCM.SetNumZeroed(3840);
	FRandomStream Noise(7919);
	const float Strength = Shot == EBadmintonShot::Smash ? 1.f : Shot == EBadmintonShot::Drop ? .55f : .75f;
	for (int32 Index = 0; Index < PCM.Num(); ++Index)
	{
		const float Time = Index / 24000.f;
		const float Envelope = FMath::Exp(-Time * 55.f) * FMath::Min(Time * 4000.f, 1.f);
		const float Sample = .55f * Noise.FRandRange(-1.f, 1.f) + .45f * FMath::Sin(2.f * PI * (Shot == EBadmintonShot::Smash ? 1500.f : 1050.f) * Time);
		PCM[Index] = static_cast<int16>(Sample * Envelope * Strength * 15000.f);
	}
	Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()), PCM.Num() * sizeof(int16));
	ContactAudio = UGameplayStatics::SpawnSound2D(this, Wave, .65f);
	if (ContactAudio)
	{
		// Procedural streams can remain alive after underrun; release this one-shot explicitly.
		TWeakObjectPtr<UAudioComponent> Voice = ContactAudio;
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer, [Voice]() { if (Voice.IsValid()) { Voice->Stop(); Voice->DestroyComponent(); } }, .25f, false);
	}
}

FString ABadmintonPlayerController::GetChallengeMessage() const
{
	return GetWorld()->GetTimeSeconds() < ChallengeMessageUntil ? ChallengeMessage : FString();
}
