#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BadmintonTypes.h"
#include "BadmintonShuttleClock.h"
#include "BadmintonPlayFeel.h"
#include "BadmintonPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ABadmintonShuttle;
class ABadmintonRacketPreview;
struct FInputActionValue;

UCLASS()
class FPS_DELL2G_API ABadmintonPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABadmintonPlayerController();
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(Exec)
	void BadmintonHost();
	UFUNCTION(Exec)
	void BadmintonJoin(const FString& Address);
	UFUNCTION(Exec)
	void BadmintonReady();
	UFUNCTION(Exec)
	void BadmintonClear();
	UFUNCTION(Exec) void BadmintonDrop();
	UFUNCTION(Exec) void BadmintonSmash();
	UFUNCTION(Exec) void BadmintonDash();
	UFUNCTION(Exec) void BadmintonSelectDrop();
	UFUNCTION(Exec) void BadmintonSelectSmash();
	UFUNCTION(Exec) void BadmintonSelectClear();
	UFUNCTION(Exec) void BadmintonSelectReceive();
	UFUNCTION(Exec) void BadmintonHairpin();
	UFUNCTION(Exec) void BadmintonStrike();
	UFUNCTION(Exec) void BadmintonCancelShot();
	bool IsTimingArmed() const { return bTimingArmed; }
	bool IsAutomaticTimingTestRunning() const { return bTimingProbe && !bProbeReported; }
	float GetTimingProgress() const;
	float GetTimingPerfectPosition() const;
	float GetTimingWindowWidth(float Seconds) const;
	FVector2D GetRacketScreenPosition() const;
	FString GetTimingMessage() const;
	EBadmintonShot GetSelectedShot() const { return TimingShot; }
	bool IsRacketAligned() const;
	Badminton::EContactHint GetContactHint() const;
	bool HasContactForecast() const { return bContactForecast; }
	float GetMouseSensitivity() const { return MouseLookSensitivity; }
	int32 GetAIDifficulty() const { return AIDifficulty; }
	FString GetChallengeMessage() const;
	const Badminton::FPracticeProgress& GetPracticeProgress() const { return Practice; }
	UFUNCTION(Exec) void BadmintonSensitivity(float Value);
	UFUNCTION(Exec) void BadmintonDifficulty(int32 Level);
	UFUNCTION(Client, Reliable) void ClientPracticeContact(int32 RallyId, int32 Sequence, int32 Hitter, EBadmintonShot Shot);
	UFUNCTION(Client, Reliable) void ClientPracticeLanding(int32 RallyId, int32 Winner, int32 Hitter, EBadmintonShot Shot, FVector Position);
	bool ResolveTimedContact(FVector& Target, float& Quality, bool& bAligned) const;
	FVector GetEyePosition() const;
	FRotator GetCourtViewRotation() const;

	UFUNCTION(Exec)
	void BadmintonEOSLogin();
	UFUNCTION(Exec)
	void BadmintonEOSHost();
	UFUNCTION(Exec)
	void BadmintonEOSFind();
	UFUNCTION(Exec)
	void BadmintonEOSJoin(int32 Index);
	UFUNCTION(Exec)
	void BadmintonEOSLeave();
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);
	UFUNCTION(Client, Reliable)
	void ClientShotFeedback(EBadmintonShot Shot, bool bContact, int32 RallyId, double ServerTime);
	bool GetShotFeedback(FString& Text, bool& bContact) const;
	UFUNCTION(Exec) void BadmintonAimReset();
	bool GetAimPreview(FVector& Target, EBadmintonShot& Shot) const;
	bool GetShuttleServerTime(double& ServerTime, double& RoundTripSeconds) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void UpdateShuttleClock();
	void RefreshContactForecast();
	Badminton::EContactHint EvaluateContact(const FVector& Direction) const;
	void SensitivityUp();
	void SensitivityDown();
	void CycleDifficulty();
	void LoadPracticeSettings();
	void SavePracticeSettings();
	void PlayContactSound(EBadmintonShot Shot);
	UFUNCTION(Client, Reliable) void ClientContactForecast(bool bFound, double PerfectAt, int32 RallyId, int32 Sequence);
	Badminton::FPracticeProgress Practice;
	FString ChallengeMessage;
	double ChallengeMessageUntil = 0.;
	bool bContactForecast = true;
	double NextForecastAt = 0.;
	int32 AIDifficulty = 1;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> ContactAudio;
	UFUNCTION(Server, Unreliable)
	void ServerRequestShuttleClock(uint32 Sequence);
	UFUNCTION(Client, Unreliable)
	void ClientReceiveShuttleClock(uint32 Sequence, double ServerGameTime);
	Badminton::FShuttleClock ShuttleClock;
	double NextClockProbeAt = 0.;
	double LastClockReplyAt = -1.;
	void UpdateMouseAim();
	bool IsMouseAimOverridden() const;
	void RestoreTimingProbeControl();
	bool bTimingProbeControlRestored = false;
	void ApplyMouseLook(float DeltaX, float DeltaY);
	void SelectTimedShot(EBadmintonShot Shot);
	void UpdateTiming();
	void RunTimingProbe(float DeltaTime);
	UFUNCTION(Server, Reliable) void ServerArmTiming(EBadmintonShot Shot, FVector2D Aim);
	UFUNCTION(Client, Reliable) void ClientArmTiming(EBadmintonShot Shot, FVector2D Aim, double StartAt, double PerfectAt, int32 RallyId, int32 Sequence);
	UFUNCTION(Server, Reliable) void ServerStrikeTiming(FVector Direction);
	UFUNCTION(Server, Reliable) void ServerInstantShot(EBadmintonShot Shot, FVector Direction, FVector2D Aim);
	void InstantShot(EBadmintonShot Shot);
	UFUNCTION(Server, Reliable) void ServerCancelTiming();
	UFUNCTION(Client, Reliable) void ClientTimingResult(const FString& Message);
	UPROPERTY(Transient) TObjectPtr<ABadmintonShuttle> TimingShuttle;
	UPROPERTY(Transient) TObjectPtr<ABadmintonRacketPreview> RacketPreview;
	bool bTimingArmed = false;
	bool bTimedDispatch = false;
	bool bTimedAligned = false;
	float DispatchQuality = 0.f;
	FVector DispatchTarget = FVector::ZeroVector;
	EBadmintonShot TimingShot = EBadmintonShot::Clear;
	FVector2D LockedAim = FVector2D::ZeroVector;
	double TimingStartAt = 0.;
	double TimingPerfectAt = 0.;
	int32 TimingRally = INDEX_NONE;
	int32 TimingSequence = INDEX_NONE;
	FString TimingMessage;
	double TimingMessageUntil = 0.;
	FVector2D CameraLook = FVector2D(0., -12.);
	bool bTimingProbe = false;
	int32 TimingProbeStage = 0;
	int32 TimingProbeShots = 0;
	int32 QuickReceiveProbeMask = 0;
	int32 TimingProbeLastRally = INDEX_NONE;
	bool bTimingProbeCaptured = false;
	bool bQuickReceiveProbeCaptured = false;
	bool bTimingProbeMissChecked = false;
	bool bContactReadyProbeCaptured = false;
	double RacketSwingUntil = 0.;

	FVector2D ShotAim = FVector2D::ZeroVector;
	EBadmintonShot AimPreviewShot = EBadmintonShot::Clear;
	int32 AimRallyId = INDEX_NONE;
	UPROPERTY(EditDefaultsOnly, Category="Badminton|Input", meta=(ClampMin="0.01", ClampMax="1.5"))
	float MouseLookSensitivity = .72f;
	bool bAimProbe = false;
	int32 AimProbeSequence = 0;
	int32 AimProbeObservedShots = 0;
	FString ShotFeedbackText;
	bool bLastShotContact = false;
	int32 FeedbackRallyId = INDEX_NONE;
	double FeedbackServerTime = -1;
	double FeedbackExpiresAt = 0;
	int32 ConfirmedShotCount = 0;
	bool bPresentationProbe = false;
	float PresentationProbeWait = 0.f;
	bool bSawConnectedCourt = false;
	void JoinFirstEOSRoom();
	void SendShot(EBadmintonShot Shot);
	void Input_Move(const FInputActionValue& Value, FVector2D Direction);
	void RunNetworkProbe(float DeltaTime);
	void RunShotProbe(float DeltaTime);
	void RunMatchProbe(float DeltaTime);
	void RunAbilityProbe(float DeltaTime);
	void RunAIProbe(float DeltaTime);
	bool bAIProbe = false;
	int32 AIProbeSequence = 0;
	int32 AIProbeReturns = 0;
	int32 AIProbeHumanShots = 0;
	float AIProbeMovement = 0.f;
	FVector AIProbePreviousPosition = FVector::ZeroVector;
	double AIProbeNextShotAt = 0.;
	bool bAbilityProbe = false;
	int32 AbilityProbeRally = 0;
	int32 AbilityProbePassed = 0;
	bool bAbilityProbeActivated = false;
	float AbilityProbeStamina = 0.f;
	bool bMatchProbe = false;
	bool bSawMatchFinished = false;
	int32 MatchProbeTargetCount = 1;
	int32 MatchProbeObservedCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MovementContext;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> MoveActions;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ReadyAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ClearAction;

	// Explicit development-only command line test; no automation in normal play.
	bool bNetworkProbe = false;
	bool bShotProbe = false;
	bool bCapturePrototype = false;
	bool bCaptureOnContact = false;
	bool bCaptureRequested = false;
	float CaptureTime = 0.f;
	float ShotProbeTime = 0.f;
	bool bProbeReported = false;
	float ProbeTime = 0.f;
	FVector ProbeStart = FVector::ZeroVector;
	FVector RemoteStart = FVector::ZeroVector;
	bool bProbeStarted = false;
};
