#include "Misc/AutomationTest.h"
#include "FPS_Dell2g/Badminton/BadmintonPlayFeel.h"
#include "FPS_Dell2g/Badminton/BadmintonShuttle.h"
#include "FPS_Dell2g/Badminton/BadmintonRules.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonContactForecastTest, "FPS_Dell2g.Badminton.PlayFeel.ContactForecast", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonContactForecastTest::RunTest(const FString& Parameters)
{
	using namespace Badminton;
	const auto* Data = GetDefault<UBadmintonShotData>();
	for (int32 Side : {0, 1})
	{
		const float Sign = ForwardSign(Side);
		const FVector Player(-440.f * Sign, 0, 96);
		const FVector Shuttle(-340.f * Sign, 0, 250);
		TestTrue(TEXT("In reach and valid height"), ContactGeometry(Side, Player, Shuttle, Data->Clear) == EContactHint::Ready);
		TestTrue(TEXT("Far shuttle is never ready"), ContactGeometry(Side, Player, FVector(-80.f * Sign, 0, 250), Data->Clear) == EContactHint::TooFar);
		TestTrue(TEXT("Opponent half is not hittable"), ContactGeometry(Side, Player, FVector(10.f * Sign, 0, 250), Data->Clear) == EContactHint::WaitReturn);
		TestTrue(TEXT("Smash cannot hit below minimum"), ContactGeometry(Side, Player, FVector(-340.f * Sign, 0, 100), Data->Smash) == EContactHint::TooLow);
		TestTrue(TEXT("Above maximum is not hittable"), ContactGeometry(Side, Player, FVector(-340.f * Sign, 0, 500), Data->Clear) == EContactHint::TooHigh);
		FVector Position(-340.f * Sign, 0, 430), Velocity(0, 0, -100);
		float Seconds = 0;
		TestTrue(TEXT("Predict descending reachable contact"), PredictContact(Side, EBadmintonShot::Smash, Data->Smash, Position, Velocity, Player, FVector::ZeroVector, Seconds));
		ABadmintonShuttle::AdvanceFlight(Position, Velocity, Seconds);
		TestTrue(TEXT("Predicted moment satisfies actual hit geometry"), ContactGeometry(Side, Player, Position, Data->Smash) == EContactHint::Ready);
		TestTrue(TEXT("Predictor finds ideal smash height"), FMath::Abs(Position.Z - 265.f) < 12.f);
		TestFalse(TEXT("Shuttle behind the limited camera has no suggested timing"), PredictContact(Side, EBadmintonShot::Clear, Data->Clear, FVector(-500.f * Sign, 0, 250), FVector(0, 0, -100), Player, FVector::ZeroVector, Seconds));
		TestFalse(TEXT("No false gauge for unreachable trajectory"), PredictContact(Side, EBadmintonShot::Clear, Data->Clear, FVector(-80.f * Sign, 0, 430), FVector(0, 0, -100), Player, FVector::ZeroVector, Seconds));
		TestTrue(TEXT("Updated actual player position brings contact within reach"), PredictContact(Side, EBadmintonShot::Clear, Data->Clear, FVector(-180.f * Sign, 0, 430), FVector(0, 0, -100), Player + FVector(140.f * Sign, 0, 0), FVector::ZeroVector, Seconds));
		const FVector Target = DefensiveTarget(Side, FVector(580.f * Sign, 200.f * Sign, 5));
		TestTrue(TEXT("Defensive return is central and legal"), IsInCourt(Target) && Target.X * Sign >= 330.f && Target.X * Sign <= 480.f && FMath::Abs(Target.Y) < 100.f);
	}
	const FVector Offset = FRotator(0, 16, 0).Vector();
	TestTrue(TEXT("Receive accepts moderate aim error"), RacketAimMatches(EBadmintonShot::Receive, FVector::ForwardVector, Offset));
	TestFalse(TEXT("Timed shot still requires precise aim"), RacketAimMatches(EBadmintonShot::Smash, FVector::ForwardVector, Offset));
	TestFalse(TEXT("Receive does not reach behind the racket"), RacketAimMatches(EBadmintonShot::Receive, FVector::ForwardVector, -FVector::ForwardVector));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonTacticsPracticeTest, "FPS_Dell2g.Badminton.PlayFeel.TacticsAndChallenges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonTacticsPracticeTest::RunTest(const FString& Parameters)
{
	using namespace Badminton;
	const auto* Data = GetDefault<UBadmintonShotData>();
	TestTrue(TEXT("Lift over a front court opponent"), ChooseTacticalShot(200, 265, 100, *Data) == EBadmintonShot::Clear);
	TestTrue(TEXT("Drop against a deep opponent"), ChooseTacticalShot(550, 265, 100, *Data) == EBadmintonShot::Drop);
	TestTrue(TEXT("Attack a high return"), ChooseTacticalShot(400, 265, 100, *Data) == EBadmintonShot::Smash);
	TestTrue(TEXT("Low stamina uses free shot"), ChooseTacticalShot(400, 265, 0, *Data) == EBadmintonShot::Clear);
	FPracticeProgress P;
	P.Contact(1, 1, 0, 0, EBadmintonShot::Serve);
	TestEqual(TEXT("Serve excluded from rally goal"), P.RallyHits, 0);
	P.Contact(1, 2, 1, 0, EBadmintonShot::Clear);
	P.Contact(1, 3, 0, 0, EBadmintonShot::Drop);
	P.Contact(1, 4, 1, 0, EBadmintonShot::Clear);
	P.Contact(1, 5, 0, 0, EBadmintonShot::Smash);
	P.Contact(1, 5, 0, 0, EBadmintonShot::Smash);
	TestEqual(TEXT("Duplicate contact ignored"), P.RallyHits, 4);
	const FVector Left = P.Target(0);
	TestTrue(TEXT("Winning combo and target earns reward"), P.Landing(1, 0, 0, 0, EBadmintonShot::Smash, Left));
	TestEqual(TEXT("Combo is counted once"), P.ComboPoints, 1);
	TestEqual(TEXT("Target counted on real landing"), P.Targets, 1);
	TestTrue(TEXT("Next target is opposite side"), P.Target(0).Y > 0);
	TestFalse(TEXT("Duplicate point ignored"), P.Landing(1, 0, 0, 0, EBadmintonShot::Smash, Left));
	P.Contact(2, 6, 0, 0, EBadmintonShot::Serve);
	TestEqual(TEXT("New rally resets hits"), P.RallyHits, 0);
	TestEqual(TEXT("Best survives new rally"), P.BestRally, 4);
	P.Contact(2, 7, 0, 0, EBadmintonShot::Smash);
	TestFalse(TEXT("Losing point cannot complete target"), P.Landing(2, 1, 0, 0, EBadmintonShot::Smash, P.Target(0)));
	TestEqual(TEXT("Previous rally drop cannot complete combo"), P.ComboPoints, 1);
	P.Contact(1, 100, 0, 0, EBadmintonShot::Clear);
	TestEqual(TEXT("Old rally message ignored"), P.Rally, 2);
	return true;
}
#endif
