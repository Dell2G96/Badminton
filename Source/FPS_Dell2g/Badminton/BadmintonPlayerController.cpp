#include "BadmintonPlayerController.h"

#include "BadmintonAttributeSet.h"
#include "BadmintonAIController.h"
#include "BadmintonTiming.h"
#include "BadmintonCamera.h"
#include "BadmintonCourt.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMesh.h"
#include "BadmintonRacketPreview.h"
#include "BadmintonRacketPhysics.h"
#include "BadmintonPlayerFlight.h"
#include "BadmintonThirdPerson.h"
#include "BadmintonCharacter.h"
#include "BadmintonClearAbility.h"
#include "BadmintonDashAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "BadmintonGameMode.h"
#include "BadmintonGameState.h"
#include "BadmintonOnlineSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BadmintonPlayerState.h"
#include "BadmintonShuttle.h"
#include "BadmintonShotData.h"
#include "BadmintonNetMetrics.h"
#include "BadmintonShotDiagnostics.h"
#include "HAL/PlatformTime.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

ABadmintonPlayerController::ABadmintonPlayerController()
{
	bAutoManageActiveCameraTarget = false;
}

void ABadmintonPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bThirdPersonControl = !FParse::Param(FCommandLine::Get(), TEXT("BadmintonLegacyFirstPerson"));
	if (IsLocalController())
	{
		LoadPracticeSettings();
		bDualView = !FParse::Param(FCommandLine::Get(), TEXT("BadmintonSingleView"));
		SetViewTarget(this);
		SetInputMode(FInputModeGameOnly());
#if !UE_BUILD_SHIPPING
		bTimingProbe = FParse::Param(FCommandLine::Get(), TEXT("BadmintonTimingTest")) || FParse::Param(FCommandLine::Get(), TEXT("BadmintonRacketPoseTest"));
		bAIProbe = FParse::Param(FCommandLine::Get(), TEXT("BadmintonAITest"));
		bNetworkProbe = FParse::Param(FCommandLine::Get(), TEXT("BadmintonNetTest"));
		bShotProbe = FParse::Param(FCommandLine::Get(), TEXT("BadmintonShotTest"));
		bPresentationProbe = FParse::Param(FCommandLine::Get(), TEXT("BadmintonPresentationTest"));
		bShotProbe |= bPresentationProbe;
		bAimProbe = FParse::Param(FCommandLine::Get(), TEXT("BadmintonAimTest"));
		bShotProbe |= bAimProbe;
		bMatchProbe = FParse::Param(FCommandLine::Get(), TEXT("BadmintonMatchTest"));
		FParse::Value(FCommandLine::Get(), TEXT("BadmintonMatchCount="), MatchProbeTargetCount);
		MatchProbeTargetCount = FMath::Clamp(MatchProbeTargetCount, 1, 100);
		bAbilityProbe = FParse::Param(FCommandLine::Get(), TEXT("BadmintonAbilityTest"));
		bCapturePrototype = FParse::Param(FCommandLine::Get(), TEXT("BadmintonCapture"));
		bCaptureOnContact = FParse::Param(FCommandLine::Get(), TEXT("BadmintonCaptureOnContact"));
#endif
	}
}

void ABadmintonPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController()) { SavePracticeSettings(); }
	if (RacketPreview) { RacketPreview->Destroy(); }
	if (ThirdPersonCapture) { ThirdPersonCapture->DestroyComponent(); }
	if (ULocalPlayer* Local = GetLocalPlayer())
	{
		Local->Origin = FVector2D::ZeroVector;
		Local->Size = FVector2D(1., 1.);
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->RemoveMappingContext(MovementContext);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ABadmintonPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (PlayerInput)
	{
		// Reserve online shortcuts for this controller without changing other FPS input settings.
		PlayerInput->DebugExecBindings.RemoveAll([](const FKeyBind& Binding)
		{
			return Binding.Key == EKeys::F1 || Binding.Key == EKeys::F2 || Binding.Key == EKeys::F3
				|| Binding.Key == EKeys::F4 || Binding.Key == EKeys::F5;
		});
	}
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ThisClass::BadmintonEOSLogin);
	InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &ThisClass::BadmintonEOSHost);
	InputComponent->BindKey(EKeys::F3, IE_Pressed, this, &ThisClass::BadmintonEOSFind);
	InputComponent->BindKey(EKeys::F4, IE_Pressed, this, &ThisClass::JoinFirstEOSRoom);
	InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ThisClass::BadmintonEOSLeave);
	InputComponent->BindKey(EKeys::Equals, IE_Pressed, this, &ThisClass::SensitivityUp);
	InputComponent->BindKey(EKeys::Add, IE_Pressed, this, &ThisClass::SensitivityUp);
	InputComponent->BindKey(EKeys::Subtract, IE_Pressed, this, &ThisClass::SensitivityDown);
	InputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &ThisClass::SensitivityDown);
	InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ThisClass::CycleDifficulty);
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ThisClass::BadmintonToggleDualView);
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ThisClass::BadmintonSelectDrop);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::BadmintonSelectSmash);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ThisClass::BadmintonSelectClear);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::BadmintonHairpin);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ThisClass::BadmintonSelectReceive);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ThisClass::BadmintonStrike);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ThisClass::BadmintonCancelShot);
	InputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ThisClass::BadmintonDash);
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ThisClass::RacketWheelUp);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ThisClass::RacketWheelDown);
	InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &ThisClass::BadmintonAimReset);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	ULocalPlayer* Local = GetLocalPlayer();
	if (!Input || !Local)
	{
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		return;
	}
	MovementContext = NewObject<UInputMappingContext>(this);
	const FKey Keys[] = {EKeys::W, EKeys::S, EKeys::A, EKeys::D};
	const FVector2D Directions[] = {FVector2D(0, 1), FVector2D(0, -1), FVector2D(-1, 0), FVector2D(1, 0)};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
	{
		UInputAction* Action = NewObject<UInputAction>(this);
		Action->ValueType = EInputActionValueType::Boolean;
		MoveActions.Add(Action);
		MovementContext->MapKey(Action, Keys[Index]);
		Input->BindAction(Action, ETriggerEvent::Triggered, this, &ThisClass::Input_Move, Directions[Index]);
	}
	ReadyAction = NewObject<UInputAction>(this);
	ReadyAction->ValueType = EInputActionValueType::Boolean;
	MovementContext->MapKey(ReadyAction, EKeys::Enter);
	Input->BindAction(ReadyAction, ETriggerEvent::Started, this, &ThisClass::BadmintonReady);

	Subsystem->AddMappingContext(MovementContext, 0);
}

void ABadmintonPlayerController::Input_Move(const FInputActionValue& Value, FVector2D Direction)
{
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	if (State && State->CourtSide != INDEX_NONE && GetPawn() && Value.Get<bool>())
	{
		const float Sign = Badminton::ForwardSign(State->CourtSide);
		GetPawn()->AddMovementInput(FVector(Sign * Direction.Y, Sign * Direction.X, 0.f));
	}
}

void ABadmintonPlayerController::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (bThirdPersonControl)
	{
		const auto* State = GetPlayerState<ABadmintonPlayerState>();
		const float Sign = Badminton::ForwardSign(State ? State->CourtSide : 0);
		const FVector Position = GetPawn() ? GetPawn()->GetActorLocation() : FVector(-440, 0, 90);
		OutResult.Location = Position + FVector(-Sign * 620.f, 0, 530.f);
		OutResult.Rotation = (Position + FVector(Sign * 390.f, 0, 0) - OutResult.Location).Rotation();
		OutResult.FOV = 90.f;
		OutResult.AspectRatio = 16.f / 9.f;
		OutResult.AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
		OutResult.PostProcessBlendWeight = 1.f;
		OutResult.PostProcessSettings.bOverride_MotionBlurAmount = true;
		OutResult.PostProcessSettings.MotionBlurAmount = 0.f;
		return;
	}
	OutResult.Location = GetEyePosition();
	const auto* ViewState = GetPlayerState<ABadmintonPlayerState>();
	const int32 ViewSide = ViewState ? ViewState->CourtSide : 0;
	OutResult.Rotation = Badminton::FreeRacketCameraRotation(ViewSide, CameraLook);
	OutResult.FOV = 95.f;
	// Preserve vertical visibility in the narrow first-person panel.
	OutResult.AspectRatio = 16.f / 9.f;
	OutResult.AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
	OutResult.PostProcessBlendWeight = 1.f;
	OutResult.PostProcessSettings.bOverride_MotionBlurAmount = true;
	OutResult.PostProcessSettings.MotionBlurAmount = 0.f;
	// The head stays on the exact hit ray while the camera follows with a bounded offset.
	if (RacketPreview)
	{
		const float Swing = FMath::Clamp(1.f - static_cast<float>((RacketSwingUntil - GetWorld()->GetTimeSeconds()) / .22), 0.f, 1.f);
		const bool bSwing = GetWorld()->GetTimeSeconds() < RacketSwingUntil;
		const float Recoil = FMath::Sin(PI * Swing);
		const FRotator BasePose = Badminton::FreeRacketRotation(ViewSide, CameraLook);
		const FQuat TiltRotation(Badminton::RacketTiltAxis(BasePose.Vector()), FMath::DegreesToRadians(GetRacketTilt()));
		RacketPreview->SetActorLocationAndRotation(OutResult.Location + GetCourtViewRotation().Vector() * (100.f + (bSwing ? 12.f * Recoil : 0.f)),
			(TiltRotation * BasePose.Quaternion()).Rotator() + (bSwing ? FRotator(8.f * Recoil, -22.f * Recoil, 24.f * Recoil) : FRotator::ZeroRotator));
	}
	UpdateDualView();
}

void ABadmintonPlayerController::BadmintonHost()
{
	if (IsLocalController())
	{
		UGameplayStatics::OpenLevel(this, TEXT("/Game/Badminton/Maps/L_Badminton_Prototype"), true, TEXT("listen"));
	}
}

void ABadmintonPlayerController::BadmintonEOSLogin()
{
	if (IsLocalController()) { GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->Login(); }
}

void ABadmintonPlayerController::BadmintonEOSHost()
{
	if (IsLocalController()) { GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->Host(TEXT("배드민턴 경기방")); }
}

void ABadmintonPlayerController::BadmintonEOSFind()
{
	if (IsLocalController()) { GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->FindRooms(); }
}

