#include "Misc/AutomationTest.h"
#include "FPS_Dell2g/Badminton/BadmintonMinimap.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonMinimapTest,"FPS_Dell2g.Badminton.Minimap.CourtOrientationAndPositions",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonMinimapTest::RunTest(const FString& Parameters)
{
	using namespace Badminton;
	for (int32 Side : {0,1})
	{
		const float Sign=ForwardSign(Side);
		TestTrue(TEXT("Local player is below net"),MinimapPoint(FVector(-440*Sign,0,96),Side).Y>.5);
		TestTrue(TEXT("Opponent is above net"),MinimapPoint(FVector(440*Sign,0,96),Side).Y<.5);
		TestTrue(TEXT("Local right maps right"),MinimapPoint(FVector(0,200*Sign,0),Side).X>.5);
		TestTrue(TEXT("Net centre remains centred"),MinimapPoint(FVector::ZeroVector,Side).Equals(FVector2D(.5,.5)));
		TestTrue(TEXT("Shuttle altitude does not move its court position"),MinimapPoint(FVector(150,80,500),Side).Equals(MinimapPoint(FVector(150,80,5),Side)));
		TestTrue(TEXT("Current and landing positions stay distinct"),!MinimapPoint(FVector(-100,50,300),Side).Equals(MinimapPoint(FVector(-520,-70,0),Side)));
		TestTrue(TEXT("Court boundary leaves space for out landings"),MinimapPoint(FVector(700*Sign,300*Sign,0),Side).X<1. && MinimapPoint(FVector(700*Sign,300*Sign,0),Side).Y>0.);
	}
	TestTrue(TEXT("Rotating sides preserves local layout"),MinimapPoint(FVector(-300,150,0),0).Equals(MinimapPoint(FVector(300,-150,0),1)));
	return true;
}
#endif
