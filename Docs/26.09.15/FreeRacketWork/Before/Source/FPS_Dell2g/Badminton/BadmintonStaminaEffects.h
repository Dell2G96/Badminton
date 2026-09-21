#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "BadmintonStaminaEffects.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BadmintonStaminaCost);

UCLASS()
class FPS_DELL2G_API UBadmintonStaminaCostEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UBadmintonStaminaCostEffect();
};

UCLASS()
class FPS_DELL2G_API UBadmintonStaminaRegenEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UBadmintonStaminaRegenEffect();
};