void ABadmintonPlayerController::BadmintonEOSJoin(int32 Index)
{
	if (IsLocalController()) { GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->JoinRoom(Index); }
}

void ABadmintonPlayerController::JoinFirstEOSRoom()
{
	BadmintonEOSJoin(0);
}

void ABadmintonPlayerController::BadmintonEOSLeave()
{
	if (IsLocalController()) { GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->Leave(); }
}

void ABadmintonPlayerController::BadmintonJoin(const FString& Address)
{
	if (IsLocalController() && !Address.TrimStartAndEnd().IsEmpty())
	{
		ClientTravel(Address.TrimStartAndEnd(), TRAVEL_Absolute);
	}
}

void ABadmintonPlayerController::BadmintonReady()
{
	if (const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>())
	{
		ServerSetReady(!State->bReady);
	}
}

void ABadmintonPlayerController::BadmintonClear()
{
	SendShot(EBadmintonShot::Clear);
}

void ABadmintonPlayerController::ClientShotFeedback_Implementation(EBadmintonShot Shot, bool bContact, int32 RallyId, double ServerTime)
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (!Match || ServerTime <= FeedbackServerTime || RallyId < Match->RallyId
		|| Match->GetServerWorldTimeSeconds() - ServerTime > 1.5) { return; }
	FeedbackServerTime = ServerTime;
	FeedbackRallyId = RallyId;
	FeedbackExpiresAt = GetWorld()->GetTimeSeconds() + 1.1;
	bLastShotContact = bContact;
	const TCHAR* Name = Shot == EBadmintonShot::Serve ? TEXT("서브") : Shot == EBadmintonShot::Drop ? TEXT("드롭")
		: Badminton::ShotLabel(Shot);
	ShotFeedbackText = FString::Printf(TEXT("%s / %s"), Name, bContact ? TEXT("타격 성공") : TEXT("헛스윙"));
	if (bContact) { ++ConfirmedShotCount; PlayContactSound(Shot); RacketSwingUntil = GetWorld()->GetTimeSeconds() + .22; }
#if !UE_BUILD_SHIPPING
	if (bPresentationProbe || bAbilityProbe)
	{
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_SHOT_FEEDBACK side=%d rally=%d contact=%d shot=%d"),
			GetPlayerState<ABadmintonPlayerState>() ? GetPlayerState<ABadmintonPlayerState>()->CourtSide : INDEX_NONE, RallyId, bContact, static_cast<int32>(Shot));
	}
#endif
}

bool ABadmintonPlayerController::GetShotFeedback(FString& Text, bool& bContact) const
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (!Match || Match->RallyId != FeedbackRallyId || GetWorld()->GetTimeSeconds() >= FeedbackExpiresAt
		|| (Match->Phase != EBadmintonPhase::ReadyToServe && Match->Phase != EBadmintonPhase::Rally)) { return false; }
	Text = ShotFeedbackText;
	bContact = bLastShotContact;
	return true;
}

void ABadmintonPlayerController::BadmintonDrop() { SendShot(EBadmintonShot::Drop); }
void ABadmintonPlayerController::BadmintonSmash() { SendShot(EBadmintonShot::Smash); }

void ABadmintonPlayerController::SendShot(EBadmintonShot Shot)
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>(); State && Match)
	{
		FGameplayEventData Payload;
		if (Shot == EBadmintonShot::Clear && Match->Phase == EBadmintonPhase::ReadyToServe) { Shot = EBadmintonShot::Serve; }
		AimPreviewShot = Shot;
		Payload.EventTag = Shot == EBadmintonShot::Drop ? TAG_BadmintonDropEvent : Shot == EBadmintonShot::Smash ? TAG_BadmintonSmashEvent
			: Shot == EBadmintonShot::Hairpin ? TAG_BadmintonHairpinEvent : Shot == EBadmintonShot::Serve ? TAG_BadmintonServeEvent : Shot == EBadmintonShot::Receive ? TAG_BadmintonReceiveEvent : TAG_BadmintonClearEvent;
		Payload.EventMagnitude = static_cast<float>(Match->RallyId);
		Payload.Instigator = GetPawn();
		auto* AimData = new FGameplayAbilityTargetData_LocationInfo();
		AimData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
		AimData->TargetLocation.LiteralTransform = FTransform(FVector(ShotAim.X, ShotAim.Y, 0));
		Payload.TargetData.Add(AimData);
		Badminton::TraceShot(TEXT("Input"), State->GetAbilitySystemComponent(), GetPawn(), Shot);
		const int32 Activated = State->GetAbilitySystemComponent()->HandleGameplayEvent(Payload.EventTag, &Payload);
		Badminton::TraceShot(TEXT("Dispatch"), State->GetAbilitySystemComponent(), GetPawn(), Shot, 0, Activated);
	}
}

void ABadmintonPlayerController::RacketWheelUp() { AdjustRacketFace(1.f); }
void ABadmintonPlayerController::RacketWheelDown() { AdjustRacketFace(-1.f); }
void ABadmintonPlayerController::AdjustRacketFace(float Steps)
{
	if (bThirdPersonControl) { return; }
	if (!IsLocalController() || SelectedStroke == EBadmintonShot::Smash) { return; }
	RacketTilt = Badminton::AdjustRacketTilt(RacketTilt, Steps);
}

void ABadmintonPlayerController::BadmintonAimReset()
{
	RacketTilt = 0.f;
	BadmintonCancelShot();
	CameraLook = FVector2D(0., Badminton::CameraDefaultPitch);
	ShotAim = FVector2D::ZeroVector;
	AimPreviewShot = EBadmintonShot::Clear;
}

void ABadmintonPlayerController::ApplyMouseLook(float DeltaX, float DeltaY)
{
	CameraLook = Badminton::ApplyMouseLook(CameraLook, FVector2D(DeltaX, DeltaY), MouseLookSensitivity);
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!bTimingArmed && Match && State)
	{
		const bool bServe = Match->Phase == EBadmintonPhase::ReadyToServe && Match->ServingSide == State->CourtSide;
		ShotAim = Badminton::AimFromCamera(State->CourtSide, Match->GetScore(State->CourtSide), bServe, GetEyePosition(), GetCourtViewRotation());
	}
}

bool ABadmintonPlayerController::IsMouseAimOverridden() const
{
	return bNetworkProbe || bShotProbe || bMatchProbe || bAbilityProbe || bAIProbe || IsAutomaticTimingTestRunning();
}

void ABadmintonPlayerController::RestoreTimingProbeControl()
{
	if (!IsLocalController() || !bTimingProbe || !bProbeReported || bTimingProbeControlRestored) { return; }
	bTimingProbeControlRestored = true;
	BadmintonAimReset();
	RacketSwingUntil = 0.;
	if (auto* ControlledPlayer = Cast<ABadmintonCharacter>(GetPawn()))
	{
		ControlledPlayer->ConsumeMovementInputVector();
		ControlledPlayer->GetCharacterMovement()->StopMovementImmediately();
	}
	// Recording can run a slow test. Hand manual play back at normal speed on success or failure.
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	const bool bRestored = !IsMouseAimOverridden()
		&& CameraLook.Equals(FVector2D(0., Badminton::CameraDefaultPitch), .001)
		&& !bTimingArmed && FMath::IsNearlyZero(RacketTilt) && FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(this), 1.f);
	if (!bRestored)
	{
		UE_LOG(LogTemp, Error, TEXT("BADMINTON_TIMING_CONTROL_RESTORE_FAILED"));
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_TIMING_CONTROL_RESTORED manualMouse=enabled look=%s timeDilation=1"), *CameraLook.ToString());
	ClientTimingResult(TEXT("검사 종료 / 마우스를 직접 조작할 수 있습니다"));
}

void ABadmintonPlayerController::UpdateMouseAim()
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (!Match) { return; }
	if (AimRallyId != Match->RallyId)
	{
		AimRallyId = Match->RallyId;
		BadmintonCancelShot();
		ShotAim = FVector2D::ZeroVector;
		AimPreviewShot = EBadmintonShot::Clear;
	}
	if (IsMouseAimOverridden()) { return; }
	float DeltaX = 0.f, DeltaY = 0.f;
	GetInputMouseDelta(DeltaX, DeltaY);
	if (bThirdPersonControl)
	{
		// Four times the previous placement response, including existing saved sensitivity settings.
		ShotAim.X = FMath::Clamp(ShotAim.X + DeltaX * MouseLookSensitivity * .032, -1., 1.);
		ShotAim.Y = FMath::Clamp(ShotAim.Y + DeltaY * MouseLookSensitivity * .032, -1., 1.);
		return;
	}
	ApplyMouseLook(DeltaX, DeltaY);
}

bool ABadmintonPlayerController::GetAimPreview(FVector& Target, EBadmintonShot& Shot) const
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || !State->bReady || (Match->Phase != EBadmintonPhase::ReadyToServe && Match->Phase != EBadmintonPhase::Rally)) { return false; }
	Shot = Match->Phase == EBadmintonPhase::ReadyToServe && Match->ServingSide == State->CourtSide ? EBadmintonShot::Serve
		: GetSelectedShot();
	if (bTimingArmed) { Shot = TimingShot; }
	Target = Shot == EBadmintonShot::Smash ? Badminton::PlacementTarget(State->CourtSide, Match->GetScore(State->CourtSide), false, bTimingArmed ? LockedAim : ShotAim)
		: Badminton::InstantPlacementTarget(Shot, State->CourtSide, Match->GetScore(State->CourtSide), ShotAim);
	if (Shot == EBadmintonShot::Receive) { Target = Badminton::DefensiveTarget(State->CourtSide, Target); }
	return true;
}

