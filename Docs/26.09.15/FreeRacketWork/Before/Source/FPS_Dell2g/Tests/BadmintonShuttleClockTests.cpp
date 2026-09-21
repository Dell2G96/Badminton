#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "FPS_Dell2g/Badminton/BadmintonShuttleClock.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonClockSymmetryTest,
	"FPS_Dell2g.Badminton.Clock.SymmetricTransitAndQueueOutlier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonClockSymmetryTest::RunTest(const FString& Parameters)
{
	Badminton::FShuttleClock Clock;
	double Now = 0.;
	TestFalse(TEXT("No estimate before a reply"), Clock.Estimate(100., 20., Now));
	auto Sequence = Clock.BeginProbe(100., 20.);
	TestTrue(TEXT("100ms round trip accepted"), Clock.Accept(Sequence, 1000.05, 100.1, 20.1));
	TestTrue(TEXT("Fresh estimate available"), Clock.Estimate(100.2, 20.2, Now));
	TestTrue(TEXT("Symmetric transit reconstructs server time"), FMath::IsNearlyEqual(Now, 1000.2, 1.e-6));
	Sequence = Clock.BeginProbe(101., 21.);
	TestTrue(TEXT("Slower queued sample is recorded"), Clock.Accept(Sequence, 1001.1, 101.3, 21.3));
	Clock.Estimate(101.3, 21.3, Now);
	TestTrue(TEXT("Lower RTT sample prevents queue outlier clock jump"), FMath::IsNearlyEqual(Now, 1001.3, 1.e-6));
	TestTrue(TEXT("Selected RTT stays 100ms"), FMath::IsNearlyEqual(Clock.GetSelectedRTT(), .1, 1.e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonClockReplyTest,
	"FPS_Dell2g.Badminton.Clock.ReorderedDuplicateAndInvalidReplies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonClockReplyTest::RunTest(const FString& Parameters)
{
	Badminton::FShuttleClock Clock;
	double Now = 0.;
	const auto Old = Clock.BeginProbe(10., 5.);
	const auto Current = Clock.BeginProbe(11., 6.);
	TestFalse(TEXT("Old response ignored"), Clock.Accept(Old, 500., 11.05, 6.05));
	TestTrue(TEXT("Old response does not consume current request"), Clock.Accept(Current, 501.05, 11.1, 6.1));
	TestFalse(TEXT("Duplicate response ignored"), Clock.Accept(Current, 501.05, 11.2, 6.2));
	TestTrue(TEXT("Duplicate does not invalidate estimate"), Clock.Estimate(11.2, 6.2, Now));
	auto Sequence = Clock.BeginProbe(12., 7.);
	TestFalse(TEXT("RTT above one second rejected"), Clock.Accept(Sequence, 502., 13.1, 8.1));
	TestFalse(TEXT("Invalid matching response clears stale estimate"), Clock.Estimate(13.1, 8.1, Now));
	Sequence = Clock.BeginProbe(14., 9.);
	TestFalse(TEXT("Nonfinite server time rejected"), Clock.Accept(Sequence, std::numeric_limits<double>::infinity(), 14.1, 9.1));
	Sequence = Clock.BeginProbe(15., 10.);
	TestFalse(TEXT("Negative RTT rejected"), Clock.Accept(Sequence, 505., 14.9, 9.9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonClockLifecycleTest,
	"FPS_Dell2g.Badminton.Clock.ExpiryPauseAndWorldTimeReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonClockLifecycleTest::RunTest(const FString& Parameters)
{
	Badminton::FShuttleClock Clock;
	double Now = 0.;
	auto Sequence = Clock.BeginProbe(100., 20.);
	Clock.Accept(Sequence, 1000.05, 100.1, 20.1);
	TestFalse(TEXT("Three seconds without responses expires estimate"), Clock.Estimate(103.2, 23.2, Now));
	TestFalse(TEXT("Paused game clock cannot follow wall time"), Clock.Estimate(101., 20.1, Now));
	Sequence = Clock.BeginProbe(104., 0.);
	TestFalse(TEXT("World time reset invalidates old origin"), Clock.Estimate(104., 0., Now));
	TestTrue(TEXT("A new world origin can synchronize"), Clock.Accept(Sequence, 3.05, 104.1, .1));
	Clock.Estimate(104.2, .2, Now);
	TestTrue(TEXT("Old offset is not retained after reset"), FMath::IsNearlyEqual(Now, 3.2, 1.e-6));
	Sequence = Clock.BeginProbe(105., 1.);
	TestFalse(TEXT("Pause during round trip rejected"), Clock.Accept(Sequence, 4.2, 105.5, 1.));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonClockCorrectionTest,
	"FPS_Dell2g.Badminton.Clock.BoundedCorrectionAndSampleExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonClockCorrectionTest::RunTest(const FString& Parameters)
{
	Badminton::FShuttleClock Clock;
	double Now = 0.;
	auto Sequence = Clock.BeginProbe(100., 20.);
	Clock.Accept(Sequence, 1000.05, 100.1, 20.1);
	Sequence = Clock.BeginProbe(101., 21.);
	Clock.Accept(Sequence, 1001.07, 101.08, 21.08);
	Clock.Estimate(101.08, 21.08, Now);
	TestTrue(TEXT("30ms offset change is limited to 5ms"), FMath::IsNearlyEqual(Now, 1001.085, 1.e-6));
	for (int32 I = 2; I <= 10; ++I)
	{
		Sequence = Clock.BeginProbe(100. + I, 20. + I);
		Clock.Accept(Sequence, 1000.05 + I, 100.1 + I, 20.1 + I);
	}
	TestTrue(TEXT("Expired low RTT sample leaves the rolling window"), FMath::IsNearlyEqual(Clock.GetSelectedRTT(), .1, 1.e-6));
	return true;
}

#endif
