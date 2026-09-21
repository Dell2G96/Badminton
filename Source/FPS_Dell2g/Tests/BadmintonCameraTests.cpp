#include "Misc/AutomationTest.h"
#include "FPS_Dell2g/Badminton/BadmintonCamera.h"
#include "FPS_Dell2g/Badminton/BadmintonRules.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonCameraLookTest, "FPS_Dell2g.Badminton.Camera.MouseLimits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonCameraLookTest::RunTest(const FString& Parameters)
{
	const FVector2D Initial(0., Badminton::CameraDefaultPitch);
	TestTrue(TEXT("Upper right stops at limits"), Badminton::ApplyMouseLook(Initial, FVector2D(1e6, 1e6), .72f).Equals(FVector2D(90., 85.)));
	TestTrue(TEXT("Lower left stops at limits"), Badminton::ApplyMouseLook(Initial, FVector2D(-1e6, -1e6), .72f).Equals(FVector2D(-90., -45.)));
	const FVector2D Once = Badminton::ApplyMouseLook(Initial, FVector2D(50., 25.), .72f);
	FVector2D Split = Initial;
	for (int32 Index=0; Index<10; ++Index) { Split = Badminton::ApplyMouseLook(Split, FVector2D(5., 2.5), .72f); }
	TestTrue(TEXT("Mouse sensitivity independent of frame count"), Once.Equals(Split, .001));
	TestTrue(TEXT("Nonfinite mouse ignored"), Badminton::ApplyMouseLook(Initial, FVector2D(std::numeric_limits<double>::quiet_NaN(), 0.), .72f).Equals(Initial));
	TestTrue(TEXT("No mouse keeps the view still"), Badminton::ApplyMouseLook(Once, FVector2D::ZeroVector, .72f).Equals(Once));
	const FVector A = Badminton::CourtCameraRotation(0, Once).Vector(), B = Badminton::CourtCameraRotation(1, Once).Vector();
	TestTrue(TEXT("Both court sides turn in their own facing direction"), B.Equals(FVector(-A.X,-A.Y,A.Z), .001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonCameraAimTest, "FPS_Dell2g.Badminton.Camera.CourtPlacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonCameraAimTest::RunTest(const FString& Parameters)
{
	for (int32 Side=0; Side<2; ++Side) for (int32 Score=0; Score<2; ++Score) for (bool bServe : {false,true})
	for (double X : {-90.,-65.,0.,65.,90.}) for (double Y : {-45.,-12.,0.,75.,85.})
	{
		const FVector Eye(-Badminton::ForwardSign(Side)*440., Badminton::ServeY(Side,Score),181.);
		const FRotator View = Badminton::CourtCameraRotation(Side,FVector2D(X,Y));
		const FVector2D Aim = Badminton::AimFromCamera(Side,Score,bServe,Eye,View);
		TestTrue(TEXT("Aim stays finite and normalized at all look limits"), !Aim.ContainsNaN() && FMath::Abs(Aim.X)<=1. && FMath::Abs(Aim.Y)<=1.);
		const FVector Target = Badminton::PlacementTarget(Side,Score,bServe,Aim);
		TestTrue(TEXT("Camera aim stays on opponent court"), Badminton::IsInCourt(Target) && Target.X*Badminton::ForwardSign(Side)>0.);
		if (bServe) { TestTrue(TEXT("Serve view stays in legal diagonal box"), Badminton::IsInServiceBox(Target,Side,Score)); }
	}
	const FVector Eye(-440.,0.,181.);
	const FVector2D Left = Badminton::AimFromCamera(0,0,false,Eye,FRotator(-12.,-8.,0.));
	const FVector2D Right = Badminton::AimFromCamera(0,0,false,Eye,FRotator(-12.,8.,0.));
	TestTrue(TEXT("Turning right moves landing right"), Right.X>Left.X);
	const FVector2D Near = Badminton::AimFromCamera(0,0,false,Eye,FRotator(-25.,0.,0.));
	const FVector2D Far = Badminton::AimFromCamera(0,0,false,Eye,FRotator(-8.,0.,0.));
	TestTrue(TEXT("Looking higher moves landing deeper"), Far.Y>Near.Y);
	return true;
}
#endif