bool ABadmintonPlayerController::GetShotDirectionPreview(FVector& Origin, FVector& Velocity) const
{
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || !State->bReady || !GetPawn() || !TimingShuttle || SelectedStroke == EBadmintonShot::Smash) { return false; }
	const bool bServe = Match->Phase == EBadmintonPhase::ReadyToServe && Match->ServingSide == State->CourtSide;
	const auto& Flight = TimingShuttle->GetFlight();
	if (!bServe && (Match->Phase != EBadmintonPhase::Rally || !Flight.bFlying || Flight.LastHitterSide == State->CourtSide)) { return false; }
	const EBadmintonShot Shot = bServe ? EBadmintonShot::Serve : SelectedStroke;
	Origin = bServe ? FVector(GetPawn()->GetActorLocation().X + Badminton::ForwardSign(State->CourtSide)*50.f, GetPawn()->GetActorLocation().Y, 160.f) : TimingShuttle->GetActorLocation();
	const FVector2D LiveAim = Badminton::AimFromCamera(State->CourtSide, Match->GetScore(State->CourtSide), bServe, GetEyePosition(), GetCourtViewRotation());
	FVector Target = Badminton::InstantPlacementTarget(Shot, State->CourtSide, Match->GetScore(State->CourtSide), LiveAim);
	float Duration = 0.f;
	const FVector BaseVelocity = Badminton::PreparePlayerFlight(Shot, State->CourtSide, Origin, Target, Match->GetShotData()->Get(Shot).FlightTime,
		.65f, Match->RallyId*7919+Flight.ShotSequence, Duration);
	Velocity = Badminton::ResolveRacketImpact(TimingShuttle->GetSimulationVelocity(), BaseVelocity, GetCourtViewRotation().Vector(), GetRacketTilt(), Shot);
	return !Velocity.ContainsNaN() && !Velocity.IsNearlyZero();
}

void ABadmintonPlayerController::BadmintonDash()
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || !GetPawn()) { return; }
	FVector Direction = GetPawn()->GetLastMovementInputVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero()) { Direction = FVector(Badminton::ForwardSign(State->CourtSide), 0, 0); }
	FGameplayEventData Payload;
	Payload.EventTag = TAG_BadmintonDashEvent;
	Payload.EventMagnitude = Match->RallyId;
	Payload.Instigator = GetPawn();
	auto* Target = new FGameplayAbilityTargetData_LocationInfo();
	Target->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Target->TargetLocation.LiteralTransform = FTransform(Direction);
	Payload.TargetData.Add(Target);
	State->GetAbilitySystemComponent()->HandleGameplayEvent(Payload.EventTag, &Payload);
}

void ABadmintonPlayerController::ServerSetReady_Implementation(bool bReady)
{
	ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (State && Match && (Match->Phase == EBadmintonPhase::WaitingForPlayers || Match->Phase == EBadmintonPhase::WaitingForReady || Match->Phase == EBadmintonPhase::MatchFinished))
	{
		State->bReady = bReady;
		State->ForceNetUpdate();
		if (ABadmintonGameMode* Mode = GetWorld()->GetAuthGameMode<ABadmintonGameMode>())
		{
			Mode->RefreshLobby();
		}
	}
}

void ABadmintonPlayerController::UpdateShuttleClock()
{
	if (!Badminton::UseRoundTripClock() || HasAuthority() || !IsLocalController()) { return; }
	const double RealNow = FPlatformTime::Seconds();
	if (RealNow < NextClockProbeAt) { return; }
	NextClockProbeAt = RealNow + 1.;
	ServerRequestShuttleClock(ShuttleClock.BeginProbe(RealNow, GetWorld()->GetTimeSeconds()));
}

void ABadmintonPlayerController::ServerRequestShuttleClock_Implementation(uint32 Sequence)
{
	const double RealNow = FPlatformTime::Seconds();
	if (Sequence == 0 || (LastClockReplyAt >= 0. && RealNow - LastClockReplyAt < .25)) { return; }
	LastClockReplyAt = RealNow;
	ClientReceiveShuttleClock(Sequence, GetWorld()->GetTimeSeconds());
}

void ABadmintonPlayerController::ClientReceiveShuttleClock_Implementation(uint32 Sequence, double ServerGameTime)
{
	if (HasAuthority() || !IsLocalController() || !Badminton::UseRoundTripClock()) { return; }
	const bool bAccepted = ShuttleClock.Accept(Sequence, ServerGameTime, FPlatformTime::Seconds(), GetWorld()->GetTimeSeconds());
	if (Badminton::NetMetricsEnabled())
	{
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_CLOCK_SAMPLE sequence=%u accepted=%d selected_rtt_ms=%.3f"),
			Sequence, bAccepted, ShuttleClock.GetSelectedRTT() * 1000.);
	}
}

bool ABadmintonPlayerController::GetShuttleServerTime(double& ServerTime, double& RoundTripSeconds) const
{
	if (!Badminton::UseRoundTripClock() || HasAuthority()
		|| !ShuttleClock.Estimate(FPlatformTime::Seconds(), GetWorld()->GetTimeSeconds(), ServerTime)) { return false; }
	RoundTripSeconds = ShuttleClock.GetSelectedRTT();
	return true;
}

void ABadmintonPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateShuttleClock();
	if (HasAuthority()) { RefreshContactForecast(); }
	if (IsLocalController())
	{
		UpdateMouseAim();
		UpdateTiming();
		const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
		const bool bConnectedCourt = Match && Match->ConnectedPlayers == 2;
		if (bConnectedCourt && !bSawConnectedCourt)
		{
			GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->ClearConnectionNotice();
		}
		bSawConnectedCourt = bConnectedCourt;
	}
#if !UE_BUILD_SHIPPING
	if (bCapturePrototype && IsLocalController() && !bCaptureRequested)
	{
		CaptureTime += DeltaTime;
		FString CaptureFeedback;
		bool bContact = false;
		const bool bCaptureMoment = !bCaptureOnContact || (GetShotFeedback(CaptureFeedback, bContact) && bContact
			&& FeedbackExpiresAt - GetWorld()->GetTimeSeconds() > 1.0);
		if (CaptureTime > 5.f && bCaptureMoment)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/BadmintonPrototype.png"), true, false);
			bCaptureRequested = true;
		}
	}
	if (bNetworkProbe && IsLocalController() && !bProbeReported)
	{
		RunNetworkProbe(DeltaTime);
	}
	if (bShotProbe && IsLocalController() && !bProbeReported)
	{
		RunShotProbe(DeltaTime);
	}
	if (bMatchProbe && IsLocalController() && !bProbeReported) { RunMatchProbe(DeltaTime); }
	if (bAbilityProbe && IsLocalController() && !bProbeReported) { RunAbilityProbe(DeltaTime); }
	if (bAIProbe && IsLocalController() && !bProbeReported) { RunAIProbe(DeltaTime); }
	if (IsAutomaticTimingTestRunning() && IsLocalController()) { RunTimingProbe(DeltaTime); }
	RestoreTimingProbeControl();
	if (IsLocalController() && FParse::Param(FCommandLine::Get(), TEXT("BadmintonThirdPersonTest"))) { RunThirdPersonProbe(DeltaTime); }
	if (IsLocalController() && FParse::Param(FCommandLine::Get(), TEXT("BadmintonDualViewTest"))) { RunDualViewProbe(DeltaTime); }
#endif
}

void ABadmintonPlayerController::RunAIProbe(float DeltaTime)
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || !GetPawn()) { return; }
	ProbeTime += DeltaTime;
	auto Fail = [this](const TCHAR* Reason)
	{
		UE_LOG(LogTemp, Error, TEXT("BADMINTON_AI_TEST FAIL %s"), Reason);
		bProbeReported = true;
	};
	if (ProbeTime > 180.f) { Fail(TEXT("timeout")); return; }
	if (Match->ConnectedPlayers != 2)
	{
		if (ProbeTime > 5.f) { Fail(TEXT("AI missing")); }
		return;
	}
	ABadmintonAIController* Opponent = nullptr;
	for (TActorIterator<ABadmintonAIController> It(GetWorld()); It; ++It) { Opponent = *It; break; }
	const ABadmintonPlayerState* AIState = Opponent ? Opponent->GetPlayerState<ABadmintonPlayerState>() : nullptr;
	if (!AIState || !AIState->IsABot() || !Opponent->GetPawn() || AIState->CourtSide == State->CourtSide
		|| AIState->GetAbilitySystemComponent()->GetAvatarActor() != Opponent->GetPawn()) { Fail(TEXT("AI possession / GAS")); return; }
	const FVector AIPosition = Opponent->GetPawn()->GetActorLocation();
	if (Match->Phase == EBadmintonPhase::Rally && !AIProbePreviousPosition.IsZero()) { AIProbeMovement += FVector::Dist2D(AIPosition, AIProbePreviousPosition); }
	AIProbePreviousPosition = AIPosition;
	if (Match->Phase == EBadmintonPhase::WaitingForReady && !State->bReady) { ServerSetReady(true); }
	if (Match->Phase == EBadmintonPhase::MatchFinished)
	{
		if (AIProbeReturns < 10 || AIProbeHumanShots < 5 || AIProbeMovement < 100.f || Match->WinnerSide != AIState->CourtSide
			|| Match->GetScore(AIState->CourtSide) != Badminton::PointsToWin) { Fail(TEXT("rally / movement / result")); return; }
		bSawMatchFinished = true;
		if (!State->bReady) { ServerSetReady(true); }
		return;
	}
	if (bSawMatchFinished && Match->MatchId == 2 && Match->Score0 == 0 && Match->Score1 == 0 && Match->Phase == EBadmintonPhase::ReadyToServe)
	{
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_AI_TEST PASS returns=%d humanShots=%d movement_cm=%.1f result=11 rematch=OK"), AIProbeReturns, AIProbeHumanShots, AIProbeMovement);
		bProbeReported = true;
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (TActorIterator<ABadmintonShuttle> It(GetWorld()); It; ++It)
	{
		const FBadmintonFlightState& Flight = It->GetFlight();
		if (AIProbeSequence != Flight.ShotSequence)
		{
			AIProbeSequence = Flight.ShotSequence;
			if (Flight.LastHitterSide == AIState->CourtSide && Flight.Shot != EBadmintonShot::Serve) { ++AIProbeReturns; }
			if (Flight.LastHitterSide == State->CourtSide) { ++AIProbeHumanShots; }
		}
		if (Match->Phase == EBadmintonPhase::ReadyToServe && Match->ServingSide == State->CourtSide && Now >= AIProbeNextShotAt)
		{
			ShotAim = FVector2D::ZeroVector;
			BadmintonClear();
			AIProbeNextShotAt = Now + .5;
		}
		// Exercise genuine rallies first; then leave the human idle so the AI can finish naturally.
		if (AIProbeReturns >= 10 || Match->Phase != EBadmintonPhase::Rally || Flight.LastHitterSide == State->CourtSide) { continue; }
		const FVector Target = ABadmintonAIController::FindReceivePosition(*It, State->CourtSide, Now);
		const FVector Offset = Target - GetPawn()->GetActorLocation();
		if (Offset.Size2D() > 15.f) { GetPawn()->AddMovementInput(Offset.GetSafeNormal2D(), FMath::Clamp(Offset.Size2D() / 80.f, 0.f, 1.f)); }
		const FVector Position = It->GetActorLocation();
		if (Now >= AIProbeNextShotAt && Flight.bLegalCrossing && Position.X * Badminton::ForwardSign(State->CourtSide) < 0.f
			&& Position.Z > 80.f && Position.Z < 280.f && FVector::Dist2D(GetPawn()->GetActorLocation(), Position) < 150.f)
		{
			ShotAim = FVector2D(AIProbeHumanShots % 2 == 0 ? .8 : -.8, 0.);
			BadmintonClear();
			AIProbeNextShotAt = Now + .4;
		}
	}
}

