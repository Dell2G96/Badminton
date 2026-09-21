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
	if (!Canvas || !GEngine || !Match || !State || Canvas->ClipX <= 0.f || Canvas->ClipY <= 0.f)
	{
		return;
	}

	// Logical coordinates keep panels inside both small windows and wide viewports.
	UIScale = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 720.f);
	const float Width = Canvas->ClipX / UIScale;
	const float Height = Canvas->ClipY / UIScale;
	const float Center = Width * .5f;
	const bool bHasSide = State->CourtSide >= 0 && State->CourtSide < Badminton::MaxPlayers;
	if (!bHasSide)
	{
		Panel(Center - 210, 24, 420, 52, PanelColor);
		Label(TEXT("Joining court..."), Center, 40, 1.25f, FLinearColor::White, 390, true);
		return;
	}

	DrawShuttleTracking(Match);
	DrawAimPreview();
	const int32 OpponentSide = 1 - State->CourtSide;
	const ABadmintonPlayerState* Opponent = nullptr;
	for (const APlayerState* Entry : Match->PlayerArray)
	{
		const ABadmintonPlayerState* Candidate = Cast<ABadmintonPlayerState>(Entry);
		if (Candidate && Candidate->CourtSide == OpponentSide)
		{
			Opponent = Candidate;
			break;
		}
	}
	const bool bLobby = Match->Phase == EBadmintonPhase::WaitingForPlayers || Match->Phase == EBadmintonPhase::WaitingForReady;
	const bool bFinished = Match->Phase == EBadmintonPhase::MatchFinished;
	const bool bReadying = bLobby || bFinished;
	const bool bServing = Match->ServingSide == State->CourtSide;
	const bool bServePhase = Match->Phase == EBadmintonPhase::ReadyToServe;
	const bool bRally = Match->Phase == EBadmintonPhase::Rally;

	Label(TEXT("BADMINTON"), 24, 26, 1.6f, Accent);
	Label(FString::Printf(TEXT("SINGLES  /  FIRST TO %d"), Badminton::PointsToWin), 24, 54, 1.f, Muted);
	Label(FString::Printf(TEXT("COURT %d  |  %d / 2 PLAYERS"), State->CourtSide + 1, Match->ConnectedPlayers), 24, 76, 1.f, Muted);

	Panel(Center - 220, 20, 440, 108, PanelColor);
	Panel(Center - 220, 20, 220, 3, Accent);
	Label(TEXT("YOU"), Center - 110, 34, 1.15f, Accent, 180, true);
	const bool bAI = Opponent && Opponent->IsABot();
	Label(bAI ? TEXT("PRACTICE AI") : TEXT("OPPONENT"), Center + 110, 34, 1.15f, Muted, 180, true);
	Label(FString::FromInt(Match->GetScore(State->CourtSide)), Center - 110, 56, 2.7f, FLinearColor::White, 150, true);
	Label(TEXT(":"), Center, 59, 2.f, Muted, 30, true);
	Label(FString::FromInt(Match->GetScore(OpponentSide)), Center + 110, 56, 2.7f, FLinearColor::White, 150, true);
	const TCHAR* LocalState = bReadying ? (State->bReady ? TEXT("READY") : TEXT("NOT READY")) : (bServing ? TEXT("SERVING") : TEXT("RECEIVING"));
	const TCHAR* RemoteState = !Opponent ? TEXT("NOT CONNECTED") : bReadying ? (Opponent->bReady ? TEXT("READY") : TEXT("NOT READY")) : (bServing ? TEXT("RECEIVING") : TEXT("SERVING"));
	Label(LocalState, Center - 110, 103, 1.f, Accent, 190, true);
	Label(RemoteState, Center + 110, 103, 1.f, Muted, 190, true);

	FString Heading;
	FString Hint;
	FLinearColor PhaseColor = Accent;
	switch (Match->Phase)
	{
	case EBadmintonPhase::WaitingForPlayers:
		Heading = TEXT("WAITING FOR OPPONENT");
		Hint = State->bReady ? TEXT("You are ready. Waiting for another player to join.") : TEXT("Press ENTER to ready up while you wait.");
		break;
	case EBadmintonPhase::WaitingForReady:
		Heading = State->bReady ? TEXT("WAITING FOR OPPONENT TO READY") : TEXT("READY TO PLAY?");
		Hint = bAI ? TEXT("Press ENTER to start a match against the AI.") : State->bReady ? TEXT("Press ENTER to cancel ready.") : TEXT("Press ENTER. The match starts when both players are ready.");
		break;
	case EBadmintonPhase::ReadyToServe:
		Heading = bServing ? TEXT("YOUR SERVE") : TEXT("OPPONENT'S SERVE");
		Hint = bServing ? TEXT("LMB to serve. Stay in your starting service half, behind the short line.") : TEXT("Get into position. Return after the shuttle crosses the net.");
		break;
	case EBadmintonPhase::Rally:
		Heading = TEXT("RALLY");
		Hint = TEXT("Move under the shuttle. Timing and reach determine contact.");
		break;
	case EBadmintonPhase::RallyComplete:
		Heading = bServing ? TEXT("YOUR POINT") : TEXT("OPPONENT'S POINT");
		Hint = FString::Printf(TEXT("%s  |  Next serve shortly"), PointReason(Match->LastPointReason));
		PhaseColor = bServing ? Accent : Warning;
		break;
	case EBadmintonPhase::MatchFinished:
		Heading = Match->WinnerSide == State->CourtSide ? TEXT("YOU WIN") : TEXT("MATCH LOST");
		Hint = bAI ? TEXT("Press ENTER to play the AI again.") : State->bReady ? TEXT("Rematch ready. Waiting for opponent. ENTER cancels.") : TEXT("Press ENTER for a rematch. Both players must be ready.");
		PhaseColor = Match->WinnerSide == State->CourtSide ? Accent : Warning;
		break;
	}
	Panel(Center - 290, 140, 580, 64, PanelColor);
	Label(Heading, Center, 149, 1.5f, PhaseColor, 548, true);
	Label(Hint, Center, 178, 1.f, FLinearColor::White, 548, true);

	const UBadmintonAttributeSet* Attributes = State->GetAttributes();
	const float MaxStamina = Attributes ? FMath::Max(0.f, Attributes->GetMaxStamina()) : 0.f;
	const float Stamina = Attributes ? FMath::Clamp(Attributes->GetStamina(), 0.f, MaxStamina) : 0.f;
	const UBadmintonShotData* Data = Match->GetShotData();
	UAbilitySystemComponent* ASC = State->GetAbilitySystemComponent();
	const bool bRecovering = ASC && (ASC->HasMatchingGameplayTag(TAG_BadmintonSwinging) || ASC->HasMatchingGameplayTag(TAG_BadmintonDashing));
	const FLinearColor StaminaColor = Stamina < FMath::Max(Data->Drop.StaminaCost, FMath::Max(Data->Smash.StaminaCost, Data->DashCost)) ? Warning : Accent;
	const float ActionsX = Width - 308;
	const float ActionsY = Height - 230;
	Panel(ActionsX, ActionsY - 76, 284, 64, PanelColor);
	Label(FString::Printf(TEXT("STAMINA  %.0f / %.0f"), Stamina, MaxStamina), ActionsX + 12, ActionsY - 66, 1.f, StaminaColor);
	Panel(ActionsX + 12, ActionsY - 43, 260, 6, FLinearColor(.12f, .18f, .23f));
	Panel(ActionsX + 12, ActionsY - 43, 260 * (MaxStamina > 0.f ? Stamina / MaxStamina : 0.f), 6, StaminaColor);
	Label(bRecovering ? TEXT("RECOVERING") : TEXT("WASD  MOVE"), ActionsX + 12, ActionsY - 31, .9f, Muted, 260);

	const TCHAR* Keys[] = {TEXT("LMB"), TEXT("RMB"), TEXT("SPACE"), TEXT("SHIFT")};
	const TCHAR* Names[] = {bServePhase ? TEXT("SERVE") : TEXT("CLEAR"), TEXT("DROP"), TEXT("SMASH"), TEXT("DASH")};
	const float Costs[] = {bServePhase ? Data->Serve.StaminaCost : Data->Clear.StaminaCost, Data->Drop.StaminaCost, Data->Smash.StaminaCost, Data->DashCost};
	const TSubclassOf<UGameplayAbility> Classes[] = {bServePhase ? UBadmintonServeAbility::StaticClass() : UBadmintonClearAbility::StaticClass(),
		UBadmintonDropAbility::StaticClass(), UBadmintonSmashAbility::StaticClass(), UBadmintonDashAbility::StaticClass()};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
	{
		const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromClass(Classes[Index]) : nullptr;
		// Read the real GAS activation rules; the HUD never approves a hit or spends stamina.
		const bool bAvailable = Spec && Spec->Ability && ASC->AbilityActorInfo.IsValid()
			&& Spec->Ability->CanActivateAbility(Spec->Handle, ASC->AbilityActorInfo.Get());
		const bool bPhaseAllows = bRally || (bServePhase && bServing && Index == 0);
		const TCHAR* Status = bAvailable ? TEXT("READY") : !bPhaseAllows ? TEXT("WAIT") : bRecovering ? TEXT("RECOVERING")
			: Stamina < Costs[Index] ? TEXT("LOW STAMINA") : TEXT("UNAVAILABLE");
		const FLinearColor Color = bAvailable ? Accent : (bPhaseAllows && Stamina < Costs[Index] ? Warning : Muted);
		const float X = ActionsX + (Index % 2) * 148;
		const float Y = ActionsY + (Index / 2) * 96;
		Panel(X, Y, 136, 86, PanelColor);
		Panel(X, Y, 136, 2, Color);
		Label(FString::Printf(TEXT("%s  %s"), Keys[Index], Names[Index]), X + 10, Y + 12, 1.f, FLinearColor::White, 116);
		Label(Costs[Index] > 0.f ? FString::Printf(TEXT("%.0f STAMINA"), Costs[Index]) : TEXT("FREE"), X + 10, Y + 37, .95f, Muted, 116);
		Label(Status, X + 10, Y + 60, .95f, Color, 116);
	}
	Label(bReadying ? TEXT("ENTER  READY / CANCEL") : TEXT("MOUSE  AIM   |   MMB  RESET AIM   |   WASD  MOVE   |   SHIFT  DASH"), Center, Height - 32, 1.f, FLinearColor::White, 760, true);
	if (const ABadmintonPlayerController* Controller = Cast<ABadmintonPlayerController>(PlayerOwner))
	{
		FString Feedback;
		bool bContact = false;
		if (Controller->GetShotFeedback(Feedback, bContact))
		{
			Panel(24, Height - 138, 280, 70, PanelColor);
			Panel(24, Height - 138, 3, 70, bContact ? Accent : Warning);
			Label(Feedback, 38, Height - 126, 1.1f, bContact ? Accent : Warning, 252);
			Label(bContact ? TEXT("Keep moving for the next return.") : TEXT("Check your position, height and timing."),
				38, Height - 98, .95f, FLinearColor::White, 252);
		}
	}

	const UBadmintonOnlineSubsystem* Online = GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>();
	if (Online && Online->IsEnabled() && (bLobby || bFinished))
	{
		Panel(24, 260, 356, 250, PanelColor);
		Label(TEXT("ONLINE ROOMS"), 38, 270, 1.2f, Accent);
		Label(TEXT("F1 LOGIN  |  F2 HOST  |  F3 SEARCH"), 38, 298, 1.f, FLinearColor::White, 328);
		Label(TEXT("F4 JOIN FIRST  |  F5 LEAVE"), 38, 320, 1.f, FLinearColor::White, 328);
		Label(Online->GetStatus(), 38, 346, 1.f, Warning, 328);
		const TArray<FString>& Rooms = Online->GetRoomLabels();
		for (int32 Index = 0; Index < FMath::Min(Rooms.Num(), 5); ++Index)
		{
			Label(Rooms[Index], 38, 371 + Index * 21, 1.f, FLinearColor::White, 328);
		}
		Label(TEXT("Console: BadmintonEOSJoin <index>"), 38, 484, .95f, Muted, 328);
	}
	if (Online && !Online->GetConnectionNotice().IsEmpty())
	{
		Panel(Center - 380, 214, 760, 38, PanelColor);
		Label(Online->GetConnectionNotice(), Center, 226, 1.f, Warning, 728, true);
	}
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
		const TCHAR* Name = Shot == EBadmintonShot::Serve ? TEXT("SERVE") : Shot == EBadmintonShot::Drop ? TEXT("DROP") : Shot == EBadmintonShot::Smash ? TEXT("SMASH") : TEXT("CLEAR");
		Panel(Screen.X / UIScale - 53, Screen.Y / UIScale + 10, 106, 22, PanelColor);
		Label(FString::Printf(TEXT("AIM / %s"), Name), Screen.X / UIScale, Screen.Y / UIScale + 14, .9f, Accent, 150, true);
	}
}
