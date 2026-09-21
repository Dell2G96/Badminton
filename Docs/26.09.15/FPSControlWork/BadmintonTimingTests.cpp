#include "Misc/AutomationTest.h"
#include "FPS_Dell2g/Badminton/BadmintonTiming.h"
#include "FPS_Dell2g/Badminton/BadmintonShotData.h"
#include "FPS_Dell2g/Badminton/BadmintonRules.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonTimingQualityTest,"FPS_Dell2g.Badminton.Timing.QualityAndPower",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBadmintonTimingQualityTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Perfect impact"),Badminton::TimingQuality(0.),1.f);
	TestEqual(TEXT("Perfect window edge"),Badminton::TimingQuality(.045),1.f);
	TestTrue(TEXT("Early and late are symmetric"),FMath::IsNearlyEqual(Badminton::TimingQuality(-.12),Badminton::TimingQuality(.12)));
	TestTrue(TEXT("Far timing is weaker"),Badminton::TimingQuality(.20)<Badminton::TimingQuality(.10));
	TestEqual(TEXT("Very late has no bonus"),Badminton::TimingQuality(1.),0.f);
	TestEqual(TEXT("NaN has no bonus"),Badminton::TimingQuality(std::numeric_limits<double>::quiet_NaN()),0.f);
	TestTrue(TEXT("Perfect flight is faster for same target"),Badminton::TimingFlightScale(1.f)<Badminton::TimingFlightScale(0.f));
	const auto* Data=GetDefault<UBadmintonShotData>();
	TestTrue(TEXT("Receive recovers faster than clear"),Data->Receive.Recovery<Data->Clear.Recovery);
	TestTrue(TEXT("Receive has wider reach"),Data->Receive.Reach>Data->Clear.Reach);
	TestEqual(TEXT("Receive is free"),Data->Receive.StaminaCost,0.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonTimingPlacementTest,"FPS_Dell2g.Badminton.Timing.PlacementAndAccuracy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBadmintonTimingPlacementTest::RunTest(const FString& Parameters)
{
	for (int32 Side=0;Side<2;++Side) for(int32 Score=0;Score<2;++Score) for(bool bServe:{false,true})
	for(float X:{-1.f,0.f,1.f}) for(float Y:{-1.f,0.f,1.f})
	{
		const FVector Target=Badminton::PlacementTarget(Side,Score,bServe,FVector2D(X,Y));
		TestTrue(TEXT("Mirrored target"),Badminton::PlacementTarget(1-Side,Score,bServe,FVector2D(X,Y)).Equals(FVector(-Target.X,-Target.Y,Target.Z),.01f));
		for(int32 Seed=0;Seed<20;++Seed)
		{
			TestTrue(TEXT("Perfect has no scatter"),Badminton::ScatterTarget(Target,Side,bServe,1.f,Seed).Equals(Target));
			const FVector Weak=Badminton::ScatterTarget(Target,Side,bServe,0.f,Seed);
			TestTrue(TEXT("Weak shot stays in opponent half"),Weak.X*Badminton::ForwardSign(Side)>0 && Badminton::IsInCourt(Weak));
			if(bServe) { TestTrue(TEXT("Timing scatter preserves diagonal service"),Badminton::IsInServiceBox(Weak,Side,Score)); }
		}
	}
	return true;
}
#endif