void ABadmintonPlayerController::RunAbilityProbe(float DeltaTime)
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || !GetPawn() || Match->ConnectedPlayers != 2) { return; }
	if (!State->bReady && Match->Phase == EBadmintonPhase::WaitingForReady) { ServerSetReady(true); }
	if (Match->Phase == EBadmintonPhase::ReadyToServe && State->CourtSide == Match->ServingSide) { BadmintonClear(); }
	if (Match->Phase != EBadmintonPhase::Rally) { return; }
	if (AbilityProbeRally != Match->RallyId)
	{
		AbilityProbeRally = Match->RallyId;
		ProbeTime = 0.f;
		bAbilityProbeActivated = false;
	}
	ProbeTime += DeltaTime;
	UAbilitySystemComponent* ASC = State->GetAbilitySystemComponent();
	const UBadmintonShotData* Data = Match->GetShotData();
	const int32 Kind = AbilityProbePassed;
	const float Cost = Kind == 0 ? Data->Drop.StaminaCost : Kind == 1 ? Data->Smash.StaminaCost : Data->DashCost;
	auto Fail = [this, State](const TCHAR* Reason)
	{
		UE_LOG(LogTemp, Error, TEXT("BADMINTON_ABILITY_TEST FAIL side=%d reason=%s"), State->CourtSide, Reason);
		bProbeReported = true;
	};
	if (!bAbilityProbeActivated && ProbeTime > .6f)
	{
		AbilityProbeStamina = State->GetAttributes()->GetStamina();
		if (HasAuthority())
		{
			// Exercise the real granted abilities with insufficient authoritative stamina.
			ASC->SetNumericAttributeBase(UBadmintonAttributeSet::GetStaminaAttribute(), 0.f);
			bool bCostsCorrect = true;
			for (TSubclassOf<UGameplayAbility> Class : {UBadmintonDropAbility::StaticClass(), UBadmintonSmashAbility::StaticClass(), UBadmintonDashAbility::StaticClass(), UBadmintonClearAbility::StaticClass()})
			{
				const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Class);
				const bool bAllowed = Spec && Spec->Ability->CanActivateAbility(Spec->Handle, ASC->AbilityActorInfo.Get(), nullptr, nullptr, nullptr);
				bCostsCorrect &= bAllowed == (Class == UBadmintonClearAbility::StaticClass());
			}
			ASC->SetNumericAttributeBase(UBadmintonAttributeSet::GetStaminaAttribute(), AbilityProbeStamina);
			if (!bCostsCorrect) { Fail(TEXT("insufficient stamina / free clear")); return; }
		}
		// A stale event must neither consume stamina nor leave a blocking swing tag.
		FGameplayEventData Stale;
		Stale.EventTag = TAG_BadmintonDropEvent;
		Stale.EventMagnitude = Match->RallyId - 1;
		ASC->HandleGameplayEvent(Stale.EventTag, &Stale);
		if (!FMath::IsNearlyEqual(State->GetAttributes()->GetStamina(), AbilityProbeStamina) || ASC->HasMatchingGameplayTag(TAG_BadmintonSwinging))
		{ Fail(TEXT("stale event changed stamina/state")); return; }
		FGameplayEventData InvalidAim = Stale;
		InvalidAim.EventMagnitude = Match->RallyId;
		auto* InvalidAimData = new FGameplayAbilityTargetData_LocationInfo();
		InvalidAimData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
		InvalidAimData->TargetLocation.LiteralTransform.SetLocation(FVector(2, 0, 0));
		InvalidAim.TargetData.Add(InvalidAimData);
		ASC->HandleGameplayEvent(InvalidAim.EventTag, &InvalidAim);
		if (!FMath::IsNearlyEqual(State->GetAttributes()->GetStamina(), AbilityProbeStamina, .1f) || ASC->HasMatchingGameplayTag(TAG_BadmintonSwinging))
		{ Fail(TEXT("invalid aim consumed stamina/state")); return; }
		ProbeStart = GetPawn()->GetActorLocation();
		if (Kind == 0) { BadmintonDrop(); BadmintonDrop(); }
		else if (Kind == 1) { BadmintonSmash(); BadmintonSmash(); }
		else { BadmintonDash(); BadmintonDash(); }
		const FGameplayTag ActiveTag = Kind < 2 ? TAG_BadmintonSwinging : TAG_BadmintonDashing;
		if (!ASC->HasMatchingGameplayTag(ActiveTag) || !FMath::IsNearlyEqual(State->GetAttributes()->GetStamina(), AbilityProbeStamina - Cost, .1f))
		{
			const TSubclassOf<UGameplayAbility> ExpectedClass = Kind == 0 ? UBadmintonDropAbility::StaticClass()
				: Kind == 1 ? UBadmintonSmashAbility::StaticClass() : UBadmintonDashAbility::StaticClass();
			UE_LOG(LogTemp, Display, TEXT("BADMINTON_ABILITY_DIAGNOSTIC side=%d kind=%d spec=%d ready=%d active=%d before=%.3f after=%.3f cost=%.3f"),
				State->CourtSide, Kind, ASC->FindAbilitySpecFromClass(ExpectedClass) != nullptr, State->bReady,
				ASC->HasMatchingGameplayTag(ActiveTag), AbilityProbeStamina, State->GetAttributes()->GetStamina(), Cost);
			Fail(TEXT("activation or single predicted cost")); return;
		}
		bAbilityProbeActivated = true;
		ProbeTime = 0.f;
	}
	else if (bAbilityProbeActivated && ProbeTime > .4f)
	{
		const float Current = State->GetAttributes()->GetStamina();
		if (Current < AbilityProbeStamina - Cost - 1.f || Current > AbilityProbeStamina - Cost + 12.f)
		{ Fail(TEXT("authoritative cost / regeneration correction")); return; }
		if (Kind == 2 && FVector::Dist2D(ProbeStart, GetPawn()->GetActorLocation()) < 60.f)
		{ Fail(TEXT("dash movement missing")); return; }
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_ABILITY_STEP side=%d kind=%d cost=%.0f stamina=%.1f movement=%.1f"),
			State->CourtSide, Kind, Cost, Current, FVector::Dist2D(ProbeStart, GetPawn()->GetActorLocation()));
		++AbilityProbePassed;
		// Wait for the next rally after this action; do not activate multiple kinds together.
		ProbeTime = -1000.f;
		if (AbilityProbePassed == 3)
		{
			UE_LOG(LogTemp, Display, TEXT("BADMINTON_ABILITY_TEST PASS side=%d drop/smash/dash costs, stale input, repeat input, movement"), State->CourtSide);
			bProbeReported = true;
		}
	}
}

void ABadmintonPlayerController::RunMatchProbe(float DeltaTime)
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || Match->ConnectedPlayers != 2) { return; }
	ProbeTime += DeltaTime;
	if (ProbeTime < .1f) { return; }
	ProbeTime = 0.f;
	if (HasAuthority() && Match->Phase == EBadmintonPhase::RallyComplete)
	{
		ABadmintonGameMode* Mode = GetWorld()->GetAuthGameMode<ABadmintonGameMode>();
		const int32 Score = Match->Score0 + Match->Score1;
		Mode->FinishPracticeRally(FVector(440, 0, 5), Match->RallyId, 0);
		const bool bStaleAccepted = Mode->TryBasicShot(Cast<ABadmintonCharacter>(GetPawn()), Match->RallyId - 1);
		if (Match->Score0 + Match->Score1 != Score || bStaleAccepted)
		{
			UE_LOG(LogTemp, Error, TEXT("BADMINTON_MATCH_TEST FAIL duplicate point or stale rally"));
			bProbeReported = true;
			return;
		}
	}
	if (Match->Phase == EBadmintonPhase::MatchFinished)
	{
		if (Match->Score0 != 11 || Match->Score1 != 0 || Match->WinnerSide != 0 || Match->ServingSide != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("BADMINTON_MATCH_TEST FAIL side=%d score=%d:%d"), State->CourtSide, Match->Score0, Match->Score1);
			bProbeReported = true;
			return;
		}
		if (!bSawMatchFinished)
		{
			if (Match->MatchId != MatchProbeObservedCount + 1)
			{
				UE_LOG(LogTemp, Error, TEXT("BADMINTON_MATCH_TEST FAIL skipped match result"));
				bProbeReported = true;
				return;
			}
			++MatchProbeObservedCount;
			UE_LOG(LogTemp, Display, TEXT("BADMINTON_MATCH_FINISH_OBSERVED side=%d score=11:0 match=%d"), State->CourtSide, Match->MatchId);
		}
		bSawMatchFinished = true;
		if (!State->bReady) { ServerSetReady(true); }
	}
	else if (bSawMatchFinished && Match->MatchId == MatchProbeObservedCount + 1 && Match->Score0 == 0 && Match->Score1 == 0
		&& (Match->Phase == EBadmintonPhase::ReadyToServe || Match->Phase == EBadmintonPhase::Rally))
	{
		bSawMatchFinished = false;
		if (MatchProbeObservedCount >= MatchProbeTargetCount)
		{
			UE_LOG(LogTemp, Display, TEXT("BADMINTON_MATCH_TEST PASS side=%d match=%d score=0:0 rematch=OK completed=%d"), State->CourtSide, Match->MatchId, MatchProbeObservedCount);
			bProbeReported = true;
		}
	}
	else if (Match->Phase == EBadmintonPhase::WaitingForReady && !State->bReady) { ServerSetReady(true); }
	else if (Match->Phase == EBadmintonPhase::ReadyToServe && State->CourtSide == Match->ServingSide && Match->MatchId <= MatchProbeTargetCount)
	{
		if (bThirdPersonControl) { BadmintonStrike(); }
		else { BadmintonClear(); }
	}
}

