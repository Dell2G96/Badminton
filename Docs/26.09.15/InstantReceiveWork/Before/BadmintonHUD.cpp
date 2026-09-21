#include "BadmintonHUD.h"

#include "AbilitySystemComponent.h"
#include "BadmintonAttributeSet.h"
#include "BadmintonClearAbility.h"
#include "BadmintonDashAbility.h"
#include "BadmintonGameState.h"
#include "BadmintonOnlineSubsystem.h"
#include "BadmintonPlayerState.h"
#include "BadmintonPlayerController.h"
#include "BadmintonShuttle.h"
#include "BadmintonShotData.h"
#include "BadmintonTiming.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

namespace
{
	const FLinearColor PanelColor(.025f, .04f, .065f, .92f);
	const FLinearColor Muted(.62f, .7f, .78f);
	const FLinearColor Accent(.25f, .95f, .77f);
	const FLinearColor Warning(1.f, .72f, .28f);

	const TCHAR* PointReason(EBadmintonPointReason Reason)
	{
		switch (Reason)
		{
		case EBadmintonPointReason::LandedIn: return TEXT("Shuttle landed in");
		case EBadmintonPointReason::Out: return TEXT("Shuttle out");
		case EBadmintonPointReason::ServiceFault: return TEXT("Service fault");
		case EBadmintonPointReason::InvalidCrossing: return TEXT("Net / crossing fault");
		default: return TEXT("");
		}
	}
}

void ABadmintonHUD::Panel(float X, float Y, float Width, float Height, const FLinearColor& Color)
{
	DrawRect(Color, X * UIScale, Y * UIScale, Width * UIScale, Height * UIScale);
}

void ABadmintonHUD::Label(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float MaxWidth, bool bCentered)
{
	UFont* Font = GEngine->GetSmallFont();
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font, Size);
	if (MaxWidth > 0.f && Width > MaxWidth)
	{
		Size *= MaxWidth / Width;
		Width = MaxWidth;
	}
	DrawText(Text, Color, (X - (bCentered ? Width * .5f : 0.f)) * UIScale, Y * UIScale, Font, Size * UIScale);
}

