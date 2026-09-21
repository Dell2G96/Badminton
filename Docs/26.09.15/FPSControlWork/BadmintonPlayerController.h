#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BadmintonTypes.h"
#include "BadmintonShuttleClock.h"
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
	UFUNCTION(Exec) void BadmintonStrike();
	UFUNCTION(Exec) void BadmintonCancelShot();
	bool IsTimingArmed() const { return bTimingArmed; }
	float GetTimingProgress() const;
	float GetTimingPerfectPosition() const;
	float GetTimingWindowWidth(float Seconds) const;
	FVector2D GetRacketScreenPosition() const { return RacketScreen; }
	FString GetTimingMessage() const;
	EBadmintonShot GetSelectedShot() const { return TimingShot; }
	bool IsRacketAligned() const;
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
	UFUNCTION(Server, Unreliable)
	void ServerRequestShuttleClock(uint32 Sequence);
	UFUNCTION(Client, Unreliable)
	void ClientReceiveShuttleClock(uint32 Sequence, double ServerGameTime);
	Badminton::FShuttleClock ShuttleClock;
	double NextClockProbeAt = 0.;
	double LastClockReplyAt = -1.;
	void UpdateMouseAim();
	void SelectTimedShot(EBadmintonShot Shot);
	void UpdateTiming();
	void RunTimingProbe(float DeltaTime);
	UFUNCTION(Server, Reliable) void ServerArmTiming(EBadmintonShot Shot, FVector2D Aim);
	UFUNCTION(Client, Reliable) void ClientArmTiming(EBadmintonShot Shot, FVector2D Aim, double StartAt, double PerfectAt, int32 RallyId, int32 Sequence);
	UFUNCTION(Server, Reliable) void ServerStrikeTiming(FVector Direction);
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
	FVector2D RacketScreen = FVector2D(.56, .56);
	double TimingStartAt = 0.;
	double TimingPerfectAt = 0.;
	int32 TimingRally = INDEX_NONE;
	int32 TimingSequence = INDEX_NONE;
	FString TimingMessage;
	double TimingMessageUntil = 0.;
	FRotator FirstPersonRotation = FRotator::ZeroRotator;
	bool bTimingProbe = false;
	int32 TimingProbeStage = 0;
	int32 TimingProbeShots = 0;
	int32 TimingProbeLastRally = INDEX_NONE;
	bool bTimingProbeCaptured = false;

	FVector2D ShotAim = FVector2D::ZeroVector;
	EBadmintonShot AimPreviewShot = EBadmintonShot::Clear;
	int32 AimRallyId = INDEX_NONE;
	UPROPERTY(EditDefaultsOnly, Category="Badminton|Input", meta=(ClampMin="0.0001", ClampMax="0.05"))
	float MouseAimSensitivity = .010f;
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