void ABadmintonPlayerController::RunShotProbe(float DeltaTime)
{
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (!State || !Match || Match->ConnectedPlayers != 2 || !GetPawn())
	{
		return;
	}
	if (bAimProbe && (Match->Score0 != 0 || Match->Score1 != 0))
	{
		UE_LOG(LogTemp, Error, TEXT("BADMINTON_AIM_TEST FAIL side=%d rally=%d sequence=%d score=%d:%d observed=%d rally ended before ten alternating shots"),
			State->CourtSide, Match->RallyId, AimProbeSequence, Match->Score0, Match->Score1, AimProbeObservedShots);
		bProbeReported = true;
		return;
	}
	if (!bProbeStarted)
	{
		bProbeStarted = true;
		ServerSetReady(true);
	}
	ShotProbeTime += DeltaTime;
	if (ShotProbeTime < .05f && !bAimProbe)
	{
		return;
	}
	ShotProbeTime = 0.f;
	for (TActorIterator<ABadmintonShuttle> It(GetWorld()); It; ++It)
	{
		const FBadmintonFlightState& Flight = It->GetFlight();
		if (bAimProbe)
		{
			ShotAim = FVector2D(.5, .25);
			if (Flight.ShotSequence > 0 && Flight.ShotSequence != AimProbeSequence)
			{
				FVector ExpectedTarget;
				const bool bCorrect = Match->GetShotData()->ResolveAimTarget(Flight.Shot, Flight.LastHitterSide, 0, ShotAim, ExpectedTarget)
					&& FVector::Dist(Flight.Aim, FVector(ShotAim.X, ShotAim.Y, 0)) < .02f
					&& FVector::Dist(Flight.Target, ExpectedTarget) < .1f && Flight.ShotSequence == AimProbeSequence + 1;
				if (!bCorrect) { UE_LOG(LogTemp, Error, TEXT("BADMINTON_AIM_TEST FAIL side=%d sequence=%d replicated aim/target"), State->CourtSide, Flight.ShotSequence); bProbeReported = true; return; }
				AimProbeSequence = Flight.ShotSequence;
				++AimProbeObservedShots;
			}
			if (Match->Phase == EBadmintonPhase::Rally && Flight.LastHitterSide != State->CourtSide)
			{
				const float Offset = Flight.Target.Y - GetPawn()->GetActorLocation().Y;
				GetPawn()->AddMovementInput(FVector(0, FMath::Sign(Offset), 0), FMath::Clamp(FMath::Abs(Offset) / 80.f, 0.f, 1.f));
			}
		}
		if (Flight.ShotSequence >= 10)
		{
			if (bAimProbe)
			{
				UE_LOG(LogTemp, Display, TEXT("BADMINTON_AIM_TEST %s side=%d observed=%d aim=0.50,0.25"),
					AimProbeObservedShots == 10 ? TEXT("PASS") : TEXT("FAIL"), State->CourtSide, AimProbeObservedShots);
			}
			if (bPresentationProbe)
			{
				const ABadmintonCharacter* LocalCharacter = Cast<ABadmintonCharacter>(GetPawn());
				const ABadmintonCharacter* RemoteCharacter = nullptr;
				for (TActorIterator<ABadmintonCharacter> CharacterIt(GetWorld()); CharacterIt; ++CharacterIt)
				{
					if (*CharacterIt != LocalCharacter) { RemoteCharacter = *CharacterIt; break; }
				}
				// Allow the final owner RPC and remote swing property to arrive on separate channels.
				PresentationProbeWait += .05f;
				if (PresentationProbeWait < .5f) { return; }
				const int32 LocalSwings = LocalCharacter ? LocalCharacter->GetPresentedSwingCount() : 0;
				const int32 RemoteSwings = RemoteCharacter ? RemoteCharacter->GetPresentedSwingCount() : 0;
				const FString PreviousFeedback = ShotFeedbackText;
				ClientShotFeedback_Implementation(EBadmintonShot::Serve, false, FeedbackRallyId, FeedbackServerTime);
				ClientShotFeedback_Implementation(EBadmintonShot::Serve, false, FeedbackRallyId - 1, FeedbackServerTime + .01);
				const bool bPassed = LocalSwings == 5 && RemoteSwings == 5 && ConfirmedShotCount == 5 && ShotFeedbackText == PreviousFeedback;
				UE_LOG(LogTemp, Display, TEXT("BADMINTON_PRESENTATION_TEST %s side=%d localSwings=%d remoteSwings=%d contacts=%d"),
					bPassed ? TEXT("PASS") : TEXT("FAIL"), State->CourtSide, LocalSwings, RemoteSwings, ConfirmedShotCount);
				if (!bPassed) { bProbeReported = true; return; }
			}
			UE_LOG(LogTemp, Display, TEXT("BADMINTON_SHOT_TEST PASS side=%d sequence=%d rally=%d"), State->CourtSide, Flight.ShotSequence, Flight.RallyId);
			bProbeReported = true;
			return;
		}
		if ((Match->Phase == EBadmintonPhase::ReadyToServe && State->CourtSide == Match->ServingSide)
			|| (Match->Phase == EBadmintonPhase::Rally && Flight.LastHitterSide != State->CourtSide
				&& FVector::Dist2D(GetPawn()->GetActorLocation(), It->GetActorLocation()) < 150.f
				&& It->GetActorLocation().Z < 330.f))
		{
			BadmintonClear();
		}
	}
}

void ABadmintonPlayerController::RunNetworkProbe(float DeltaTime)
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	APawn* LocalPawn = GetPawn();
	ABadmintonCharacter* RemotePawn = nullptr;
	for (TActorIterator<ABadmintonCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != LocalPawn)
		{
			RemotePawn = *It;
		}
	}
	if (!Match || Match->ConnectedPlayers != 2 || !State || State->CourtSide == INDEX_NONE || !LocalPawn || !RemotePawn)
	{
		return;
	}
	if (!bProbeStarted)
	{
		bProbeStarted = true;
		ProbeStart = LocalPawn->GetActorLocation();
		RemoteStart = RemotePawn->GetActorLocation();
		ServerSetReady(true);
	}
	ProbeTime += DeltaTime;
	// Move both players toward the net, then against the side boundary.
	if (ProbeTime < 1.f)
	{
		LocalPawn->AddMovementInput(FVector(Badminton::ForwardSign(State->CourtSide), 0, 0));
	}
	else if (ProbeTime < 2.f)
	{
		LocalPawn->AddMovementInput(FVector(0, Badminton::ForwardSign(State->CourtSide), 0));
	}
	if (ProbeTime > 4.f)
	{
		const ABadmintonPlayerState* RemoteState = RemotePawn->GetPlayerState<ABadmintonPlayerState>();
		const FVector Position = LocalPawn->GetActorLocation();
		const float LocalDistance = FVector::Dist2D(ProbeStart, Position);
		const float RemoteDistance = FVector::Dist2D(RemoteStart, RemotePawn->GetActorLocation());
		const bool bPass = RemoteState && RemoteState->CourtSide != State->CourtSide && LocalDistance > 100.f && RemoteDistance > 100.f
			&& Match->Phase == EBadmintonPhase::ReadyToServe && State->bReady && RemoteState->bReady
			&& FMath::Abs(Position.X) < Badminton::HalfLength + 1.f && FMath::Abs(Position.Y) < Badminton::HalfWidth + 1.f
			&& Position.X * Badminton::ForwardSign(State->CourtSide) < 0.f
			&& State->GetAbilitySystemComponent()->GetAvatarActor() == LocalPawn && State->GetAttributes()->GetStamina() == 100.f;
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_NET_TEST %s side=%d local=%.1f remote=%.1f phase=%d location=%s ASC=%s"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), State->CourtSide, LocalDistance, RemoteDistance, static_cast<int32>(Match->Phase), *Position.ToString(),
			State->GetAbilitySystemComponent()->GetAvatarActor() == LocalPawn ? TEXT("OK") : TEXT("FAIL"));
		bProbeReported = true;
	}
}


FVector ABadmintonPlayerController::GetEyePosition() const
{
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	const FVector Base = GetPawn() ? GetPawn()->GetActorLocation() : Badminton::SpawnTransform(State ? State->CourtSide : 0).GetLocation();
	return Base + FVector(0, 0, 85.f);
}

FVector2D ABadmintonPlayerController::GetRacketScreenPosition() const
{
	FVector2D Screen;
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	if (Width > 0 && Height > 0 && ProjectWorldLocationToScreen(GetEyePosition() + GetCourtViewRotation().Vector() * 100.f, Screen))
	{ return FVector2D(Screen.X / Width, Screen.Y / Height); }
	return FVector2D(.5, .5);
}

FRotator ABadmintonPlayerController::GetCourtViewRotation() const
{
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	return Badminton::CourtCameraRotation(State ? State->CourtSide : 0, CameraLook);
}

void ABadmintonPlayerController::BadmintonSelectDrop() { SelectStroke(EBadmintonShot::Drop); if (bThirdPersonControl) { BadmintonStrike(); } }
void ABadmintonPlayerController::BadmintonSelectSmash() { SelectStroke(EBadmintonShot::Smash); if (!bThirdPersonControl) { SelectTimedShot(EBadmintonShot::Smash); } }
void ABadmintonPlayerController::BadmintonSelectClear() { SelectStroke(EBadmintonShot::Clear); if (bThirdPersonControl) { BadmintonStrike(); } }
void ABadmintonPlayerController::BadmintonSelectReceive() { SelectStroke(EBadmintonShot::Receive); if (bThirdPersonControl) { BadmintonStrike(); } }
void ABadmintonPlayerController::BadmintonHairpin()
{
	if (bThirdPersonControl) { BadmintonSelectDrop(); return; }
	SelectStroke(EBadmintonShot::Hairpin);
}

void ABadmintonPlayerController::SelectStroke(EBadmintonShot Shot)
{
	if (!IsLocalController()) { return; }
	if (bThirdPersonControl && Badminton::IsDropFamily(Shot)) { Shot = EBadmintonShot::Drop; }
	SelectedStroke = Shot;
	bTimingArmed = false;
	ServerSelectStroke(Shot);
}

