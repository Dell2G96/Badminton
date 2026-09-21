#include "BadmintonPlayerController.h"

#include "BadmintonOnlineSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"

bool ABadmintonPlayerController::IsOnlineGameplayBlocked() const
{
	return bOnlineLobbyOpen || FPlatformTime::Seconds() < OnlineInputResumeAt;
}

void ABadmintonPlayerController::SetOnlineLobbyOpen(bool bOpen)
{
	if (!IsLocalController() || bOnlineLobbyOpen == bOpen) { return; }
	bOnlineLobbyOpen = bOpen;
	bShowMouseCursor = bOpen;
	bEnableClickEvents = bOpen;
	SetIgnoreMoveInput(bOpen);
	SetIgnoreLookInput(bOpen);
	if (bOpen)
	{
		BadmintonCancelShot();
		if (GetPawn()) { GetPawn()->ConsumeMovementInputVector(); }
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		// The click that closes the menu must not also strike on the court.
		OnlineInputResumeAt = FPlatformTime::Seconds() + .15;
		SetInputMode(FInputModeGameOnly());
	}
}

void ABadmintonPlayerController::BadmintonToggleOnlineLobby()
{
	SetOnlineLobbyOpen(!bOnlineLobbyOpen);
}

void ABadmintonPlayerController::PreviousOnlineRoom()
{
	if (bOnlineLobbyOpen) { GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->MoveRoomSelection(-1); }
}

void ABadmintonPlayerController::NextOnlineRoom()
{
	if (bOnlineLobbyOpen) { GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->MoveRoomSelection(1); }
}

void ABadmintonPlayerController::PreviousOnlinePage()
{
	if (bOnlineLobbyOpen) { GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->MoveRoomSelection(-6); }
}

void ABadmintonPlayerController::NextOnlinePage()
{
	if (bOnlineLobbyOpen) { GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>()->MoveRoomSelection(6); }
}
