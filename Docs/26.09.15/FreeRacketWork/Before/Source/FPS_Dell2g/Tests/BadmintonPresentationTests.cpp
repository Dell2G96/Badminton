#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "FPS_Dell2g/Badminton/BadmintonPresentation.h"
#include "FPS_Dell2g/Badminton/BadmintonShuttle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonPresentationReceiptTest,
	"FPS_Dell2g.Badminton.Presentation.BehindClockStillAdvances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonPresentationReceiptTest::RunTest(const FString& Parameters)
{
	const FVector Start(-390, 70, 160);
	const FVector InitialVelocity = ABadmintonShuttle::SolveVelocity(Start, FVector(440, -110, 160), 1.7f);
	for (const int32 FPS : {30, 60, 120})
	{
		const double Elapsed = 1. / FPS;
		const auto Age = Badminton::GetPresentationAge(99.9 + Elapsed, 100., 20. + Elapsed, 20.);
		TestTrue(TEXT("Keep the negative raw clock observation"), Age.Raw < 0.);
		TestTrue(TEXT("Elapsed receipt time advances the target"), FMath::IsNearlyEqual(Age.Seconds, float(Elapsed), 1.e-6f));
		FVector Position = Start;
		FVector Velocity = InitialVelocity;
		ABadmintonShuttle::AdvanceFlight(Position, Velocity, Age.Seconds);
		TestTrue(TEXT("The flight target moves despite a behind server estimate"), Position.X > Start.X);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonPresentationBoundsTest,
	"FPS_Dell2g.Badminton.Presentation.LossPauseAndFreshSnapshotBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonPresentationBoundsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("No extrapolation at receipt with a behind clock"), Badminton::GetPresentationAge(99., 100., 20., 20.).Seconds, 0.f);
	TestEqual(TEXT("Packet silence stays capped at 150ms"), Badminton::GetPresentationAge(99., 100., 21., 20.).Seconds, .15f);
	TestEqual(TEXT("Clock jump ahead stays capped"), Badminton::GetPresentationAge(110., 100., 20.01, 20.).Seconds, .15f);
	TestEqual(TEXT("Fresh snapshot starts a new receipt interval"), Badminton::GetPresentationAge(100., 101., 21., 21.).Seconds, 0.f);
	TestTrue(TEXT("Paused game time leaves target age unchanged"), FMath::IsNearlyEqual(Badminton::GetPresentationAge(99., 100., 20.05, 20.).Seconds, .05f));
	TestEqual(TEXT("A local time reset cannot extrapolate backwards"), Badminton::GetPresentationAge(99., 100., 0., 20.).Seconds, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonPresentationClockTest,
	"FPS_Dell2g.Badminton.Presentation.PreservesUsefulServerEstimate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonPresentationClockTest::RunTest(const FString& Parameters)
{
	const auto Age = Badminton::GetPresentationAge(100.08, 100., 20.02, 20.);
	TestTrue(TEXT("Use estimated transport age when ahead of receipt time"), FMath::IsNearlyEqual(Age.Seconds, .08f));
	const auto Shifted = Badminton::GetPresentationAge(1000.08, 1000., 2020.02, 2020.);
	TestTrue(TEXT("Independent clock origins do not matter"), FMath::IsNearlyEqual(Age.Seconds, Shifted.Seconds));
	return true;
}

#endif
