#pragma once

#include "BadmintonClearAbility.h"
#include "BadmintonDashAbility.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BadmintonDashEvent);

UCLASS()
class FPS_DELL2G_API UBadmintonDashAbility : public UBadmintonClearAbility
{
	GENERATED_BODY()
public:
	UBadmintonDashAbility();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
protected:
	virtual float GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const override;
private:
	UFUNCTION() void FinishDash();
};
