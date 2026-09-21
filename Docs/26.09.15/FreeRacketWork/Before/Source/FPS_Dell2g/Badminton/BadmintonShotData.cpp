#include "BadmintonShotData.h"
#include "Abilities/GameplayAbilityTargetTypes.h"

UBadmintonShotData::UBadmintonShotData()
{
	Serve = Clear;
	Receive.FlightTime = 1.f;
	Receive.TargetDistance = 400.f;
	Receive.TargetHeight = 100.f;
	Receive.MinimumHeight = 45.f;
	Receive.MaximumHeight = 340.f;
	Receive.Reach = 210.f;
	Receive.Recovery = .18f;
	Drop.FlightTime = 1.15f;
	Drop.TargetDistance = 220.f;
	Drop.TargetHeight = 40.f;
	Drop.StaminaCost = 8.f;
	Drop.Recovery = .4f;
	Smash.FlightTime = .65f;
	Smash.TargetDistance = 480.f;
	Smash.TargetHeight = 35.f;
	Smash.MinimumHeight = 220.f;
	Smash.StaminaCost = 18.f;
	Smash.Recovery = .5f;
}

bool UBadmintonShotData::ReadAim(const FGameplayAbilityTargetDataHandle& TargetData, FVector2D& Aim)
{
	if (TargetData.Num() != 1 || !TargetData.Get(0)
		|| TargetData.Get(0)->GetScriptStruct() != FGameplayAbilityTargetData_LocationInfo::StaticStruct()) { return false; }
	const auto* Location = static_cast<const FGameplayAbilityTargetData_LocationInfo*>(TargetData.Get(0));
	if (Location->TargetLocation.LocationType != EGameplayAbilityTargetingLocationType::LiteralTransform) { return false; }
	const FVector Value = Location->TargetLocation.LiteralTransform.GetLocation();
	if (Value.ContainsNaN() || FMath::Abs(Value.X) > 1.f || FMath::Abs(Value.Y) > 1.f || FMath::Abs(Value.Z) > .001f) { return false; }
	Aim = FVector2D(Value.X, Value.Y);
	return true;
}

bool UBadmintonShotData::ResolveAimTarget(EBadmintonShot Shot, int32 Side, int32 Score, const FVector2D& Aim, FVector& Target) const
{
	if ((Side != 0 && Side != 1) || !FMath::IsFinite(Aim.X) || !FMath::IsFinite(Aim.Y)
		|| FMath::Abs(Aim.X) > 1.f || FMath::Abs(Aim.Y) > 1.f || !FMath::IsFinite(AimWidth) || !FMath::IsFinite(AimDepth)) { return false; }
	const FBadmintonShotParameters& Params = Get(Shot);
	if (!FMath::IsFinite(Params.TargetDistance) || !FMath::IsFinite(Params.TargetHeight)) { return false; }
	constexpr float Margin = 40.f;
	const float Sign = Badminton::ForwardSign(Side);
	const bool bServe = Shot == EBadmintonShot::Serve;
	const float Depth = FMath::Clamp(Params.TargetDistance + Aim.Y * FMath::Clamp(AimDepth, 0.f, 200.f),
		bServe ? Badminton::ShortServiceLine + Margin : 120.f, Badminton::HalfLength - 120.f);
	const float Lateral = bServe ? -Badminton::ServeY(Side, Score) + Sign * Aim.X * (Badminton::HalfWidth * .5f - Margin)
		: Sign * Aim.X * FMath::Clamp(AimWidth, 0.f, Badminton::HalfWidth - Margin);
	Target = FVector(Sign * Depth, Lateral, Params.TargetHeight);
	return true;
}

const FBadmintonShotParameters& UBadmintonShotData::Get(EBadmintonShot Shot) const
{
	switch (Shot)
	{
	case EBadmintonShot::Serve: return Serve;
	case EBadmintonShot::Drop: return Drop;
	case EBadmintonShot::Smash: return Smash;
	case EBadmintonShot::Receive: return Receive;
	default: return Clear;
	}
}
