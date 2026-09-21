#include "BadmintonDashAbility.h"

#include "BadmintonCharacter.h"
#include "BadmintonGameState.h"
#include "BadmintonPlayerState.h"
#include "BadmintonShotData.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/CharacterMovementComponent.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_BadmintonDashEvent, "Event.Badminton.Dash");

UBadmintonDashAbility::UBadmintonDashAbility()
{
	ActivationOwnedTags.Reset();
	ActivationOwnedTags.AddTag(TAG_BadmintonDashing);
	SetShotTrigger(EBadmintonShot::Clear, TAG_BadmintonDashEvent);
}

float UBadmintonDashAbility::GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const ABadmintonGameState* Match = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld()->GetGameState<ABadmintonGameState>() : nullptr;
	return (Match ? Match->GetShotData() : GetDefault<UBadmintonShotData>())->DashCost;
}

bool UBadmintonDashAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!UGameplayAbility::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !ActorInfo || !ActorInfo->AvatarActor.IsValid()) { return false; }
	const ABadmintonGameState* Match = ActorInfo->AvatarActor->GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* Player = Cast<ABadmintonPlayerState>(ActorInfo->OwnerActor.Get());
	return Match && Player && Player->bReady && Match->ConnectedPlayers == 2 && Match->Phase == EBadmintonPhase::Rally;
}

void UBadmintonDashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (!Match || !TriggerEventData || TriggerEventData->EventMagnitude != static_cast<float>(Match->RallyId)
		|| TriggerEventData->TargetData.Num() != 1 || !TriggerEventData->TargetData.Get(0)
		|| TriggerEventData->TargetData.Get(0)->GetScriptStruct() != FGameplayAbilityTargetData_LocationInfo::StaticStruct())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const auto* Target = static_cast<const FGameplayAbilityTargetData_LocationInfo*>(TriggerEventData->TargetData.Get(0));
	FVector Direction = Target->TargetLocation.LiteralTransform.GetLocation();
	if (Direction.ContainsNaN() || FMath::Abs(Direction.Z) > .01f || Direction.SizeSquared() > 1.1f || Direction.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	const UBadmintonShotData* Data = Match->GetShotData();
	UAbilityTask_ApplyRootMotionConstantForce* Motion = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("BadmintonDash"), Direction.GetSafeNormal2D(), Data->DashSpeed, Data->DashDuration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 450.f, true);
	Motion->ReadyForActivation();
	UAbilityTask_WaitDelay* Recovery = UAbilityTask_WaitDelay::WaitDelay(this, Data->DashDuration + Data->DashRecovery);
	Recovery->OnFinish.AddDynamic(this, &ThisClass::FinishDash);
	Recovery->ReadyForActivation();
}

void UBadmintonDashAbility::FinishDash()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