void ABadmintonHUD::DrawHUD()
{
	Super::DrawHUD();
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = PlayerOwner ? PlayerOwner->GetPlayerState<ABadmintonPlayerState>() : nullptr;
	const ABadmintonPlayerController* Controller = Cast<ABadmintonPlayerController>(PlayerOwner);
	if (!Canvas || !GEngine || !Match || !State || !Controller || Canvas->ClipX <= 0 || Canvas->ClipY <= 0) { return; }
	UIScale = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 720.f);
	const float Width = Canvas->ClipX / UIScale, Height = Canvas->ClipY / UIScale, Center = Width * .5f;
	if (State->CourtSide < 0 || State->CourtSide > 1) { Label(TEXT("JOINING COURT..."), Center, 40, 1.2f, Accent, 300, true); return; }
	const bool bServing = State->CourtSide == Match->ServingSide;
	const bool bLobby = Match->Phase == EBadmintonPhase::WaitingForReady || Match->Phase == EBadmintonPhase::WaitingForPlayers;
	const bool bFinished = Match->Phase == EBadmintonPhase::MatchFinished;
	const bool bPlay = Match->Phase == EBadmintonPhase::Rally || Match->Phase == EBadmintonPhase::ReadyToServe;
	const bool bArmed = Controller->IsTimingArmed();
	const bool bQuickReceive = bArmed && Controller->GetSelectedShot() == EBadmintonShot::Receive;
	const bool bCanHit = Controller->GetContactHint() == Badminton::EContactHint::Ready;
	const FString ContactText = Badminton::ContactHintText(Controller->GetContactHint());
	DrawShuttleTracking(Match);
	DrawAimPreview();
	if (bPlay)
	{
		const FVector Target = Controller->GetPracticeProgress().Target(State->CourtSide);
		for (int32 Index = 0; Index < 32; ++Index)
		{
			const float A = Index * 2.f * PI / 32.f, B = (Index + 1) * 2.f * PI / 32.f;
			FVector2D Start, End;
			if (PlayerOwner->ProjectWorldLocationToScreen(Target + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 85.f, Start)
				&& PlayerOwner->ProjectWorldLocationToScreen(Target + FVector(FMath::Cos(B), FMath::Sin(B), 0) * 85.f, End))
			{ DrawLine(Start.X, Start.Y, End.X, End.Y, Warning, 2.f * UIScale); }
		}
		FVector2D Screen;
		if (PlayerOwner->ProjectWorldLocationToScreen(Target, Screen)) { Label(TEXT("CHALLENGE"), Screen.X / UIScale, Screen.Y / UIScale, .8f, Warning, 110, true); }
	}
	if (bPlay)
	{
		const float CY = Height * .5f;
		const FLinearColor Crosshair = bArmed && bCanHit ? Accent : FLinearColor::White;
		Panel(Center-7, CY-.75f, 4, 1.5f, Crosshair);
		Panel(Center+3, CY-.75f, 4, 1.5f, Crosshair);
		Panel(Center-.75f, CY-7, 1.5f, 4, Crosshair);
		Panel(Center-.75f, CY+3, 1.5f, 4, Crosshair);
	}
	Label(Controller->IsAutomaticTimingTestRunning() ? TEXT("AUTO TEST / CAMERA CONTROLLED") : TEXT("SHUTTLE / FIRST PERSON"), 24, 24, 1.25f, Accent, 300);
	Label(FString::Printf(TEXT("AI %s / FIRST TO 11 / N: LEVEL"), Controller->GetAIDifficulty() == 0 ? TEXT("EASY") : Controller->GetAIDifficulty() == 1 ? TEXT("NORMAL") : TEXT("HARD")), 24, 49, .9f, Muted, 310);
	Label(FString::Printf(TEXT("MOUSE %.2f  /  - + ADJUST"), Controller->GetMouseSensitivity()), 24, 70, .85f, Muted, 300);
	const auto& Practice = Controller->GetPracticeProgress();
	Panel(24, 106, 218, 100, PanelColor);
	Label(TEXT("PRACTICE CHALLENGES"), 35, 116, .85f, Accent, 196);
	Label(FString::Printf(TEXT("RALLY %d / 10   BEST %d"), Practice.RallyHits, Practice.BestRally), 35, 137, .85f, Practice.RallyHits >= 10 ? Accent : Muted, 196);
	Label(FString::Printf(TEXT("L / R TARGETS  %d / 2"), FMath::Min(Practice.Targets, 2)), 35, 159, .85f, Practice.Targets >= 2 ? Accent : Muted, 196);
	Label(FString::Printf(TEXT("DROP + SMASH POINT  %d"), Practice.ComboPoints), 35, 181, .85f, Practice.ComboPoints > 0 ? Accent : Muted, 196);
	Panel(Center - 124, 16, 248, 63, PanelColor);
	Label(TEXT("YOU"), Center - 83, 27, .9f, Accent, 65, true);
	Label(TEXT("AI"), Center + 83, 27, .9f, Muted, 65, true);
	Label(FString::Printf(TEXT("%d : %d"), Match->GetScore(State->CourtSide), Match->GetScore(1-State->CourtSide)), Center, 40, 2.f, FLinearColor::White, 160, true);
	const auto* Attr = State->GetAttributes();
	Panel(Width-221, 20, 197, 49, PanelColor);
	Label(FString::Printf(TEXT("STAMINA %.0f"), Attr->GetStamina()), Width-209, 29, .9f, Accent, 172);
	Panel(Width-209, 53, 172, 4, Muted);
	Panel(Width-209, 53, 172 * FMath::Clamp(Attr->GetStamina()/FMath::Max(1.f, Attr->GetMaxStamina()), 0.f, 1.f), 4, Accent);
	if (bLobby || bFinished)
	{
		Panel(Center-260, Height*.38f, 520, 87, PanelColor);
		const TCHAR* Heading = bLobby ? TEXT("READY TO PLAY?") : Match->WinnerSide == State->CourtSide ? TEXT("YOU WIN") : TEXT("MATCH LOST");
		Label(Heading, Center, Height*.38f+13, 1.7f, Accent, 490, true);
		Label(bLobby ? TEXT("ENTER - START AI MATCH") : TEXT("ENTER - REMATCH"), Center, Height*.38f+50, 1.15f, FLinearColor::White, 490, true);
	}
	else if (Match->Phase == EBadmintonPhase::RallyComplete)
	{
		Label(bServing ? TEXT("YOUR POINT") : TEXT("AI POINT"), Center, 104, 1.4f, Accent, 420, true);
		Label(PointReason(Match->LastPointReason), Center, 132, .95f, Muted, 420, true);
	}
	else if (Match->Phase == EBadmintonPhase::ReadyToServe)
	{
		Label(bServing ? TEXT("YOUR SERVE - SELECT 1 / 2 / 3 / SPACE, THEN LEFT CLICK") : TEXT("AI SERVE - GET READY"), Center, 99, 1.1f, Accent, 730, true);
	}
	// Four shots occupy one compact row, leaving the lower centre clear for the timing gauge.
	const EBadmintonShot Shots[] = {EBadmintonShot::Drop, EBadmintonShot::Smash, EBadmintonShot::Clear, EBadmintonShot::Receive};
	const TCHAR* ShotKeys[] = {TEXT("1"), TEXT("2"), TEXT("3"), TEXT("SPACE")};
	const UBadmintonShotData* Data = Match->GetShotData();
	for (int32 Index=0; Index<4; ++Index)
	{
		const float X=Center-306+Index*156;
		const bool bSelected=bArmed && Controller->GetSelectedShot()==Shots[Index];
		const FLinearColor Color=bSelected ? Accent : Attr->GetStamina()<Data->Get(Shots[Index]).StaminaCost ? Warning : Muted;
		Panel(X, Height-85, 150, 49, PanelColor);
		Panel(X, Height-85, 150, 2, Color);
		Label(FString::Printf(TEXT("%s  %s"),ShotKeys[Index],Badminton::ShotLabel(Shots[Index])),X+9,Height-74,.92f,Color,134);
		Label(Data->Get(Shots[Index]).StaminaCost>0 ? FString::Printf(TEXT("%.0f STAMINA"),Data->Get(Shots[Index]).StaminaCost) : Shots[Index] == EBadmintonShot::Receive ? TEXT("FREE / NO TIMING") : TEXT("FREE"),X+9,Height-53,.75f,Muted,134);
	}
	if (bQuickReceive)
	{
		const float Y = Height - 188;
		Panel(Center-242, Y-27, 484, 107, PanelColor);
		Label(TEXT("QUICK RECEIVE / NO TIMING"), Center, Y-16, 1.1f, Accent, 450, true);
		Label(TEXT("MOUSE LOOK - AIM CENTRE RACKET + LEFT CLICK"), Center, Y+17, 1.05f, FLinearColor::White, 455, true);
		Label(ContactText, Center, Y+52, .88f, bCanHit?Accent:Warning, 455, true);
	}
	else if (bArmed)
	{
		const float Y=Height-188, BarX=Center-220, BarWidth=440;
		Panel(Center-242,Y-27,484,107,PanelColor);
		Label(FString::Printf(TEXT("%s  /  LEFT CLICK TO HIT"),Badminton::ShotLabel(Controller->GetSelectedShot())),Center,Y-16,1.1f,Accent,450,true);
		Panel(BarX,Y+17,BarWidth,20,FLinearColor(.15f,.20f,.27f));
		if (Controller->HasContactForecast())
		{
		const float Perfect=Controller->GetTimingPerfectPosition();
		for (int32 Zone=0; Zone<2; ++Zone)
		{
			const float Half=Controller->GetTimingWindowWidth(Zone==0 ? .125f : .045f);
			const float Left=FMath::Clamp(Perfect-Half,0.f,1.f), Right=FMath::Clamp(Perfect+Half,0.f,1.f);
			Panel(BarX+Left*BarWidth,Y+17,(Right-Left)*BarWidth,20,Zone==0 ? FLinearColor(.23f,.49f,.43f) : Accent);
		}
		const float Cursor=BarX+Controller->GetTimingProgress()*BarWidth;
		Panel(Cursor-2,Y+10,4,34,FLinearColor::White);
		}
		else { Label(TEXT("MOVE INTO REACH / TRACKING CONTACT"), Center, Y+20, .9f, Warning, 420, true); }
		Label(ContactText,Center,Y+52,.88f,bCanHit?Accent:Warning,455,true);
	}
	else if (bPlay)
	{
		Label(TEXT("MOUSE LOOK / AIM   |   1-3 SHOTS   |   SPACE QUICK RECEIVE"),Center,Height-145,1.05f,FLinearColor::White,730,true);
	}
	const FString Challenge = Controller->GetChallengeMessage();
	if (!Challenge.IsEmpty()) { Label(Challenge, Center, 165, 1.15f, Accent, 650, true); }
	const FString Feedback=Controller->GetTimingMessage();
	if (!Feedback.IsEmpty())
	{
		Panel(Center-242,Height-247,484,36,PanelColor);
		Label(Feedback,Center,Height-238,1.05f,Warning,462,true);
	}
	Label(TEXT("WASD MOVE   |   SHIFT DASH   |   RMB CANCEL   |   MMB RECENTRE VIEW"),Center,Height-22,.82f,Muted,800,true);
}

