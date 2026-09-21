#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "FPS_Dell2g/Badminton/BadmintonShuttle.h"
#include "FPS_Dell2g/Badminton/BadmintonCourt.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonLandingTest,
	"FPS_Dell2g.Badminton.Flight.LandingMatchesActualCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonLandingTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->SetGameState(World->SpawnActor<AGameStateBase>());
	World->SpawnActor<ABadmintonCourt>();
	ABadmintonShuttle* Shuttle = World->SpawnActor<ABadmintonShuttle>();
	int32 Rally = 0;
	// Actual actor sweeps provide the independent reference, including component move pullback.
	for (const int32 FPS : {30, 60, 144})
	{
		for (const int32 Side : {0, 1})
		{
			const float Sign = Badminton::ForwardSign(Side);
			for (const int32 Case : {0, 1, 2, 3, 4})
			{
				const FVector Start(-400 * Sign, 50, Case == 3 ? 100 : 250);
				const FVector Target((Case == 1 ? 200 : 500) * Sign, Case == 4 ? 300 : -80, Case == 2 ? 160 : 5);
				const float Duration = Case == 3 ? .55f : (Case == 1 ? 1.5f : 2.f);
				Shuttle->Launch(Start, Target, Side, ++Rally, Duration);
				TestTrue(TEXT("Landing exists for clear, drop, elevated aim, net touch and out"), Shuttle->GetFlight().bHasLanding);
				const FVector Predicted = Shuttle->GetFlight().Landing;
				TestTrue(TEXT("Marker lies on the floor"), FMath::Abs(Predicted.Z) < .1f);
				for (int32 Frame = 0; Frame < FPS * 8 + 1 && Shuttle->GetFlight().bFlying; ++Frame)
				{ Shuttle->Tick(1.f / FPS); }
				const float Error = FVector::Dist2D(Predicted, Shuttle->GetActorLocation());
				TestTrue(FString::Printf(TEXT("Landing within 2cm: fps=%d side=%d case=%d error=%.3f"), FPS, Side, Case, Error), Error < 2.f);
				TestFalse(TEXT("Landing marker clears at rally end"), Shuttle->GetFlight().bHasLanding);
				if (Case == 2) { TestTrue(TEXT("Elevated aiming point is not incorrectly reused as landing"), FVector::Dist2D(Predicted, Target) > 10.f); }
				if (Case == 3) { TestTrue(TEXT("Net-hit landing stays on hitter side"), Predicted.X * Sign < 0.f); }
				Shuttle->ResetForServe(Start, Rally);
				TestFalse(TEXT("No stale landing marker at serve reset"), Shuttle->GetFlight().bHasLanding);
			}
		}
	}
	FVector Landing;
	TestFalse(TEXT("No invented landing beyond finite floor"), ABadmintonShuttle::PredictLanding(World, FVector(900, 0, 200), FVector(200, 0, 0), 0, true, Landing));
	World->DestroyWorld(false);
	return true;
}
#endif
