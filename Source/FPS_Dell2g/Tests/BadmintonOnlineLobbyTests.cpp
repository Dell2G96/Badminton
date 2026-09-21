#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "FPS_Dell2g/Badminton/BadmintonOnlineSubsystem.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonOnlineSelectionTest, "FPS_Dell2g.Badminton.Online.RoomSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonOnlineSelectionTest::RunTest(const FString& Parameters)
{
	auto* Instance = NewObject<UGameInstance>();
	auto* Online = NewObject<UBadmintonOnlineSubsystem>(Instance);
	Online->MoveRoomSelection(1);
	TestEqual(TEXT("Empty list has no selected room"), Online->GetSelectedRoom(), INDEX_NONE);
	// Synthetic search results exercise selection only; no service or credential is used.
	Online->Rooms.SetNum(20);
	Online->SelectRoom(0);
	Online->MoveRoomSelection(6);
	TestEqual(TEXT("Second page selection"), Online->GetSelectedRoom(), 6);
	Online->SelectRoom(-1);
	Online->SelectRoom(20);
	TestEqual(TEXT("Invalid indices preserve selection"), Online->GetSelectedRoom(), 6);
	Online->bBusy = true;
	Online->SelectRoom(2);
	Online->MoveRoomSelection(1);
	TestEqual(TEXT("Pending request cannot change selection"), Online->GetSelectedRoom(), 6);
	Online->bBusy = false;
	Online->MoveRoomSelection(MAX_int32);
	TestEqual(TEXT("Large positive offset clamps to final room"), Online->GetSelectedRoom(), 19);
	Online->MoveRoomSelection(MIN_int32);
	TestEqual(TEXT("Large negative offset clamps to first room"), Online->GetSelectedRoom(), 0);
	Online->JoinRoom(0);
	TestFalse(TEXT("Unavailable service does not start a request"), Online->IsBusy());
	TestTrue(TEXT("Unavailable service exposes readable status"), Online->GetStatus().Contains(TEXT("사용할 수 없습니다")));
	return true;
}
#endif

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "FPS_Dell2g/Badminton/BadmintonHUD.h"
#include "FPS_Dell2g/Badminton/BadmintonPlayerController.h"
#include "FPS_Dell2g/Badminton/BadmintonPlayerState.h"
#include "Components/InputComponent.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/Paths.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

namespace
{
	class FVerifyBadmintonOnlineLobbyPIE : public IAutomationLatentCommand
	{
	public:
		explicit FVerifyBadmintonOnlineLobbyPIE(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt == 0.) { StartedAt = Now; NextAt = Now + 3.; }
			if (Now - StartedAt > 35.) { Test->AddError(TEXT("Online lobby PIE timed out")); return true; }
			if (Now < NextAt) { return false; }
			UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
			auto* PC = World ? Cast<ABadmintonPlayerController>(World->GetFirstPlayerController()) : nullptr;
			auto* HUD = PC ? Cast<ABadmintonHUD>(PC->GetHUD()) : nullptr;
			auto* State = PC ? PC->GetPlayerState<ABadmintonPlayerState>() : nullptr;
			if (!PC || !HUD || !State || !PC->GetPawn()) { return false; }
			auto* Online = World->GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>();
			if (Stage == 0)
			{
				Test->TestFalse(TEXT("Offline practice starts with menu closed"), PC->IsOnlineLobbyOpen());
				bool bBound = false;
				for (FInputKeyBinding& Binding : PC->InputComponent->KeyBindings)
				{
					if (Binding.Chord.Key == EKeys::F6 && Binding.KeyEvent == IE_Pressed)
					{
						Binding.KeyDelegate.Execute(EKeys::F6);
						bBound = true;
						break;
					}
				}
				Test->TestTrue(TEXT("F6 binding opens menu"), bBound && PC->IsOnlineLobbyOpen());
				PC->SetOnlineLobbyOpen(true);
				Test->TestTrue(TEXT("Menu enables cursor and ignores movement"), PC->bShowMouseCursor && PC->IsMoveInputIgnored());
				const bool bReady = State->bReady;
				PC->BadmintonReady();
				Test->TestEqual(TEXT("Enter in menu never readies the player"), State->bReady, bReady);
				PC->GetPawn()->ConsumeMovementInputVector();
				PC->GetPawn()->AddMovementInput(FVector::ForwardVector);
				Test->TestTrue(TEXT("Menu blocks pawn movement"), PC->GetPawn()->GetPendingMovementInputVector().IsNearlyZero());
				Test->TestFalse(TEXT("Offline join cannot start service request"), Online->IsBusy());
			}
			else if (Stage == 1)
			{
				Test->TestNotNull(TEXT("Close button has a rendered hitbox"), HUD->GetHitBoxWithName(TEXT("OnlineClose")));
				Test->TestNull(TEXT("Offline login button is disabled"), HUD->GetHitBoxWithName(TEXT("OnlineLogin")));
				Test->TestNull(TEXT("Empty room list disables join"), HUD->GetHitBoxWithName(TEXT("OnlineJoin")));
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/BadmintonOnlineLobbyPIE.png"), true, false);
			}
			else if (Stage == 2)
			{
				HUD->NotifyHitBoxClick(TEXT("OnlineClose"));
				Test->TestFalse(TEXT("Close button hides menu"), PC->IsOnlineLobbyOpen());
				Test->TestFalse(TEXT("Idempotent open restores movement without a stacked ignore"), PC->IsMoveInputIgnored());
				Test->TestFalse(TEXT("Close restores look and cursor"), PC->IsLookInputIgnored() || PC->bShowMouseCursor);
				const bool bReady = State->bReady;
				PC->BadmintonReady();
				Test->TestEqual(TEXT("Closing input cannot also ready the player"), State->bReady, bReady);
				PC->BadmintonToggleOnlineLobby();
				PC->BadmintonToggleOnlineLobby();
				Test->TestFalse(TEXT("Repeated toggle restores input"), PC->IsMoveInputIgnored());
			}
			else
			{
				Test->TestNull(TEXT("Closed menu clears stale click targets"), HUD->GetHitBoxWithName(TEXT("OnlineClose")));
				UE_LOG(LogTemp, Display, TEXT("BADMINTON_ONLINE_LOBBY_PIE PASS offline=1 input=1 hitboxes=1"));
				return true;
			}
			++Stage;
			NextAt = Now + 1.;
			return false;
		}
	private:
		FAutomationTestBase* Test;
		double StartedAt = 0., NextAt = 0.;
		int32 Stage = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonOnlineLobbyPIETest, "FPS_Dell2g.Badminton.Online.LobbyPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FBadmintonOnlineLobbyPIETest::RunTest(const FString& Parameters)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		AddError(TEXT("Run in an idle editor; existing PIE sessions are preserved."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Badminton/Maps/L_Badminton_Prototype")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyBadmintonOnlineLobbyPIE(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	return true;
}
#endif
