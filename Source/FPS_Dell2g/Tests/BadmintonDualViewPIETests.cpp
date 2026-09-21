#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "FPS_Dell2g/Badminton/BadmintonPlayerController.h"
#include "FPS_Dell2g/Badminton/BadmintonGameViewportClient.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Pawn.h"
#include "Misc/Paths.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

namespace
{
	class FVerifyBadmintonDualViewPIE : public IAutomationLatentCommand
	{
	public:
		explicit FVerifyBadmintonDualViewPIE(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt == 0.) { StartedAt = Now; NextCheckAt = Now + 3.; }
			if (Now - StartedAt > 30.) { Test->AddError(TEXT("PIE dual view timed out")); return true; }
			if (Now < NextCheckAt) { return false; }
			UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
			auto* Controller = World ? Cast<ABadmintonPlayerController>(World->GetFirstPlayerController()) : nullptr;
			ULocalPlayer* Local = Controller ? Controller->GetLocalPlayer() : nullptr;
			if (!Controller || !Controller->GetPawn() || !Local || (!Controller->UsesThirdPersonControl() && !Controller->GetThirdPersonTexture())) { return false; }
			Test->TestEqual(TEXT("Actual PIE world"), World->WorldType, EWorldType::PIE);
			Test->TestEqual(TEXT("No extra local player"), World->GetGameInstance()->GetNumLocalPlayers(), 1);
			Test->TestNotNull(TEXT("Configured viewport active in PIE"), Cast<UBadmintonGameViewportClient>(Local->ViewportClient));
			const bool bExpectedDual = !Controller->UsesThirdPersonControl() && Stage != 1;
			Test->TestEqual(TEXT("Camera mode retained between frames"), Controller->IsDualViewEnabled(), bExpectedDual);
			Test->TestTrue(TEXT("Viewport origin retained between frames"), Local->Origin.Equals(FVector2D(bExpectedDual ? .5 : 0., 0.)));
			Test->TestTrue(TEXT("Viewport size retained between frames"), Local->Size.Equals(FVector2D(bExpectedDual ? .5 : 1., 1.)));
			if (bExpectedDual)
			{
				FVector2D Third;
				Test->TestTrue(TEXT("Local pawn projects into third person"), Controller->ProjectThirdPerson(Controller->GetPawn()->GetActorLocation(), Third));
				const FVector2D Aim = Controller->GetRacketScreenPosition();
				Test->TestTrue(TEXT("Aim projects into first-person half"), Aim.X >= .5 && Aim.X < 1. && Aim.Y >= 0. && Aim.Y < 1.);
			}
			if (Controller->UsesThirdPersonControl())
			{
				FVector2D Body;
				Test->TestTrue(TEXT("Local body projects into full third-person viewport"), Controller->ProjectWorldLocationToScreen(Controller->GetPawn()->GetActorLocation(), Body));
				Test->TestNull(TEXT("No extra scene capture in third-person mode"), Controller->GetThirdPersonTexture());
			}
			if (Stage < 2)
			{
				const FRotator Before = Controller->GetCourtViewRotation();
				Controller->BadmintonToggleDualView();
				Test->TestTrue(TEXT("Toggle preserves aim"), Before.Equals(Controller->GetCourtViewRotation()));
			}
			else if (Stage == 2)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/BadmintonPlayModePIE.png"), true, false);
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("BADMINTON_PLAY_MODE_PIE_CHECK completed stages=4 thirdPerson=%d"), Controller->UsesThirdPersonControl());
				return true;
			}
			++Stage;
			NextCheckAt = Now + 1.;
			return false;
		}
	private:
		FAutomationTestBase* Test;
		double StartedAt = 0.;
		double NextCheckAt = 0.;
		int32 Stage = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonDualViewPIETest, "FPS_Dell2g.Badminton.Camera.PlayModePIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FBadmintonDualViewPIETest::RunTest(const FString& Parameters)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		AddError(TEXT("Run this test in an idle editor; existing PIE sessions are preserved."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Badminton/Maps/L_Badminton_Prototype")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyBadmintonDualViewPIE(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	return true;
}
#endif
