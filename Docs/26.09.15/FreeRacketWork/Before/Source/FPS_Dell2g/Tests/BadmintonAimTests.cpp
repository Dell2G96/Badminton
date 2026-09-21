#include "Misc/AutomationTest.h"
#include "FPS_Dell2g/Badminton/BadmintonShotData.h"
#include "FPS_Dell2g/Badminton/BadmintonRules.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	FGameplayAbilityTargetDataHandle AimPayload(const FVector& Value)
	{
		FGameplayAbilityTargetDataHandle Handle;
		auto* Data = new FGameplayAbilityTargetData_LocationInfo();
		Data->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
		Data->TargetLocation.LiteralTransform.SetLocation(Value);
		Handle.Add(Data);
		return Handle;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonAimPayloadTest, "FPS_Dell2g.Badminton.Aim.RejectMalformedPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonAimPayloadTest::RunTest(const FString& Parameters)
{
	FVector2D Aim;
	TestFalse(TEXT("Missing aim is rejected"), UBadmintonShotData::ReadAim({}, Aim));
	TestTrue(TEXT("Full diagonal aim is valid"), UBadmintonShotData::ReadAim(AimPayload(FVector(1, -1, 0)), Aim));
	TestTrue(TEXT("Both axes survive extraction"), Aim.Equals(FVector2D(1, -1)));
	for (const FVector& Invalid : {FVector(1.01, 0, 0), FVector(0, -1.01, 0), FVector(0, 0, 10)})
	{
		TestFalse(TEXT("Out-of-range aim is rejected"), UBadmintonShotData::ReadAim(AimPayload(Invalid), Aim));
	}
	FGameplayAbilityTargetDataHandle Multiple = AimPayload(FVector::ZeroVector);
	Multiple.Append(AimPayload(FVector::ZeroVector));
	TestFalse(TEXT("Multiple locations are rejected"), UBadmintonShotData::ReadAim(Multiple, Aim));
	FGameplayAbilityTargetDataHandle WrongType;
	WrongType.Add(new FGameplayAbilityTargetData_SingleTargetHit());
	TestFalse(TEXT("Client hit results are not trusted as aim"), UBadmintonShotData::ReadAim(WrongType, Aim));
	FGameplayAbilityTargetDataHandle ActorLocation;
	auto* Nonliteral = new FGameplayAbilityTargetData_LocationInfo();
	Nonliteral->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::ActorTransform;
	ActorLocation.Add(Nonliteral);
	TestFalse(TEXT("A nonliteral target is rejected"), UBadmintonShotData::ReadAim(ActorLocation, Aim));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonAimSymmetryTest, "FPS_Dell2g.Badminton.Aim.MirroredSidesAndServiceBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonAimSymmetryTest::RunTest(const FString& Parameters)
{
	const UBadmintonShotData* Data = GetDefault<UBadmintonShotData>();
	for (const EBadmintonShot Shot : {EBadmintonShot::Serve, EBadmintonShot::Clear, EBadmintonShot::Drop, EBadmintonShot::Smash})
	{
		for (const int32 Score : {0, 1, 10, 11})
		{
			for (const double X : {-1., 0., 1.})
			{
				for (const double Y : {-1., 0., 1.})
				{
					FVector Near, Far;
					TestTrue(TEXT("Near side resolves"), Data->ResolveAimTarget(Shot, 0, Score, FVector2D(X, Y), Near));
					TestTrue(TEXT("Far side resolves"), Data->ResolveAimTarget(Shot, 1, Score, FVector2D(X, Y), Far));
					TestTrue(TEXT("Screen-relative aim is symmetric"), FVector(-Near.X, -Near.Y, Near.Z).Equals(Far, .01f));
					TestTrue(TEXT("Targets stay in opponent court"), Near.X > 0 && Far.X < 0 && Badminton::IsInCourt(Near) && Badminton::IsInCourt(Far));
					if (Shot == EBadmintonShot::Serve)
					{
						TestTrue(TEXT("Near diagonal service box"), Badminton::IsInServiceBox(Near, 0, Score));
						TestTrue(TEXT("Far diagonal service box"), Badminton::IsInServiceBox(Far, 1, Score));
					}
				}
			}
		}
		FVector Neutral;
		Data->ResolveAimTarget(Shot, 0, 0, FVector2D::ZeroVector, Neutral);
		TestTrue(TEXT("Centered aim preserves existing default target"), Neutral.Equals(FVector(Data->Get(Shot).TargetDistance,
			Shot == EBadmintonShot::Serve ? -Badminton::ServeY(0, 0) : 0, Data->Get(Shot).TargetHeight), .01f));
	}
	FVector Left, Right, Short, Deep;
	Data->ResolveAimTarget(EBadmintonShot::Clear, 0, 0, FVector2D(-1, 0), Left);
	Data->ResolveAimTarget(EBadmintonShot::Clear, 0, 0, FVector2D(1, 0), Right);
	Data->ResolveAimTarget(EBadmintonShot::Clear, 0, 0, FVector2D(0, -1), Short);
	Data->ResolveAimTarget(EBadmintonShot::Clear, 0, 0, FVector2D(0, 1), Deep);
	TestTrue(TEXT("Mouse right shifts aim right"), Right.Y > Left.Y);
	TestTrue(TEXT("Mouse up shifts aim deeper"), Deep.X > Short.X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBadmintonAimInvalidValuesTest, "FPS_Dell2g.Badminton.Aim.RejectNonFiniteAndInvalidSide",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBadmintonAimInvalidValuesTest::RunTest(const FString& Parameters)
{
	const UBadmintonShotData* Data = GetDefault<UBadmintonShotData>();
	FVector Target;
	for (int32 Side : {-1, 2}) { TestFalse(TEXT("Invalid side is rejected"), Data->ResolveAimTarget(EBadmintonShot::Clear, Side, 0, FVector2D::ZeroVector, Target)); }
	for (const FVector2D& Invalid : {FVector2D(std::numeric_limits<double>::quiet_NaN(), 0), FVector2D(0, std::numeric_limits<double>::infinity()), FVector2D(-2, 0)})
	{
		TestFalse(TEXT("Invalid aim cannot produce a server target"), Data->ResolveAimTarget(EBadmintonShot::Clear, 0, 0, Invalid, Target));
	}
	UBadmintonShotData* Custom = NewObject<UBadmintonShotData>();
	Custom->AimDepth = 10000;
	Custom->AimWidth = 10000;
	TestTrue(TEXT("Large tuning values use bounded ranges"), Custom->ResolveAimTarget(EBadmintonShot::Clear, 0, 0, FVector2D(1, 1), Target));
	TestTrue(TEXT("Clamped target remains in court"), Badminton::IsInCourt(Target));
	Custom->AimWidth = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Invalid tuning is rejected"), Custom->ResolveAimTarget(EBadmintonShot::Clear, 0, 0, FVector2D::ZeroVector, Target));
	return true;
}
#endif
