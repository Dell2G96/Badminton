#include "BadmintonPlayerController.h"

#include "BadmintonPlayerState.h"
#include "BadmintonRacketPreview.h"
#include "BadmintonTiming.h"
#include "Camera/CameraTypes.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerInput.h"
#include "Components/InputComponent.h"
#include "TimerManager.h"

UTextureRenderTarget2D* ABadmintonPlayerController::GetThirdPersonTexture() const
{
	return ThirdPersonTexture;
}

void ABadmintonPlayerController::BadmintonToggleDualView()
{
	if (!IsLocalController() || bThirdPersonControl) { return; }
	bDualView = !bDualView;
	UpdateDualView();
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_DUAL_VIEW enabled=%d"), bDualView);
}

void ABadmintonPlayerController::UpdateDualView()
{
	ULocalPlayer* Local = GetLocalPlayer();
	if (!IsLocalController() || !Local || bThirdPersonControl) { return; }
	Local->Origin = FVector2D(bDualView ? .5 : 0., 0.);
	Local->Size = FVector2D(bDualView ? .5 : 1., 1.);
	if (!bDualView) { return; }
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	if (Width < 2 || Height < 2 || !GetPawn()) { return; }
	// Limit the extra view to 720p height, preserving the actual panel aspect ratio.
	const float Scale = FMath::Min(1.f, 720.f / Height);
	const int32 TargetWidth = FMath::Max(2, FMath::RoundToInt(Width * .5f * Scale));
	const int32 TargetHeight = FMath::Max(2, FMath::RoundToInt(Height * Scale));
	if (!ThirdPersonTexture)
	{
		ThirdPersonTexture = NewObject<UTextureRenderTarget2D>(this);
		ThirdPersonTexture->RenderTargetFormat = RTF_RGBA8_SRGB;
		ThirdPersonTexture->ClearColor = FLinearColor::Black;
		ThirdPersonTexture->InitAutoFormat(TargetWidth, TargetHeight);
		ThirdPersonCapture = NewObject<USceneCaptureComponent2D>(this);
		ThirdPersonCapture->TextureTarget = ThirdPersonTexture;
		ThirdPersonCapture->CaptureSource = SCS_FinalColorLDR;
		ThirdPersonCapture->bCaptureEveryFrame = false;
		ThirdPersonCapture->bCaptureOnMovement = false;
		ThirdPersonCapture->bAlwaysPersistRenderingState = true;
		ThirdPersonCapture->PostProcessSettings.bOverride_MotionBlurAmount = true;
		ThirdPersonCapture->PostProcessSettings.MotionBlurAmount = 0.f;
		ThirdPersonCapture->RegisterComponent();
	}
	else if (ThirdPersonTexture->SizeX != TargetWidth || ThirdPersonTexture->SizeY != TargetHeight)
	{
		ThirdPersonTexture->ResizeTarget(TargetWidth, TargetHeight);
	}
	const auto* State = GetPlayerState<ABadmintonPlayerState>();
	const float Sign = Badminton::ForwardSign(State ? State->CourtSide : 0);
	const FVector PawnPosition = GetPawn()->GetActorLocation();
	// Court-aligned camera: mouse aim never spins the positioning reference.
	const FVector Location = PawnPosition + FVector(-Sign * 620.f, 0.f, 530.f);
	const FVector Focus = PawnPosition + FVector(Sign * 390.f, 0.f, 0.f);
	ThirdPersonCapture->SetWorldLocationAndRotation(Location, (Focus - Location).Rotation());
	ThirdPersonCapture->FOVAngle = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(62.f * .5f)) * TargetWidth / TargetHeight));
	ThirdPersonCapture->HiddenActors.Reset();
	if (RacketPreview) { ThirdPersonCapture->HiddenActors.Add(RacketPreview); }
	// Default capture view owner is null, so the owner-hidden body is visible here.
	ThirdPersonCapture->CaptureScene();
}