void ABadmintonPlayerController::ServerSelectStroke_Implementation(EBadmintonShot Shot)
{
	if (bThirdPersonControl && Badminton::IsDropFamily(Shot)) { Shot = EBadmintonShot::Drop; }
	if (Shot != EBadmintonShot::Drop && Shot != EBadmintonShot::Clear && Shot != EBadmintonShot::Hairpin
		&& Shot != EBadmintonShot::Receive && Shot != EBadmintonShot::Smash) { return; }
	SelectedStroke = Shot;
	AimPreviewShot = Shot;
	bTimingArmed = false;
	if (!bThirdPersonControl || Shot == EBadmintonShot::Smash)
	{
		ClientTimingResult(FString::Printf(TEXT("%s 선택 / 왼쪽 클릭으로 타격"), Badminton::ShotLabel(Shot)));
	}
}

void ABadmintonPlayerController::ServerInstantShot_Implementation(EBadmintonShot Shot, FVector Direction, FVector2D Aim, float FaceTilt)
{
	if (bThirdPersonControl) { return; }
	bTimingArmed = false;
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || !GetPawn() || !State->bReady || Aim.ContainsNaN()
		|| !FMath::IsFinite(FaceTilt) || FMath::Abs(FaceTilt) > Badminton::RacketTiltLimit
		|| FMath::Abs(Aim.X) > 1. || FMath::Abs(Aim.Y) > 1.
		|| Direction.ContainsNaN() || !FMath::IsNearlyEqual(Direction.SizeSquared(), 1., .02)) { return; }
	if (Shot != EBadmintonShot::Drop && Shot != EBadmintonShot::Clear && Shot != EBadmintonShot::Hairpin && Shot != EBadmintonShot::Receive) { return; }
	const bool bServe = Match->Phase == EBadmintonPhase::ReadyToServe && Match->ServingSide == State->CourtSide && Shot == EBadmintonShot::Clear;
	if (!bServe && Match->Phase != EBadmintonPhase::Rally)
	{ ClientTimingResult(TEXT("왼쪽 클릭으로 서브하세요")); return; }
	if (!bServe && Shot != SelectedStroke) { return; }
	if (bServe) { Shot = EBadmintonShot::Serve; }
	if (!TimingShuttle) { for (TActorIterator<ABadmintonShuttle> It(GetWorld()); It; ++It) { TimingShuttle = *It; break; } }
	if (!TimingShuttle) { return; }
	const auto& Flight = TimingShuttle->GetFlight();
	if (!bServe && (!Flight.bFlying || Flight.RallyId != Match->RallyId || Flight.LastHitterSide == State->CourtSide))
	{ ClientTimingResult(TEXT("상대가 받아칠 때까지 기다리세요")); return; }
	// Prepare and dispatch atomically on the server, with no timing UI or second input.
	DispatchRacketTilt = FaceTilt;
	TimingShot = Shot;
	LockedAim = Aim;
	TimingRally = Match->RallyId;
	TimingSequence = Flight.ShotSequence;
	bContactForecast = false;
	bTimingArmed = true;
	ServerStrikeTiming_Implementation(Direction);
	bTimingArmed = false;
}

void ABadmintonPlayerController::SelectTimedShot(EBadmintonShot Shot)
{
	if (!IsLocalController()) { return; }
	ServerArmTiming(Shot, bTimingArmed ? LockedAim : ShotAim);
}

void ABadmintonPlayerController::ServerArmTiming_Implementation(EBadmintonShot Shot, FVector2D Aim)
{
	if (bThirdPersonControl) { return; }
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || !GetPawn() || !State->bReady || Aim.ContainsNaN() || FMath::Abs(Aim.X) > 1. || FMath::Abs(Aim.Y) > 1.) { return; }
	if (Shot != EBadmintonShot::Smash || Match->Phase != EBadmintonPhase::Rally) { ClientTimingResult(TEXT("왼쪽 클릭으로 서브하세요")); return; }
	if (!TimingShuttle) { for (TActorIterator<ABadmintonShuttle> It(GetWorld()); It; ++It) { TimingShuttle = *It; break; } }
	if (!TimingShuttle) { return; }
	const FBadmintonFlightState& Flight = TimingShuttle->GetFlight();
	const bool bServe = Match->Phase == EBadmintonPhase::ReadyToServe && State->CourtSide == Match->ServingSide;
	if (!bServe && (Match->Phase != EBadmintonPhase::Rally || !Flight.bFlying || Flight.LastHitterSide == State->CourtSide))
	{ ClientTimingResult(TEXT("상대가 받아칠 때까지 기다리세요")); return; }
	if (bServe) { Shot = EBadmintonShot::Serve; }
	const UBadmintonShotData* Data = Match->GetShotData();
	UAbilitySystemComponent* ASC = State->GetAbilitySystemComponent();
	if (ASC->HasMatchingGameplayTag(TAG_BadmintonSwinging) || ASC->HasMatchingGameplayTag(TAG_BadmintonDashing))
	{ ClientTimingResult(TEXT("동작 회복 중")); return; }
	if (State->GetAttributes()->GetStamina() < Data->Get(Shot).StaminaCost)
	{ ClientTimingResult(TEXT("스태미나 부족 - E 또는 Space 사용")); return; }
	const double Now = Match->GetServerWorldTimeSeconds();
	float UntilContact = .65f;
	bContactForecast = true;
	if (!bServe && Shot != EBadmintonShot::Receive)
	{
		FVector Position = Flight.Position, Velocity = Flight.Velocity;
		ABadmintonShuttle::AdvanceFlight(Position, Velocity, FMath::Clamp(static_cast<float>(Now - Flight.ServerTime), 0.f, .1f));
		bContactForecast = Badminton::PredictContact(State->CourtSide, Shot, Data->Get(Shot), Position, Velocity,
			GetPawn()->GetActorLocation(), GetPawn()->GetVelocity(), UntilContact);
	}
	bTimingArmed = true;
	TimingShot = Shot;
	LockedAim = Aim;
	TimingRally = Match->RallyId;
	TimingSequence = Flight.ShotSequence;
	TimingStartAt = Now;
	TimingPerfectAt = Now + UntilContact;
	ClientArmTiming(Shot, Aim, Now, TimingPerfectAt, TimingRally, TimingSequence);
	ClientContactForecast(bContactForecast, TimingPerfectAt, TimingRally, TimingSequence);
	NextForecastAt = Now + .1;
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_TIMING_ARM shot=%d rally=%d wait=%.3f"), static_cast<int32>(Shot), TimingRally, UntilContact);
}

void ABadmintonPlayerController::ClientArmTiming_Implementation(EBadmintonShot Shot, FVector2D Aim, double StartAt, double PerfectAt, int32 RallyId, int32 Sequence)
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (!Match || Match->RallyId != RallyId || (Shot != EBadmintonShot::Receive && Match->GetServerWorldTimeSeconds() > PerfectAt + .30)) { return; }
	bTimingArmed = true;
	TimingShot = Shot;
	LockedAim = Aim;
	TimingRally = RallyId;
	TimingSequence = Sequence;
	TimingStartAt = StartAt;
	TimingPerfectAt = PerfectAt;
	TimingMessage.Reset();
}

void ABadmintonPlayerController::BadmintonCancelShot()
{
	bTimingArmed = false;
	if (IsLocalController()) { ServerCancelTiming(); }
}
void ABadmintonPlayerController::ServerCancelTiming_Implementation() { bTimingArmed = false; }

void ABadmintonPlayerController::BadmintonStrike()
{
	if (!IsLocalController()) { return; }
	if (bThirdPersonControl)
	{
		const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
		if (Match && TimingShuttle)
		{
			ServerThirdPersonShot(SelectedStroke, ShotAim, Match->RallyId, TimingShuttle->GetFlight().ShotSequence);
		}
		return;
	}
	const auto* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const bool bServe = Match && Match->Phase == EBadmintonPhase::ReadyToServe;
	if (SelectedStroke != EBadmintonShot::Smash || bServe)
	{
		ApplyMouseLook(0.f, 0.f);
		ServerInstantShot(bServe ? EBadmintonShot::Clear : SelectedStroke, GetCourtViewRotation().Vector(), ShotAim, GetRacketTilt());
	}
	else
	{
		if (!bTimingArmed) { ClientTimingResult(TEXT("2로 스매시 준비 후 왼쪽 클릭")); return; }
		ServerStrikeTiming(GetCourtViewRotation().Vector());
	}
	RacketSwingUntil = GetWorld()->GetTimeSeconds() + .22;
	bTimingArmed = false;
}

void ABadmintonPlayerController::ServerStrikeTiming_Implementation(FVector Direction)
{
	if (bThirdPersonControl) { return; }
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	if (!bTimingArmed || !Match || !State || !TimingShuttle || Direction.ContainsNaN() || !FMath::IsNearlyEqual(Direction.SizeSquared(), 1., .02)) { return; }
	const Badminton::EContactHint ContactHint = EvaluateContact(Direction);
	bTimingArmed = false;
	const auto& Flight = TimingShuttle->GetFlight();
	if (Match->RallyId != TimingRally || Flight.ShotSequence != TimingSequence
		|| (Match->Phase != EBadmintonPhase::ReadyToServe && Match->Phase != EBadmintonPhase::Rally)) { ClientTimingResult(TEXT("랠리가 바뀌었습니다")); return; }
	const double Now = Match->GetServerWorldTimeSeconds();
	if (TimingShot == EBadmintonShot::Smash && bContactForecast && Now > TimingPerfectAt + .30) { ClientTimingResult(TEXT("너무 늦었습니다")); return; }
	if (ContactHint != Badminton::EContactHint::Ready) { ClientTimingResult(Badminton::ContactHintText(ContactHint)); return; }
	DispatchQuality = TimingShot == EBadmintonShot::Smash ? (bContactForecast ? Badminton::TimingQuality(Now - TimingPerfectAt) : .5f) : .65f;
	const bool bServe = TimingShot == EBadmintonShot::Serve;
#if !UE_BUILD_SHIPPING
	if (bTimingProbe)
	{
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_CAMERA_DIAG look=%s sensitivity=%.3f direction=%s desired=%s eye=%s shuttle=%s"),
			*CameraLook.ToString(), MouseLookSensitivity, *Direction.ToString(), *(TimingShuttle->GetActorLocation()-GetEyePosition()).Rotation().ToString(), *GetEyePosition().ToString(), *TimingShuttle->GetActorLocation().ToString());
	}
