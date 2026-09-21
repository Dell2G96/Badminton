#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NativeGameplayTags.h"
#include "BadmintonTypes.h"
#include "BadmintonClearAbility.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BadmintonClearEvent);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BadmintonDropEvent);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BadmintonSmashEvent);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BadmintonServeEvent);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BadmintonSwinging);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BadmintonDashing);

UCLASS()
class FPS_DELL2G_API UBadmintonClearAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBadmintonClearAbility();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual float GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const;
	EBadmintonShot ShotType = EBadmintonShot::Clear;
	void SetShotTrigger(EBadmintonShot Shot, FGameplayTag Event);

private:
	UFUNCTION()
	void FinishSwing();
};

UCLASS()
class FPS_DELL2G_API UBadmintonDropAbility : public UBadmintonClearAbility
{
	GENERATED_BODY()
public: UBadmintonDropAbility();
};

UCLASS()
class FPS_DELL2G_API UBadmintonSmashAbility : public UBadmintonClearAbility
{
	GENERATED_BODY()
public: UBadmintonSmashAbility();
};

UCLASS()
class FPS_DELL2G_API UBadmintonServeAbility : public UBadmintonClearAbility
{
	GENERATED_BODY()
public: UBadmintonServeAbility();
};
