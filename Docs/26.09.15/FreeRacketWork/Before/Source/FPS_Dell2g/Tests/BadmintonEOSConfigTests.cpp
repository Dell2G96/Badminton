#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonEOSConfigurationTest, "FPS_Dell2g.EOS.Configuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonEOSConfigurationTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("BadmintonEOSConfigurationTest")))
	{
		AddInfo(TEXT("Use Start-BadmintonEOS.ps1 -ConfigurationTest for isolated synthetic configuration validation."));
		return true;
	}
	FString Platform;
	GConfig->GetString(TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), Platform, GEngineIni);
	TestEqual(TEXT("Per-run EngineIni enables EOS"), Platform, FString(TEXT("EOS")));
	const TCHAR* Section = TEXT("/Script/OnlineSubsystemEOS.EOSSettings");
	TArray<FString> Scopes;
	GConfig->GetArray(Section, TEXT("AuthScopeFlags"), Scopes, GEngineIni);
	TestEqual(TEXT("Exactly one consent scope"), Scopes.Num(), 1);
	TestTrue(TEXT("Basic profile consent only"), Scopes.Contains(TEXT("BasicProfile")));
	TArray<FString> Artifacts;
	GConfig->GetArray(Section, TEXT("Artifacts"), Artifacts, GEngineIni);
	TestEqual(TEXT("Exactly one runtime artifact"), Artifacts.Num(), 1);
	TestTrue(TEXT("Synthetic artifact loaded without displaying credentials"), Artifacts.Num() == 1 && Artifacts[0].Contains(TEXT("SyntheticOnlyNotARealSecret")));
	TArray<FString> Drivers;
	GConfig->GetArray(TEXT("/Script/Engine.Engine"), TEXT("NetDriverDefinitions"), Drivers, GEngineIni);
	TestTrue(TEXT("EOS P2P driver configured"), Drivers.ContainsByPredicate([](const FString& Driver) { return Driver.Contains(TEXT("SocketSubsystemEOS.NetDriverEOSBase")); }));
	bool bP2P = false;
	GConfig->GetBool(TEXT("/Script/SocketSubsystemEOS.NetDriverEOSBase"), TEXT("bIsUsingP2PSockets"), bP2P, GEngineIni);
	TestTrue(TEXT("P2P sockets enabled"), bP2P);
	bool bCheckedPlayerInput = false;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		const UWorld* World = Context.World();
		const APlayerController* Player = World && World->IsGameWorld() ? World->GetFirstPlayerController() : nullptr;
		if (!Player || !Player->PlayerInput) { continue; }
		bCheckedPlayerInput = true;
		TestFalse(TEXT("Online keys cannot execute view-mode debug commands"), Player->PlayerInput->DebugExecBindings.ContainsByPredicate([](const FKeyBind& Binding)
		{
			return Binding.Key == EKeys::F1 || Binding.Key == EKeys::F2 || Binding.Key == EKeys::F3 || Binding.Key == EKeys::F4 || Binding.Key == EKeys::F5;
		}));
	}
	TestTrue(TEXT("Checked actual local player input bindings"), bCheckedPlayerInput);
	return true;
}
#endif
