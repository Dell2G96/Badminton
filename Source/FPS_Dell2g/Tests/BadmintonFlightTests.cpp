#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "FPS_Dell2g/Badminton/BadmintonShuttle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonFlightTargetTest,
	"FPS_Dell2g.Badminton.Flight.ReachesTargetAcrossFrameRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonFlightTargetTest::RunTest(const FString& Parameters)
{
	const FVector Start(-390, 70, 160);
	const FVector Target(440, -110, 160);
	constexpr float Duration = 1.7f;
	for (const int32 FramesPerSecond : {30, 60, 120})
	{
		FVector Position = Start;
		FVector Velocity = ABadmintonShuttle::SolveVelocity(Start, Target, Duration);
		float Remaining = Duration;
		while (Remaining > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Remaining, 1.f / FramesPerSecond);
			ABadmintonShuttle::AdvanceFlight(Position, Velocity, Step);
			Remaining -= Step;
		}
		TestTrue(FString::Printf(TEXT("Target within 1cm at %dfps; error=%.3f"), FramesPerSecond, FVector::Dist(Position, Target)),
			FVector::Dist(Position, Target) < 1.f);
		TestTrue(TEXT("The shuttle descends at the receiving target"), Velocity.Z < 0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonFlightMirrorTest,
	"FPS_Dell2g.Badminton.Flight.CourtSidesHaveEqualFlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonFlightMirrorTest::RunTest(const FString& Parameters)
{
	FVector NearPosition(-390, 0, 160);
	FVector FarPosition(390, 0, 160);
	FVector NearVelocity = ABadmintonShuttle::SolveVelocity(NearPosition, FVector(440, 0, 160), 1.7f);
	FVector FarVelocity = ABadmintonShuttle::SolveVelocity(FarPosition, FVector(-440, 0, 160), 1.7f);
	ABadmintonShuttle::AdvanceFlight(NearPosition, NearVelocity, .8f);
	ABadmintonShuttle::AdvanceFlight(FarPosition, FarVelocity, .8f);
	TestTrue(TEXT("Both directions have matching heights"), FMath::IsNearlyEqual(NearPosition.Z, FarPosition.Z, .01));
	TestTrue(TEXT("Horizontal positions mirror across the net"), FMath::IsNearlyZero(NearPosition.X + FarPosition.X, .01));
	TestTrue(TEXT("Clear passes above the net"), NearPosition.Z > 155.f);
	return true;
}

#endif
