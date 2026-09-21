#include "BadmintonHUD.h"

#include "BadmintonAttributeSet.h"
#include "BadmintonGameState.h"
#include "BadmintonPlayerController.h"
#include "BadmintonPlayerState.h"
#include "BadmintonShuttle.h"
#include "BadmintonTiming.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/ScopeExit.h"

bool ABadmintonHUD::ProjectFirstPerson(const FVector& Position, FVector2D& Screen) const
{
	if (!PlayerOwner || !PlayerOwner->ProjectWorldLocationToScreen(Position, Screen)) { return false; }
	const auto* Controller = Cast<ABadmintonPlayerController>(PlayerOwner);
	int32 Width = 0, Height = 0;
	PlayerOwner->GetViewportSize(Width, Height);
	const float Left = Controller && Controller->IsDualViewEnabled() ? Width * .5f : 0.f;
	return Screen.X >= Left + 8.f && Screen.X < Width - 8.f && Screen.Y >= 8.f && Screen.Y < Height - 8.f;
}

void ABadmintonHUD::DrawDualViewHUD(const ABadmintonGameState* Match)
{
	const auto* Controller = Cast<ABadmintonPlayerController>(PlayerOwner);
	const auto* State = PlayerOwner->GetPlayerState<ABadmintonPlayerState>();
	int32 PixelWidth = 0, PixelHeight = 0;
	PlayerOwner->GetViewportSize(PixelWidth, PixelHeight);
	if (PixelWidth < 2 || PixelHeight < 2) { return; }
	// Engine gives HUD the right-hand view's canvas; temporarily draw in window coordinates.
	TGuardValue<float> ClipX(Canvas->ClipX, static_cast<float>(PixelWidth));
	TGuardValue<float> ClipY(Canvas->ClipY, static_cast<float>(PixelHeight));
	Canvas->Canvas->PushAbsoluteTransform(FMatrix::Identity);
	ON_SCOPE_EXIT { Canvas->Canvas->PopTransform(); };
	UIScale = FMath::Min(PixelWidth / 1280.f, PixelHeight / 720.f);
	const float Width = PixelWidth / UIScale, Height = PixelHeight / UIScale;
	const float Half = Width * .5f, LeftCenter = Width * .25f, RightCenter = Width * .75f;
	const FLinearColor Dark(.025f, .04f, .065f, .92f), Accent(.25f, .95f, .77f), Muted(.7f, .77f, .83f), Warning(1.f, .72f, .28f);
	Panel(0, 0, Half, Height, FLinearColor::Black);
	if (UTextureRenderTarget2D* Texture = Controller->GetThirdPersonTexture())
	{
		DrawTexture(Texture, 0, 0, PixelWidth * .5f, PixelHeight, 0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);
	}
	if (State->CourtSide < 0 || State->CourtSide > 1)
	{
		Label(TEXT("코트에 입장하는 중..."), LeftCenter, 40, 1.2f, Accent, Half - 40, true);
		return;
	}
	DrawShuttleTracking(Match);
	DrawAimPreview();
	// The third-person view supplies positioning context and the same server landing marker.
	if (TrackedShuttle.IsValid())
	{
		FVector2D Screen;
		if (Controller->ProjectThirdPerson(TrackedShuttle->GetActorLocation(), Screen))
		{
			const float X = Screen.X / UIScale, Y = Screen.Y / UIScale;
			Panel(X - 4, Y - 4, 8, 8, Warning);
		}
		const auto& Flight = TrackedShuttle->GetFlight();
		if (Match->Phase == EBadmintonPhase::Rally && Flight.bFlying && Flight.bHasLanding && Flight.RallyId == Match->RallyId)
		{
#if !UE_BUILD_SHIPPING
			if (!bDualViewTestReported && Controller->IsAutomaticTimingTestRunning())
			{
				UE_LOG(LogTemp, Display, TEXT("BADMINTON_DUAL_VIEW_UI_TEST PASS shuttle=1 landing=1"));
				bDualViewTestReported = true;
			}
#endif
			for (int32 Index = 0; Index < 32; ++Index)
			{
				const float A = Index * 2.f * PI / 32.f, B = (Index + 1) * 2.f * PI / 32.f;
				FVector2D Start, End;
				const FVector Landing = FVector(Flight.Landing) + FVector(0, 0, 4);
				if (Controller->ProjectThirdPerson(Landing + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 38.f, Start)
					&& Controller->ProjectThirdPerson(Landing + FVector(FMath::Cos(B), FMath::Sin(B), 0) * 38.f, End))
				{ DrawLine(Start.X, Start.Y, End.X, End.Y, Warning, 2.f * UIScale); }
			}
		}
	}
	Panel(Half - 2, 0, 4, Height, Accent);
	Panel(0, 0, Width, 78, Dark);
	Label(TEXT("3인칭 / 위치 확인"), 20, 15, 1.05f, Accent, Half - 160);
	Label(TEXT("1인칭 / 조준과 타격"), Half + 90, 15, 1.05f, Accent, Half - 250);
	const bool bReady = Controller->GetContactHint() == Badminton::EContactHint::Ready;
	Label(TEXT("WASD 이동 / Shift 대시"), 20, 48, .85f, Muted, Half - 160);
	Label(TEXT("마우스 라켓 / 휠 각도 / 가운데 클릭 초기화"), Half + 90, 48, .8f, Muted, Half - 250);
	Panel(Half - 72, 8, 144, 60, Dark);
	Label(TEXT("나 : 상대"), Half, 15, .85f, Muted, 132, true);
	Label(FString::Printf(TEXT("%d : %d"), Match->GetScore(State->CourtSide), Match->GetScore(1 - State->CourtSide)), Half, 34, 1.6f, FLinearColor::White, 132, true);
	Label(FString::Printf(TEXT("스태미나 %.0f"), State->GetAttributes()->GetStamina()), Width - 82, 15, .9f, Accent, 125, true);
	// Keep the right margin inside the window, including ultrawide and resized windows.
	Panel(Width - 144, 46, 124, 4, Muted);
	Panel(Width - 144, 46, 124 * FMath::Clamp(State->GetAttributes()->GetStamina() / FMath::Max(1.f, State->GetAttributes()->GetMaxStamina()), 0.f, 1.f), 4, Accent);
	const bool bPlay = Match->Phase == EBadmintonPhase::Rally || Match->Phase == EBadmintonPhase::ReadyToServe;
	if (bPlay)
	{
		const FVector Target = Controller->GetPracticeProgress().Target(State->CourtSide);
		for (int32 Index = 0; Index < 32; ++Index)
		{
			const float A = Index * 2.f * PI / 32.f, B = (Index + 1) * 2.f * PI / 32.f;
			FVector2D Start, End;
			if (Controller->ProjectThirdPerson(Target + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 85.f, Start)
				&& Controller->ProjectThirdPerson(Target + FVector(FMath::Cos(B), FMath::Sin(B), 0) * 85.f, End))
			{ DrawLine(Start.X, Start.Y, End.X, End.Y, Accent, 2.f * UIScale); }
		}
		FVector Origin, Velocity;
		if (Controller->GetShotDirectionPreview(Origin, Velocity))
		{
			const float Sign = Badminton::ForwardSign(State->CourtSide);
			const FVector2D Direction = FVector2D(Velocity.Y * Sign, -Velocity.X * Sign).GetSafeNormal();
			const FVector2D Start(Half - 80, 188), End = Start + Direction * 40.f, Cross(-Direction.Y, Direction.X);
			Panel(Half - 144, 96, 128, 112, Dark);
			Label(TEXT("타구 방향"), Half - 80, 104, .8f, Accent, 112, true);
			auto Line = [&](FVector2D A, FVector2D B) { DrawLine(A.X * UIScale, A.Y * UIScale, B.X * UIScale, B.Y * UIScale, Accent, 2.f * UIScale); };
			Line(Start, End); Line(End, End - Direction * 9.f + Cross * 5.f); Line(End, End - Direction * 9.f - Cross * 5.f);
#if !UE_BUILD_SHIPPING
			if (Controller->IsAutomaticTimingTestRunning() && !bDirectionPreviewTestReported)
			{
				UE_LOG(LogTemp, Display, TEXT("BADMINTON_DIRECTION_UI_TEST PASS velocity=%s angle=%.1f"), *Velocity.ToString(), Controller->GetRacketTilt());
				bDirectionPreviewTestReported = true;
			}
#endif
		}
		const FVector2D Racket = Controller->GetRacketScreenPosition();
		const float X = Width * Racket.X, Y = Height * Racket.Y;
		if (X > Half + 10 && X < Width - 10 && Y > 88 && Y < Height - 10)
		{
			const FLinearColor Color = bReady ? Accent : FLinearColor::White;
			Panel(X - 7, Y - 1, 4, 2, Color); Panel(X + 3, Y - 1, 4, 2, Color);
			Panel(X - 1, Y - 7, 2, 4, Color); Panel(X - 1, Y + 3, 2, 4, Color);
		}
	}
	Panel(0, Height - 98, Width, 98, Dark);
	Label(TEXT("Q 드롭 | 2 스매시 | E 클리어 | R 헤어핀 | Space 리시브"), Half, Height - 27, .9f, Muted, Width - 40, true);
	Label(FString::Printf(TEXT("상대 난이도 %d / N 변경 | 감도 %.2f / - + | V 단일 화면"), Controller->GetAIDifficulty() + 1, Controller->GetMouseSensitivity()), LeftCenter, Height - 51, .85f, Muted, Half - 36, true);
	Label(FString::Printf(TEXT("랠리 %d / 최고 %d | 목표 %d/2 | 라켓 면 %+.0f도"), Controller->GetPracticeProgress().RallyHits, Controller->GetPracticeProgress().BestRally, FMath::Min(2, Controller->GetPracticeProgress().Targets), Controller->GetRacketTilt()), LeftCenter, Height - 79, .9f, Accent, Half - 36, true);
	Label(FString::Printf(TEXT("%s / 왼쪽 클릭 타격 / 오른쪽 클릭 취소"), Badminton::ShotLabel(Controller->GetSelectedShot())), RightCenter, Height - 81, 1.f, Accent, Half - 36, true);
	Label(Badminton::ContactHintText(Controller->GetContactHint()), RightCenter, Height - 53, .85f, bReady ? Accent : Warning, Half - 36, true);
	if (Controller->IsTimingArmed())
	{
		const float BarWidth = FMath::Min(400.f, Half - 60), X = RightCenter - BarWidth * .5f, Y = Height - 127;
		Panel(X, Y, BarWidth, 15, Dark);
		if (Controller->HasContactForecast())
		{
			const float Perfect = Controller->GetTimingPerfectPosition();
			for (const float Window : {.125f, .045f})
			{
				const float Radius = Controller->GetTimingWindowWidth(Window);
				const float A = FMath::Clamp(Perfect - Radius, 0.f, 1.f), B = FMath::Clamp(Perfect + Radius, 0.f, 1.f);
				Panel(X + A * BarWidth, Y, (B - A) * BarWidth, 15, Window > .1f ? Muted : Accent);
			}
			Panel(X + Controller->GetTimingProgress() * BarWidth - 2, Y - 3, 4, 21, FLinearColor::White);
		}
	}
	FString Status;
	if (Match->Phase == EBadmintonPhase::WaitingForPlayers || Match->Phase == EBadmintonPhase::WaitingForReady) { Status = TEXT("Enter / 경기 준비"); }
	else if (Match->Phase == EBadmintonPhase::MatchFinished) { Status = Match->WinnerSide == State->CourtSide ? TEXT("승리! / Enter 재경기") : TEXT("패배 / Enter 재경기"); }
	else if (Match->Phase == EBadmintonPhase::ReadyToServe) { Status = Match->ServingSide == State->CourtSide ? TEXT("내 서브 / 왼쪽 클릭") : TEXT("상대 서브 / 준비하세요"); }
	else if (Match->Phase == EBadmintonPhase::RallyComplete) { Status = Match->ServingSide == State->CourtSide ? TEXT("내 득점") : TEXT("상대 득점"); }
	if (!Status.IsEmpty())
	{
		Panel(LeftCenter - 190, 95, 380, 35, Dark);
		Label(Status, LeftCenter, 104, 1.1f, Accent, 356, true);
	}
	const FString Feedback = Controller->GetTimingMessage();
	if (!Feedback.IsEmpty()) { Label(Feedback, RightCenter, 102, .95f, Warning, Half - 40, true); }
	const FString Challenge = Controller->GetChallengeMessage();
	if (!Challenge.IsEmpty()) { Label(Challenge, LeftCenter, 144, .95f, Accent, Half - 40, true); }
}
