#include "Misc/AutomationTest.h"
#include "FPS_Dell2g/Badminton/BadmintonThirdPerson.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonAutomaticDropTest, "FPS_Dell2g.Badminton.ThirdPerson.AutomaticDropPosition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonAutomaticDropTest::RunTest(const FString& Parameters)
{
	for (const int32 Side : {0, 1})
	{
		const float Sign = Badminton::ForwardSign(Side);
		for (const float Lateral : {-200.f, 0.f, 200.f})
		{
			TestEqual(TEXT("Front court chooses hairpin on either side"), Badminton::ResolveDropShot(Side, FVector(-Sign * 150.f, Lateral, 96)), EBadmintonShot::Hairpin);
			TestEqual(TEXT("Service line belongs to front court"), Badminton::ResolveDropShot(Side, FVector(-Sign * Badminton::ShortServiceLine, Lateral, 96)), EBadmintonShot::Hairpin);
			TestEqual(TEXT("Immediately behind service line chooses drop"), Badminton::ResolveDropShot(Side, FVector(-Sign * (Badminton::ShortServiceLine + .1f), Lateral, 96)), EBadmintonShot::Drop);
			TestEqual(TEXT("Back court chooses drop"), Badminton::ResolveDropShot(Side, FVector(-Sign * 600.f, Lateral, 96)), EBadmintonShot::Drop);
			TestEqual(TEXT("Opponent court cannot select own front court hairpin"), Badminton::ResolveDropShot(Side, FVector(Sign * 150.f, Lateral, 96)), EBadmintonShot::Drop);
		}
		// Selecting Q in back court and moving forward must change the effective shot without reselecting.
		TestNotEqual(TEXT("Crossing the boundary changes the effective shot"),
			Badminton::ResolveDropShot(Side, FVector(-Sign * 250.f, 0, 96)),
			Badminton::ResolveDropShot(Side, FVector(-Sign * 150.f, 0, 96)));
	}
	TestEqual(TEXT("Invalid side does not grant a hairpin"), Badminton::ResolveDropShot(INDEX_NONE, FVector(0,0,96)), EBadmintonShot::Drop);
	TestEqual(TEXT("Non-finite position does not grant a hairpin"), Badminton::ResolveDropShot(0, FVector(std::numeric_limits<float>::infinity(),0,96)), EBadmintonShot::Drop);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonThirdPersonTimingTest, "FPS_Dell2g.Badminton.ThirdPerson.DescendingWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonThirdPersonTimingTest::RunTest(const FString& Parameters)
{
	const auto* Data = GetDefault<UBadmintonShotData>();
	for (int32 Side = 0; Side < 2; ++Side)
	for (EBadmintonShot Shot : {EBadmintonShot::Clear, EBadmintonShot::Drop, EBadmintonShot::Smash, EBadmintonShot::Receive, EBadmintonShot::Hairpin})
	{
		const auto& Params = Data->Get(Shot);
		const FVector Player(-Badminton::ForwardSign(Side) * 400.f, 0, 90);
		const FVector Ideal(Player.X, 0, Badminton::ContactHeight(Shot, Params));
		float Quality, Seconds;
		TestEqual(TEXT("Descending ideal contact accepted"), Badminton::ThirdPersonContact(Side, Shot, Params, Player, Ideal, FVector(0,0,-300), Quality, Seconds), Badminton::EContactHint::Ready);
		TestTrue(TEXT("Exact descending plane is perfect"), Quality > .99f && FMath::Abs(Seconds) < .001f);
		TestEqual(TEXT("Rising shuttle rejected at same point"), Badminton::ThirdPersonContact(Side, Shot, Params, Player, Ideal, FVector(0,0,300), Quality, Seconds), Badminton::EContactHint::TooHigh);
		TestEqual(TEXT("Out of reach rejected"), Badminton::ThirdPersonContact(Side, Shot, Params, Player + FVector(0, Params.Reach + 1, 0), Ideal, FVector(0,0,-300), Quality, Seconds), Badminton::EContactHint::TooFar);
		for (const float Age : {-.10f, .10f, .18f})
		{
			FVector Position = Ideal, Velocity(0,0,-300);
			ABadmintonShuttle::AdvanceFlight(Position, Velocity, Age);
			float ContactTime;
			TestTrue(TEXT("Contact plane can be recovered from trajectory"), Badminton::DescendingContactTime(Position, Velocity, Ideal.Z, ContactTime));
			TestTrue(TEXT("Signed timing follows real flight time"), FMath::IsNearlyEqual(ContactTime, -Age, .001f));
			if (Position.Z >= Params.MinimumHeight && Position.Z <= Params.MaximumHeight)
			{
				TestEqual(TEXT("Near timing still connects within geometry"), Badminton::ThirdPersonContact(Side, Shot, Params, Player, Position, Velocity, Quality, Seconds), Badminton::EContactHint::Ready);
				TestTrue(TEXT("Imperfect timing lowers quality"), Quality < .99f);
			}
		}
		FVector Late = Ideal, LateVelocity(0,0,-300);
		ABadmintonShuttle::AdvanceFlight(Late, LateVelocity, .4f);
		TestTrue(TEXT("Late swing rejected"), Badminton::ThirdPersonContact(Side, Shot, Params, Player, Late, LateVelocity, Quality, Seconds) != Badminton::EContactHint::Ready);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonThirdPersonForgivenessTest, "FPS_Dell2g.Badminton.ThirdPerson.ForgivingTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonThirdPersonForgivenessTest::RunTest(const FString& Parameters)
{
	const auto* Data = GetDefault<UBadmintonShotData>();
	for (const int32 Side : {0, 1})
	for (const EBadmintonShot Shot : {EBadmintonShot::Clear, EBadmintonShot::Drop, EBadmintonShot::Receive, EBadmintonShot::Hairpin, EBadmintonShot::Smash})
	{
		const auto& Params = Data->Get(Shot);
		const FVector Player(-Badminton::ForwardSign(Side) * 150.f, 0, 96);
		const FVector Ideal(Player.X, 0, Badminton::ContactHeight(Shot, Params));
		float Quality, Seconds;
		FVector Early = Ideal, Velocity(0, 0, -350);
		ABadmintonShuttle::AdvanceFlight(Early, Velocity, -.27f);
		TestEqual(TEXT("Early descending return beyond old window now connects"),
			Badminton::ThirdPersonContact(Side, Shot, Params, Player, Early, Velocity, Quality, Seconds), Badminton::EContactHint::Ready);
		TestTrue(TEXT("Assisted edge retains useful but imperfect quality"), Quality >= .35f && Quality < .6f);
		Early = Ideal; Velocity = FVector(0, 0, -350);
		ABadmintonShuttle::AdvanceFlight(Early, Velocity, -.075f);
		TestEqual(TEXT("Human reaction error still connects"),
			Badminton::ThirdPersonContact(Side, Shot, Params, Player, Early, Velocity, Quality, Seconds), Badminton::EContactHint::Ready);
		TestEqual(TEXT("Wider perfect range"), Quality, 1.f);
		TestEqual(TEXT("Height floor remains enforced"), Badminton::ThirdPersonContact(Side, Shot, Params, Player,
			FVector(Player.X, 0, Params.MinimumHeight - 1.f), FVector(0,0,-350), Quality, Seconds), Badminton::EContactHint::TooLow);
	}
	// Use a synthetic broad height range to isolate both temporal boundaries from height rejection.
	FBadmintonShotParameters Wide = Data->Clear;
	Wide.MinimumHeight = 1.f; Wide.MaximumHeight = 1000.f;
	for (const float Age : {-.34f, .34f})
	{
		FVector Position(-400, 0, 205), Velocity(0, 0, -350);
		ABadmintonShuttle::AdvanceFlight(Position, Velocity, Age);
		float Quality, Seconds;
		TestTrue(TEXT("Outside relaxed time window remains a miss"), Badminton::ThirdPersonContact(0, EBadmintonShot::Clear,
			Wide, FVector(-400,0,96), Position, Velocity, Quality, Seconds) != Badminton::EContactHint::Ready);
	}
	TestEqual(TEXT("Non-finite timing has no quality"), Badminton::ThirdPersonTimingQuality(std::numeric_limits<float>::infinity()), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonThirdPersonPhysicsTest, "FPS_Dell2g.Badminton.ThirdPerson.TimingChangesFlight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonThirdPersonPhysicsTest::RunTest(const FString& Parameters)
{
	const auto* Data = GetDefault<UBadmintonShotData>();
	for (int32 Side = 0; Side < 2; ++Side)
	for (EBadmintonShot Shot : {EBadmintonShot::Clear, EBadmintonShot::Drop, EBadmintonShot::Smash, EBadmintonShot::Receive, EBadmintonShot::Hairpin})
	{
		const FVector Start(-Badminton::ForwardSign(Side) * 350.f, 0, Badminton::ContactHeight(Shot, Data->Get(Shot)));
		FVector Perfect = Badminton::ThirdPersonTarget(Shot, Side, 0, FVector2D(.5,.5)), Weak = Perfect;
		float FastTime, SlowTime;
		const FVector Fast = Badminton::PrepareThirdPersonFlight(Shot, Side, Start, Perfect, Data->Get(Shot).FlightTime, 1.f, 7919, FastTime);
		const FVector Slow = Badminton::PrepareThirdPersonFlight(Shot, Side, Start, Weak, Data->Get(Shot).FlightTime, .2f, 7919, SlowTime);
		TestTrue(TEXT("Poor timing lengthens flight"), SlowTime > FastTime);
		TestTrue(TEXT("Poor timing introduces placement error"), !Weak.Equals(Perfect, 1.f));
		TestTrue(TEXT("Outgoing velocity responds to timing"), !Fast.Equals(Slow, 1.f));
		TestTrue(TEXT("Finite physically solved velocity"), !Slow.ContainsNaN() && !Fast.ContainsNaN());
		FVector Landing = Start, Velocity = Slow;
		ABadmintonShuttle::AdvanceFlight(Landing, Velocity, SlowTime);
		TestTrue(TEXT("Timing trajectory reaches its dispersed target"), Landing.Equals(Weak, .1f));
	}
	return true;
}
#endif
