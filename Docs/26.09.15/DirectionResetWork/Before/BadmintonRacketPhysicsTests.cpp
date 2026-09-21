#include "Misc/AutomationTest.h"
#include "FPS_Dell2g/Badminton/BadmintonRacketPhysics.h"
#include "FPS_Dell2g/Badminton/BadmintonShuttle.h"
#include "FPS_Dell2g/Badminton/BadmintonCourt.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonRacketPhysicsTest,"FPS_Dell2g.Badminton.RacketPhysics.ReflectionAndSmashExclusion",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonRacketPhysicsTest::RunTest(const FString& Parameters)
{
	using namespace Badminton;
	const FVector Incoming(-500,60,-200), Desired(550,-50,500), Face=FVector::ForwardVector;
	for (EBadmintonShot Shot : {EBadmintonShot::Serve,EBadmintonShot::Drop,EBadmintonShot::Clear,EBadmintonShot::Hairpin,EBadmintonShot::Receive})
	{ TestTrue(TEXT("Neutral face preserves the original stroke exactly"),ResolveRacketImpact(Incoming,Desired,Face,0,Shot).Equals(Desired,.0001)); }
	for (float Angle : {-30.f,-12.f,12.f,30.f})
	{
		TestTrue(TEXT("Smash velocity ignores wheel angle"),ResolveRacketImpact(Incoming,Desired,Face,Angle,EBadmintonShot::Smash).Equals(Desired,.0001));
		const FVector Out=ResolveRacketImpact(Incoming,Desired,Face,Angle,EBadmintonShot::Clear);
		const FVector Normal=TiltedRacketNormal(Face,Angle), Surface=RacketSurfaceVelocity(Incoming,Desired,Face);
		const FVector Before=Incoming-Surface, After=Out-Surface;
		TestTrue(TEXT("Normal velocity follows restitution in moving racket frame"), FMath::Abs(FVector::DotProduct(After,Normal)+RacketRestitution*FVector::DotProduct(Before,Normal))<.001);
		TestTrue(TEXT("Tangential relative velocity follows string friction"), (After-Normal*FVector::DotProduct(After,Normal)).Equals(RacketTangentialRetention*(Before-Normal*FVector::DotProduct(Before,Normal)),.001));
		const FVector Mirrored=ResolveRacketImpact(FVector(-Incoming.X,-Incoming.Y,Incoming.Z),FVector(-Desired.X,-Desired.Y,Desired.Z),-Face,Angle,EBadmintonShot::Clear);
		TestTrue(TEXT("Opposite court applies the same tilt physics"),Mirrored.Equals(FVector(-Out.X,-Out.Y,Out.Z),.001));
	}
	TestTrue(TEXT("Opening face raises this return"),ResolveRacketImpact(Incoming,Desired,Face,12,EBadmintonShot::Clear).Z>Desired.Z);
	TestTrue(TEXT("Closing face lowers this return"),ResolveRacketImpact(Incoming,Desired,Face,-12,EBadmintonShot::Clear).Z<Desired.Z);
	TestEqual(TEXT("Wheel step is 3 degrees"),AdjustRacketTilt(0,1),3.f);
	TestEqual(TEXT("Wheel upper stop"),AdjustRacketTilt(0,1000),30.f);
	TestEqual(TEXT("Wheel lower stop"),AdjustRacketTilt(0,-1000),-30.f);
	TestEqual(TEXT("Nonfinite wheel is ignored"),AdjustRacketTilt(6,std::numeric_limits<float>::quiet_NaN()),6.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonRacketLandingTest,"FPS_Dell2g.Badminton.RacketPhysics.TiltChangesActualLanding",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBadmintonRacketLandingTest::RunTest(const FString& Parameters)
{
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	World->SetGameState(World->SpawnActor<AGameStateBase>());
	World->SpawnActor<ABadmintonCourt>();
	auto* Shuttle=World->SpawnActor<ABadmintonShuttle>();
	int32 Rally=0;
	for (int32 Side : {0,1})
	{
		const float Sign=Badminton::ForwardSign(Side);
		const FVector Start(-350*Sign,0,230),Target(300*Sign,0,5);
		const FVector Desired=ABadmintonShuttle::SolveVelocity(Start,Target,1.7f);
		FVector PreviousLanding=FVector::ZeroVector;
		for (float Angle : {-6.f,0.f,6.f})
		{
			const FVector Velocity=Badminton::ResolveRacketImpact(FVector(-500*Sign,0,-150),Desired,FVector(Sign,0,0),Angle,EBadmintonShot::Clear);
			Shuttle->Launch(Start,Target,Side,++Rally,1.7f,EBadmintonShot::Clear,FVector2D::ZeroVector,&Velocity);
			TestTrue(TEXT("Tilted flight has a ground prediction"),Shuttle->GetFlight().bHasLanding);
			const FVector Predicted=Shuttle->GetFlight().Landing;
			for(int32 Frame=0;Frame<144*8 && Shuttle->GetFlight().bFlying;++Frame) {Shuttle->Tick(1.f/144.f);}
			const FVector Actual=Shuttle->GetActorLocation();
			TestTrue(TEXT("Minimap/world prediction remains within 2cm after tilt"),FVector::Dist2D(Predicted,Actual)<2.f);
			if(Angle>-6.f) {TestTrue(TEXT("Wheel angle changes real ground position, not only the marker"),FVector::Dist2D(PreviousLanding,Actual)>5.f);}
			PreviousLanding=Actual;
		}
	}
	World->DestroyWorld(false);
	return true;
}
#endif