void ABadmintonHUD::DrawShuttleTracking(const ABadmintonGameState* Match)
{
	if (!TrackedShuttle.IsValid())
	{
		for (TActorIterator<ABadmintonShuttle> It(GetWorld()); It; ++It) { TrackedShuttle = *It; break; }
	}
	if (!TrackedShuttle.IsValid()) { return; }
	const FBadmintonFlightState& Flight = TrackedShuttle->GetFlight();
	if (Match->Phase != EBadmintonPhase::Rally || !Flight.bFlying || Flight.RallyId != Match->RallyId)
	{
		ShuttleTrail.Reset();
		return;
	}
	if (TrailRallyId != Flight.RallyId || TrailSequence != Flight.ShotSequence)
	{
		ShuttleTrail.Reset();
		TrailRallyId = Flight.RallyId;
		TrailSequence = Flight.ShotSequence;
		LastTrailSampleTime = 0;
	}
	if (Flight.bHasLanding)
	{
		const FVector Landing = FVector(Flight.Landing) + FVector(0, 0, 3.f);
		const bool bOut = FMath::Abs(Landing.X) > Badminton::HalfLength || FMath::Abs(Landing.Y) > Badminton::HalfWidth;
		const FLinearColor LandingColor = bOut ? FLinearColor(1.f, .3f, .25f) : FLinearColor(.25f, .8f, 1.f);
		for (int32 Index = 0; Index < 48; ++Index)
		{
			const float A = Index * 2.f * PI / 48.f, B = (Index + 1) * 2.f * PI / 48.f;
			FVector2D Start, End;
			if (PlayerOwner->ProjectWorldLocationToScreen(Landing + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 38.f, Start)
				&& PlayerOwner->ProjectWorldLocationToScreen(Landing + FVector(FMath::Cos(B), FMath::Sin(B), 0) * 38.f, End))
			{ DrawLine(Start.X, Start.Y, End.X, End.Y, LandingColor, 3.f * UIScale); }
		}
		FVector2D Screen;
		if (PlayerOwner->ProjectWorldLocationToScreen(Landing, Screen))
		{
			DrawLine(Screen.X - 6 * UIScale, Screen.Y, Screen.X + 6 * UIScale, Screen.Y, LandingColor, 2.f * UIScale);
			DrawLine(Screen.X, Screen.Y - 6 * UIScale, Screen.X, Screen.Y + 6 * UIScale, LandingColor, 2.f * UIScale);
			Panel(Screen.X / UIScale - 65, Screen.Y / UIScale - 29, 130, 22, PanelColor);
			Label(bOut ? TEXT("LANDING / OUT") : TEXT("LANDING"), Screen.X / UIScale, Screen.Y / UIScale - 25, .85f, LandingColor, 124, true);
		}
	}
	const FVector Position = TrackedShuttle->GetActorLocation();
	if (GetWorld()->GetTimeSeconds() - LastTrailSampleTime >= .025)
	{
		ShuttleTrail.Add(Position);
		if (ShuttleTrail.Num() > 6) { ShuttleTrail.RemoveAt(0); }
		LastTrailSampleTime = GetWorld()->GetTimeSeconds();
	}
	// Sample only the displayed path. This is not a landing prediction or a collision shape.
	for (int32 Index = 1; Index < ShuttleTrail.Num(); ++Index)
	{
		FVector2D Start, End;
		if (PlayerOwner->ProjectWorldLocationToScreen(ShuttleTrail[Index - 1], Start)
			&& PlayerOwner->ProjectWorldLocationToScreen(ShuttleTrail[Index], End))
		{
			FLinearColor TrailColor = Warning;
			TrailColor.A = .65f * Index / ShuttleTrail.Num();
			DrawLine(Start.X, Start.Y, End.X, End.Y, TrailColor, 2.f * UIScale);
		}
	}
	FVector2D Screen;
	if (PlayerOwner->ProjectWorldLocationToScreen(Position, Screen))
	{
		const float Radius = 8.f * UIScale;
		const FVector2D Corners[] = {Screen + FVector2D(0, -Radius), Screen + FVector2D(Radius, 0),
			Screen + FVector2D(0, Radius), Screen + FVector2D(-Radius, 0)};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector2D& A = Corners[Index];
			const FVector2D& B = Corners[(Index + 1) % 4];
			DrawLine(A.X, A.Y, B.X, B.Y, Warning, 1.5f * UIScale);
		}
	}
}