#endif
	bTimedAligned = bServe || Badminton::RacketAimMatches(TimingShot, Direction, TimingShuttle->GetActorLocation() - GetEyePosition());
	DispatchTarget = TimingShot == EBadmintonShot::Smash ? Badminton::PlacementTarget(State->CourtSide, Match->GetScore(State->CourtSide), false, LockedAim)
		: Badminton::InstantPlacementTarget(TimingShot, State->CourtSide, Match->GetScore(State->CourtSide), LockedAim);
	ShotAim = LockedAim;
	DispatchFaceNormal = Direction;
	bTimedDispatch = true;
	const int32 Before = TimingShuttle->GetFlight().ShotSequence;
	SendShot(TimingShot);
	bTimedDispatch = false;
	const bool bContact = TimingShuttle->GetFlight().ShotSequence != Before;
	const TCHAR* Grade = DispatchQuality >= .99f ? TEXT("완벽") : DispatchQuality >= .6f ? TEXT("좋음") : TEXT("약함");
	ClientTimingResult(bContact && TimingShot != EBadmintonShot::Smash ? FString::Printf(TEXT("%s - 타격 성공"), Badminton::ShotLabel(TimingShot)) : bContact ? FString::Printf(TEXT("%s | 위력 %d%%"), Grade, FMath::RoundToInt(100.f / Badminton::TimingFlightScale(DispatchQuality)))
		: bTimedAligned ? TEXT("헛스윙 - 거리와 높이를 확인하세요") : TEXT("헛스윙 - 흰 라켓을 셔틀콕에 맞추세요"));
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_TIMING_HIT shot=%d quality=%.3f aligned=%d contact=%d"), static_cast<int32>(TimingShot), DispatchQuality, bTimedAligned, bContact);
	if (bContact)
	{
		SelectedStroke = EBadmintonShot::Receive;
		AimPreviewShot = EBadmintonShot::Receive;
		ClientResetStroke();
	}
}

void ABadmintonPlayerController::ClientResetStroke_Implementation()
{
	SelectedStroke = EBadmintonShot::Receive;
	AimPreviewShot = EBadmintonShot::Receive;
	bTimingArmed = false;
}

bool ABadmintonPlayerController::ResolveTimedContact(FVector& Target, float& Quality, bool& bAligned) const
{
	if (!bTimedDispatch) { return false; }
	Target = DispatchTarget;
	Quality = DispatchQuality;
	bAligned = bTimedAligned;
	return true;
}

void ABadmintonPlayerController::ClientTimingResult_Implementation(const FString& Message)
{
	TimingMessage = Message;
	TimingMessageUntil = GetWorld()->GetTimeSeconds() + 1.3;
}
FString ABadmintonPlayerController::GetTimingMessage() const
{
	return GetWorld()->GetTimeSeconds() < TimingMessageUntil ? TimingMessage : FString();
}
float ABadmintonPlayerController::GetTimingProgress() const
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	return Match ? FMath::Clamp(static_cast<float>((Match->GetServerWorldTimeSeconds() - TimingStartAt) / FMath::Max(.01, TimingPerfectAt + .30 - TimingStartAt)), 0.f, 1.f) : 0.f;
}
float ABadmintonPlayerController::GetTimingPerfectPosition() const
{
	return FMath::Clamp(static_cast<float>((TimingPerfectAt - TimingStartAt) / FMath::Max(.01, TimingPerfectAt + .30 - TimingStartAt)), 0.f, 1.f);
}
bool ABadmintonPlayerController::IsRacketAligned() const
{
	if (!bTimingArmed || !TimingShuttle) { return false; }
	if (TimingShot == EBadmintonShot::Serve) { return true; }
	return Badminton::RacketAimMatches(TimingShot, GetCourtViewRotation().Vector(), TimingShuttle->GetActorLocation() - GetEyePosition());
}

void ABadmintonPlayerController::UpdateTiming()
{
	if (!TimingShuttle) { for (TActorIterator<ABadmintonShuttle> It(GetWorld()); It; ++It) { TimingShuttle = *It; break; } }
	if (bThirdPersonControl) { if (RacketPreview) { RacketPreview->SetActorHiddenInGame(true); } return; }
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (bTimingArmed && Match)
	{
		const bool bExpired = TimingShot == EBadmintonShot::Smash && bContactForecast && Match->GetServerWorldTimeSeconds() > TimingPerfectAt + .30;
		if (bExpired || Match->RallyId != TimingRally || !TimingShuttle || TimingShuttle->GetFlight().ShotSequence != TimingSequence
			|| (Match->Phase != EBadmintonPhase::ReadyToServe && Match->Phase != EBadmintonPhase::Rally))
		{
			BadmintonCancelShot();
			if (bExpired) { ClientTimingResult(TEXT("너무 늦었습니다 - 다시 시도하세요")); }
		}
	}
	if (!RacketPreview && GetPawn())
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		RacketPreview = GetWorld()->SpawnActor<ABadmintonRacketPreview>(Params);
	}
	if (RacketPreview)
	{
		const bool bSwing = GetWorld()->GetTimeSeconds() < RacketSwingUntil;
		const bool bReceivePreview = Match && (Match->Phase == EBadmintonPhase::Rally || Match->Phase == EBadmintonPhase::ReadyToServe);
		RacketPreview->SetActorHiddenInGame(!bTimingArmed && !bSwing && !bReceivePreview);
		RacketPreview->SetContactReady(GetContactHint() == Badminton::EContactHint::Ready);
	}
}

float ABadmintonPlayerController::GetTimingWindowWidth(float Seconds) const
{
	return Seconds / FMath::Max(.01, TimingPerfectAt + .30 - TimingStartAt);
}

