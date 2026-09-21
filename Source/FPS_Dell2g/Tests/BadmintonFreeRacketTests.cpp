#include "Misc/AutomationTest.h"
#include "FPS_Dell2g/Badminton/BadmintonCamera.h"
#include "FPS_Dell2g/Badminton/BadmintonShotData.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonFreeRacketTest, "FPS_Dell2g.Badminton.Camera.FreeRacketAndHairpin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonFreeRacketTest::RunTest(const FString& Parameters)
{
	using namespace Badminton;
	for (int32 Side : {0,1}) for (double X : {-90.,-45.,0.,45.,90.}) for (double Y : {-45.,-12.,40.,85.})
	{
		const FVector2D Look(X,Y);
		const FRotator Racket = FreeRacketRotation(Side,Look), Camera = FreeRacketCameraRotation(Side,Look);
		TestTrue(TEXT("Rotated racket face stays aligned to the real hit ray"), Racket.Vector().Equals(CourtCameraRotation(Side,Look).Vector(),.0001));
		const FVector Local = Camera.UnrotateVector(Racket.Vector());
		TestTrue(TEXT("Racket centre remains inside 95-degree camera view"), Local.X > .8 && FMath::Abs(Local.Y/Local.X) < 1. && FMath::Abs(Local.Z/Local.X) < .5);
		if (X == -90.) { TestTrue(TEXT("Arm extends left of screen centre"), Local.Y < -.01); }
		if (X == 90.) { TestTrue(TEXT("Arm extends right of screen centre"), Local.Y > .01); }
		if (Y == 85.) { TestTrue(TEXT("Arm extends above screen centre"), Local.Z > .1); }
	}
	TestEqual(TEXT("Racket roll covers a full 180 degrees"), FMath::Abs(FreeRacketRotation(0,FVector2D(-90.,0.)).Roll - FreeRacketRotation(0,FVector2D(90.,0.)).Roll), 180.);
	for (int32 Side : {0,1}) for (double Depth : {-1.,0.,1.})
	{
		const FVector Hairpin = InstantPlacementTarget(EBadmintonShot::Hairpin,Side,0,FVector2D(.5,Depth));
		const FVector Drop = InstantPlacementTarget(EBadmintonShot::Drop,Side,0,FVector2D(.5,Depth));
		TestTrue(TEXT("Hairpin lands closer to net than drop"), FMath::Abs(Hairpin.X) >= 65. && FMath::Abs(Hairpin.X) <= 135. && FMath::Abs(Hairpin.X) < FMath::Abs(Drop.X));
		TestTrue(TEXT("Hairpin uses opponent half and ground height"), Hairpin.X*ForwardSign(Side)>0. && Hairpin.Z==5.);
	}
	return true;
}
#endif