void ABadmintonHUD::DrawAimPreview()
{
	const ABadmintonPlayerController* Controller = Cast<ABadmintonPlayerController>(PlayerOwner);
	FVector Target;
	EBadmintonShot Shot;
	if (!Controller || !Controller->GetAimPreview(Target, Shot)) { return; }
	// A ground reference for the shot direction, not an exact landing prediction.
	Target.Z = 4.f;
	for (int32 Index = 0; Index < 24; ++Index)
	{
		const float A = Index * 2.f * PI / 24.f;
		const float B = (Index + 1) * 2.f * PI / 24.f;
		FVector2D Start, End;
		if (PlayerOwner->ProjectWorldLocationToScreen(Target + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 28, Start)
			&& PlayerOwner->ProjectWorldLocationToScreen(Target + FVector(FMath::Cos(B), FMath::Sin(B), 0) * 28, End))
		{
			DrawLine(Start.X, Start.Y, End.X, End.Y, Accent, 1.5f * UIScale);
		}
	}
	FVector2D Screen;
	if (PlayerOwner->ProjectWorldLocationToScreen(Target, Screen))
	{
		DrawLine(Screen.X - 5 * UIScale, Screen.Y, Screen.X + 5 * UIScale, Screen.Y, Accent, UIScale);
		DrawLine(Screen.X, Screen.Y - 5 * UIScale, Screen.X, Screen.Y + 5 * UIScale, Accent, UIScale);
		const TCHAR* Name = Controller->IsTimingArmed() ? TEXT("AIM LOCKED") : TEXT("AIM");
		Panel(Screen.X / UIScale - 53, Screen.Y / UIScale + 10, 106, 22, PanelColor);
		Label(FString::Printf(TEXT("%s"), Name), Screen.X / UIScale, Screen.Y / UIScale + 14, .9f, Accent, 150, true);
	}
}