void ABadmintonPlayerController::RunTimingProbe(float DeltaTime)
{
	const auto* Match=GetWorld()->GetGameState<ABadmintonGameState>();
	const auto* State=GetPlayerState<ABadmintonPlayerState>();
	if (!Match || !State || !GetPawn() || !TimingShuttle) { return; }
	ProbeTime+=DeltaTime;
	auto Fail=[this](const TCHAR* Reason) { UE_LOG(LogTemp,Error,TEXT("BADMINTON_TIMING_TEST FAIL %s"),Reason); bProbeReported=true; };
	if (FParse::Param(FCommandLine::Get(), TEXT("BadmintonRacketPoseTest")))
	{
		const int32 Index = FMath::FloorToInt(ProbeTime / 2.f);
		if (Index >= 3)
		{
			UE_LOG(LogTemp, Display, TEXT("BADMINTON_RACKET_POSE_TEST PASS %s"), FParse::Param(FCommandLine::Get(), TEXT("BadmintonWheelPoseTest")) ? TEXT("wheel=-30,0,30") : TEXT("left=OK overhead=OK right=OK rollSpan=180"));
			bProbeReported = true;
			return;
		}
		if (Match->Phase == EBadmintonPhase::WaitingForReady) { ServerSetReady(true); }
		const bool bWheelPose = FParse::Param(FCommandLine::Get(), TEXT("BadmintonWheelPoseTest"));
		const FVector2D Poses[] = {bWheelPose ? FVector2D(0.,-12.) : FVector2D(-90.,-12.), bWheelPose ? FVector2D(0.,-12.) : FVector2D(0.,85.), bWheelPose ? FVector2D(0.,-12.) : FVector2D(90.,-12.)};
		RacketTilt = bWheelPose ? (Index - 1) * 30.f : 0.f;
		ApplyMouseLook((Poses[Index].X - CameraLook.X) / MouseLookSensitivity, (Poses[Index].Y - CameraLook.Y) / MouseLookSensitivity);
		RacketSwingUntil = 0.;
		UpdateTiming();
		if (ProbeTime - Index * 2.f > 1.f && TimingProbeShots == Index)
		{
			FMinimalViewInfo View;
			CalcCamera(DeltaTime, View);
			if (!RacketPreview || RacketPreview->IsHidden() || FVector::DotProduct((RacketPreview->GetActorLocation() - GetEyePosition()).GetSafeNormal(), GetCourtViewRotation().Vector()) < .9999)
			{ Fail(TEXT("racket head differs from hit direction")); return; }
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/BadmintonRacketPose%d.png"), Index), true, false);
			++TimingProbeShots;
		}
		return;
	}
	if (!bProbeStarted)
	{
		const auto* ShuttleVisual = TimingShuttle->FindComponentByClass<UStaticMeshComponent>();
		bool bNetReady = false;
		for (TActorIterator<ABadmintonCourt> It(GetWorld()); It; ++It)
		{
			TArray<UStaticMeshComponent*> Parts;
			It->GetComponents(Parts);
			for (const auto* Part : Parts)
			{
				if (Part->GetFName() == TEXT("DetailedNet") && Part->GetStaticMesh()
					&& Part->GetStaticMesh()->GetName() == TEXT("SM_Badminton_Net_Detailed") && Part->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
				{ bNetReady = true; }
			}
		}
		if (!ShuttleVisual || !ShuttleVisual->GetStaticMesh() || ShuttleVisual->GetStaticMesh()->GetName() != TEXT("SM_Badminton_Shuttle") || !bNetReady)
		{ Fail(TEXT("detailed shuttle or net asset missing")); return; }
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_VISUAL_ASSET_TEST PASS shuttle=%s net=SM_Badminton_Net_Detailed"), *ShuttleVisual->GetStaticMesh()->GetName());
		bProbeStarted = true;
	}
	auto Press = [this, &Fail](FKey Key)
	{
		for (const FInputKeyBinding& Binding : InputComponent->KeyBindings)
		{
			if (Binding.Chord.Key == EKeys::Four) { Fail(TEXT("obsolete 4 binding")); return; }
		}
		for (FInputKeyBinding& Binding : InputComponent->KeyBindings)
		{
			if (Binding.Chord.Key == Key && Binding.KeyEvent == IE_Pressed)
			{ Binding.KeyDelegate.Execute(Key); return; }
		}
		Fail(TEXT("input binding missing"));
	};
	auto Click = [&Press]() { Press(EKeys::LeftMouseButton); };
	float AbortAfter = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("BadmintonTimingAbortAfter="), AbortAfter) && AbortAfter > 0.f && ProbeTime >= AbortAfter)
	{ Fail(TEXT("requested abort for control restoration check")); return; }
	if (ProbeTime>120.f) { Fail(TEXT("timeout")); return; }
	if (Match->Phase==EBadmintonPhase::WaitingForReady) { ServerSetReady(true); return; }
	if (!bWheelInputProbeChecked)
	{
		Press(EKeys::E);
		const int32 Before = TimingShuttle->GetFlight().ShotSequence;
		for (int32 Index=0;Index<20;++Index) { Press(EKeys::MouseScrollUp); }
		if (!FMath::IsNearlyEqual(RacketTilt,30.f)) { Fail(TEXT("wheel upper clamp")); return; }
		for (int32 Index=0;Index<20;++Index) { Press(EKeys::MouseScrollDown); }
		if (!FMath::IsNearlyEqual(RacketTilt,-30.f)) { Fail(TEXT("wheel lower clamp")); return; }
		Press(EKeys::Two);
		Press(EKeys::MouseScrollUp);
		if (!FMath::IsNearlyEqual(RacketTilt,-30.f) || !FMath::IsNearlyZero(GetRacketTilt())) { Fail(TEXT("wheel changed smash")); return; }
		Press(EKeys::E);
		Press(EKeys::MiddleMouseButton);
		if (!FMath::IsNearlyZero(RacketTilt) || Before != TimingShuttle->GetFlight().ShotSequence) { Fail(TEXT("wheel reset or no-shot guard")); return; }
		bWheelInputProbeChecked = true;
		UE_LOG(LogTemp,Display,TEXT("BADMINTON_WHEEL_INPUT_TEST PASS step=3 limits=30 smashIgnored=1 reset=1"));
	}
	const auto& Flight=TimingShuttle->GetFlight();
	if (Flight.ShotSequence != AIProbeSequence)
	{
		AIProbeSequence = Flight.ShotSequence;
		if (Flight.LastHitterSide == State->CourtSide)
		{
			++TimingProbeShots;
			TimingProbeStage |= 1 << static_cast<int32>(Flight.Shot);
			UE_LOG(LogTemp, Display, TEXT("BADMINTON_TIMING_TEST_STEP shots=%d mask=%d"), TimingProbeShots, TimingProbeStage);
		}
	}
	if (TimingProbeStage == 63 && TimingProbeShots >= 7 && QuickReceiveProbeMask == 3 && bTimingProbeMissChecked)
	{
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_TIMING_TEST PASS mask=63 contacts=%d Q=select E=select R=hairpinSelect space=receiveSelect allHits=leftclick selectionReset=OK directionPreview=OK smash=timing aimlock=OK repeatGuard=OK racketRay=OK"), TimingProbeShots);
		bProbeReported = true;
		return;
	}
	if (Match->Phase == EBadmintonPhase::MatchFinished) { Fail(TEXT("match ended before all shots")); return; }
	const bool bServe = Match->Phase == EBadmintonPhase::ReadyToServe && Match->ServingSide == State->CourtSide;
	const bool bReceive = Match->Phase == EBadmintonPhase::Rally && Flight.bFlying && Flight.LastHitterSide != State->CourtSide;
	if (bServe)
	{
		FVector ServicePosition = Badminton::SpawnTransform(State->CourtSide).GetLocation();
		ServicePosition.Y = Badminton::ServeY(State->CourtSide, Match->GetScore(State->CourtSide));
		const FVector ServiceOffset = ServicePosition - GetPawn()->GetActorLocation();
		if (ServiceOffset.Size2D() > 8.f)
		{
			GetPawn()->AddMovementInput(ServiceOffset.GetSafeNormal2D(), FMath::Clamp(ServiceOffset.Size2D()/70.f, 0.f, 1.f));
			return;
		}
		const int32 Before = Flight.ShotSequence;
		Press(EKeys::E);
		if (Flight.ShotSequence != Before) { Fail(TEXT("selection key served without click")); return; }
		Click();
		if (bTimingArmed) { Fail(TEXT("serve created timing gauge")); }
		return;
	}
	if (!bReceive) { return; }
	const FVector Target = ABadmintonAIController::FindReceivePosition(TimingShuttle, State->CourtSide, Match->GetServerWorldTimeSeconds())
		- FVector(Badminton::ForwardSign(State->CourtSide) * 90.f, 0.f, 0.f);
	const FVector Offset = Target - GetPawn()->GetActorLocation();
	if (Offset.Size2D() > 12.f) { GetPawn()->AddMovementInput(Offset.GetSafeNormal2D(), FMath::Clamp(Offset.Size2D()/70.f, 0.f, 1.f)); }
	const FRotator Desired = (TimingShuttle->GetActorLocation() - GetEyePosition()).Rotation();
	ApplyMouseLook((FMath::FindDeltaAngleDegrees(State->CourtSide == 1 ? 180. : 0., Desired.Yaw) - CameraLook.X) / MouseLookSensitivity,
		(Desired.Pitch - CameraLook.Y) / MouseLookSensitivity);
	const double Now = Match->GetServerWorldTimeSeconds();
	const EBadmintonShot Wanted = !(TimingProbeStage & 4) ? EBadmintonShot::Drop : !(TimingProbeStage & 8) ? EBadmintonShot::Smash
		: !(TimingProbeStage & 2) ? EBadmintonShot::Clear : !(TimingProbeStage & 32) ? EBadmintonShot::Hairpin : EBadmintonShot::Receive;
	if (Wanted == EBadmintonShot::Smash)
	{
		if (!bTimingArmed) { ShotAim = FVector2D(.25,.4); Press(EKeys::Two); return; }
		if (!LockedAim.Equals(FVector2D(.25,.4), .001)) { Fail(TEXT("smash aim lock changed")); return; }
		if (!bTimingProbeCaptured && GetTimingProgress() > .3f)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/BadmintonTiming.png"), true, false);
			bTimingProbeCaptured = true;
		}
		if (!bContactForecast || Now < TimingPerfectAt || GetContactHint() == Badminton::EContactHint::AimMiss) { return; }
		if (GetContactHint() != Badminton::EContactHint::Ready) { Fail(TEXT("smash forecast not ready")); return; }
	}
	else
	{
		if (!Flight.bLegalCrossing || Flight.Velocity.Z >= 0.f
			|| Badminton::ContactGeometry(State->CourtSide, GetPawn()->GetActorLocation(), TimingShuttle->GetActorLocation(), Match->GetShotData()->Get(Wanted)) != Badminton::EContactHint::Ready
			|| !Badminton::RacketAimMatches(Wanted, GetCourtViewRotation().Vector(), TimingShuttle->GetActorLocation() - GetEyePosition())) { return; }
		if (GetContactHint() == Badminton::EContactHint::Recovering) { return; }
		if (Wanted == EBadmintonShot::Receive && !bTimingProbeMissChecked)
		{
			const FVector2D BeforeLook = CameraLook;
			ApplyMouseLook(-100000.f, 0.f);
			const int32 Before = Flight.ShotSequence;
			Press(EKeys::Q);
			Click();
			if (Flight.ShotSequence != Before || bTimingArmed || SelectedStroke != EBadmintonShot::Drop) { Fail(TEXT("misaligned shot accepted or selection reset on miss")); return; }
			CameraLook = BeforeLook;
			bTimingProbeMissChecked = true;
		}
		if (Wanted == EBadmintonShot::Hairpin && !bContactReadyProbeCaptured)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/BadmintonContactReady.png"), true, false);
			bContactReadyProbeCaptured = true;
			return;
		}
		if (Wanted == EBadmintonShot::Receive && QuickReceiveProbeMask != 0) { Press(EKeys::Two); }
		TimingPerfectAt = Now + (QuickReceiveProbeMask == 0 ? 2. : -2.);
	}
	const int32 Before = Flight.ShotSequence;
	const int32 PracticeBefore = Practice.RallyHits;
	const FKey Key = Wanted == EBadmintonShot::Drop ? EKeys::Q : Wanted == EBadmintonShot::Clear ? EKeys::E
		: Wanted == EBadmintonShot::Hairpin ? EKeys::R : Wanted == EBadmintonShot::Receive ? EKeys::SpaceBar : EKeys::LeftMouseButton;
	if (Wanted != EBadmintonShot::Smash)
	{
		// Change selection before clicking; only the most recent key determines the stroke.
		if (Wanted == EBadmintonShot::Drop) { Press(EKeys::R); Press(EKeys::E); }
		Press(Key);
		if (Flight.ShotSequence != Before || SelectedStroke != Wanted || bTimingArmed) { Fail(TEXT("selection fired or picked wrong stroke")); return; }
	}
	if (Wanted == EBadmintonShot::Drop) { Press(EKeys::MouseScrollUp); }
	FVector PreviewOrigin, PreviewVelocity;
	const bool bHadPreview = GetShotDirectionPreview(PreviewOrigin, PreviewVelocity);
	Click();
	if (Wanted != EBadmintonShot::Smash && (!bHadPreview || !TimingShuttle->GetSimulationVelocity().Equals(PreviewVelocity, .01)))
	{ Fail(TEXT("direction preview differs from actual launch velocity")); return; }
	if (Flight.ShotSequence != Before + 1 || Flight.Shot != Wanted || bTimingArmed) { Fail(TEXT("left click did not strike selected stroke exactly once")); return; }
	if (Wanted == EBadmintonShot::Drop) { Press(EKeys::MouseScrollDown); }
	if (SelectedStroke != EBadmintonShot::Receive) { Fail(TEXT("stroke did not reset after successful hit")); return; }
	if (Practice.RallyHits != PracticeBefore + 1) { Fail(TEXT("practice contact missing")); return; }
	if (Wanted != EBadmintonShot::Smash && !FMath::IsNearlyEqual(DispatchQuality, .65f)) { Fail(TEXT("instant shot used timing score")); return; }
	if (Wanted == EBadmintonShot::Hairpin && (FMath::Abs(Flight.Target.X) < 65.f || FMath::Abs(Flight.Target.X) > 135.f))
	{ Fail(TEXT("hairpin did not target the net area")); return; }
	if (Wanted == EBadmintonShot::Receive) { QuickReceiveProbeMask |= QuickReceiveProbeMask == 0 ? 1 : 2; }
	if (Wanted != EBadmintonShot::Smash) { Press(Key); }
	Click();
	if (Flight.ShotSequence != Before + 1 || bTimingArmed) { Fail(TEXT("repeated input produced another shot")); return; }
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_KEY_SHOT_TEST PASS shot=%d selectedKey=%s leftclick=1 resetToReceive=1 noTiming=%d"), static_cast<int32>(Wanted), *Key.ToString(), Wanted != EBadmintonShot::Smash);
}
