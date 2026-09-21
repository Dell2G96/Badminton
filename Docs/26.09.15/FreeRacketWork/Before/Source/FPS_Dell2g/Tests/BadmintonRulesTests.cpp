#include "Misc/AutomationTest.h"
#include "FPS_Dell2g/Badminton/BadmintonRules.h"
#include "FPS_Dell2g/Badminton/BadmintonShotData.h"
#include "FPS_Dell2g/Badminton/BadmintonShuttle.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonLandingRulesTest, "FPS_Dell2g.Badminton.Rules.LandingAndService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonLandingRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Opponent court landing awards hitter"), Badminton::JudgeLanding(FVector(440, 80, 5), 0, false, 0, true).Winner, 0);
	TestEqual(TEXT("Own court landing awards opponent"), Badminton::JudgeLanding(FVector(-440, 80, 5), 0, false, 0, true).Winner, 1);
	TestEqual(TEXT("Out awards opponent"), Badminton::JudgeLanding(FVector(680, 0, 5), 0, false, 0, true).Winner, 1);
	TestTrue(TEXT("Boundary line is in"), Badminton::IsInCourt(FVector(670, 259, 5)));
	TestFalse(TEXT("Beyond sideline is out"), Badminton::IsInCourt(FVector(440, 259.1, 5)));
	TestTrue(TEXT("Even service diagonal"), Badminton::IsInServiceBox(FVector(440, -100, 5), 0, 0));
	TestFalse(TEXT("Even service wrong half"), Badminton::IsInServiceBox(FVector(440, 100, 5), 0, 0));
	TestTrue(TEXT("Odd service diagonal"), Badminton::IsInServiceBox(FVector(440, 100, 5), 0, 1));
	TestTrue(TEXT("Side 1 diagonal symmetry"), Badminton::IsInServiceBox(FVector(-440, 100, 5), 1, 0));
	TestFalse(TEXT("Short service fault"), Badminton::IsInServiceBox(FVector(197, -100, 5), 0, 0));
	TestEqual(TEXT("Invalid net crossing cannot score"), Badminton::JudgeLanding(FVector(440, 0, 5), 0, false, 0, false).Winner, 1);
	TestEqual(TEXT("Invalid player ignored"), Badminton::JudgeLanding(FVector(440, 0, 5), 3, false, 0, true).Winner, INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonShotProfilesTest, "FPS_Dell2g.Badminton.Shots.TrajectoryAndCosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonShotProfilesTest::RunTest(const FString& Parameters)
{
	const UBadmintonShotData* Data = GetDefault<UBadmintonShotData>();
	TestEqual(TEXT("Clear remains free"), Data->Clear.StaminaCost, 0.f);
	TestEqual(TEXT("Serve remains free"), Data->Serve.StaminaCost, 0.f);
	for (EBadmintonShot Shot : {EBadmintonShot::Serve, EBadmintonShot::Clear, EBadmintonShot::Drop, EBadmintonShot::Smash})
	{
		const FBadmintonShotParameters& Params = Data->Get(Shot);
		const FVector Start(-390, 0, Shot == EBadmintonShot::Smash ? 300.f : 160.f);
		const FVector Target(Params.TargetDistance, 0, Params.TargetHeight);
		FVector Position = Start;
		FVector Velocity = ABadmintonShuttle::SolveVelocity(Start, Target, Params.FlightTime);
		float Remaining = Params.FlightTime;
		bool bCrossedAboveNet = false;
		while (Remaining > SMALL_NUMBER)
		{
			const FVector Previous = Position;
			const float Step = FMath::Min(Remaining, 1.f / 120.f);
			ABadmintonShuttle::AdvanceFlight(Position, Velocity, Step);
			Remaining -= Step;
			if (Previous.X <= 0 && Position.X > 0) { bCrossedAboveNet = Position.Z > 155.f; }
		}
		TestTrue(TEXT("Default shot clears net from intended height"), bCrossedAboveNet);
		TestTrue(TEXT("Default shot reaches its target"), FVector::Dist(Position, Target) < 1.f);
	}
	TestTrue(TEXT("Drop lands shorter than clear"), Data->Drop.TargetDistance < Data->Clear.TargetDistance);
	TestTrue(TEXT("Smash flies faster than clear"), Data->Smash.FlightTime < Data->Clear.FlightTime);
	return true;
}
#endif