bool ABadmintonPlayerController::ProjectThirdPerson(const FVector& Position, FVector2D& Screen) const
{
	if (!bDualView || !ThirdPersonCapture || !ThirdPersonTexture) { return false; }
	FMinimalViewInfo View;
	View.Location = ThirdPersonCapture->GetComponentLocation();
	View.Rotation = ThirdPersonCapture->GetComponentRotation();
	View.FOV = ThirdPersonCapture->FOVAngle;
	View.AspectRatio = static_cast<float>(ThirdPersonTexture->SizeX) / ThirdPersonTexture->SizeY;
	FMatrix ViewMatrix, ProjectionMatrix, ViewProjection;
	UGameplayStatics::GetViewProjectionMatrix(View, ViewMatrix, ProjectionMatrix, ViewProjection);
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	return FSceneView::ProjectWorldToScreen(Position, FIntRect(0, 0, Width / 2, Height), ViewProjection, Screen)
		&& Screen.X >= 8 && Screen.X < Width / 2 - 8 && Screen.Y >= 8 && Screen.Y < Height - 8;
}

void ABadmintonPlayerController::RunDualViewProbe(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	if (DualViewProbeStage >= 5 || !GetPawn()) { return; }
	DualViewProbeTime += DeltaTime;
	if (DualViewProbeTime < 4.f + DualViewProbeStage * 3.f) { return; }
	auto Fail = [this](const TCHAR* Reason)
	{
		UE_LOG(LogTemp, Error, TEXT("BADMINTON_DUAL_VIEW_TEST FAIL %s"), Reason);
		DualViewProbeStage = 5;
	};
	const ULocalPlayer* Local = GetLocalPlayer();
	if (!Local || GetGameInstance()->GetNumLocalPlayers() != 1) { Fail(TEXT("unexpected local player count")); return; }
	if (DualViewProbeStage == 0 || DualViewProbeStage == 3)
	{
		int32 Width, Height;
		GetViewportSize(Width, Height);
		FVector2D First, Third;
		const bool bFirstProjected = ProjectWorldLocationToScreen(GetEyePosition() + GetCourtViewRotation().Vector() * 100.f, First);
		const bool bThirdProjected = ProjectThirdPerson(GetPawn()->GetActorLocation(), Third);
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_DUAL_VIEW_DIAGNOSTIC dual=%d texture=%d origin=%s size=%s viewport=%dx%d first=%d:%s third=%d:%s"), bDualView, ThirdPersonTexture != nullptr, *Local->Origin.ToString(), *Local->Size.ToString(), Width, Height, bFirstProjected, *First.ToString(), bThirdProjected, *Third.ToString());
		if (!bDualView || !ThirdPersonTexture || !Local->Origin.Equals(FVector2D(.5, 0.)) || !Local->Size.Equals(FVector2D(.5, 1.))
			|| !bFirstProjected || First.X < Width * .5f || First.X >= Width || !bThirdProjected)
		{ Fail(TEXT("view rectangles / pawn and aim projection")); return; }
		const float Aspect = static_cast<float>(ThirdPersonTexture->SizeX) / ThirdPersonTexture->SizeY;
		if (!FMath::IsNearlyEqual(Aspect, Width * .5f / Height, .01f)) { Fail(TEXT("capture aspect ratio")); return; }
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/BadmintonDualView%d.png"), DualViewProbeStage), true, false);
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_DUAL_VIEW_CHECK stage=%d viewport=%dx%d target=%dx%d aim=%s pawn=%s"), DualViewProbeStage, Width, Height, ThirdPersonTexture->SizeX, ThirdPersonTexture->SizeY, *First.ToString(), *Third.ToString());
	}
	else if (DualViewProbeStage == 1)
	{
		const FRotator Before = GetCourtViewRotation();
		for (FInputKeyBinding& Binding : InputComponent->KeyBindings)
		{
			if (Binding.Chord.Key == EKeys::V && Binding.KeyEvent == IE_Pressed) { Binding.KeyDelegate.Execute(EKeys::V); break; }
		}
		if (bDualView || !Before.Equals(GetCourtViewRotation()) || !Local->Size.Equals(FVector2D(1, 1))) { Fail(TEXT("V key single view / aim preservation")); return; }
	}
	else if (DualViewProbeStage == 2)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/BadmintonSingleView.png"), true, false);
		// Timers tick after PlayerTick; allow a rendered single-view frame before restoring.
		FTimerHandle RestoreHandle;
		GetWorldTimerManager().SetTimer(RestoreHandle, [WeakThis = TWeakObjectPtr<ABadmintonPlayerController>(this)]()
		{
			if (WeakThis.IsValid()) { WeakThis->BadmintonToggleDualView(); }
		}, .2f, false);
	}
	else if (DualViewProbeStage == 4)
	{
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_DUAL_VIEW_TEST PASS onePlayer=1 thirdPerson=left firstPerson=right toggle=OK projection=OK"));
	}
	++DualViewProbeStage;
#endif
}
