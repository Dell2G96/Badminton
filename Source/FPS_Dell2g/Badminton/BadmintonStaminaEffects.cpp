#include "BadmintonStaminaEffects.h"
#include "BadmintonAttributeSet.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_BadmintonStaminaCost, "Data.Badminton.StaminaCost");

UBadmintonStaminaCostEffect::UBadmintonStaminaCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UBadmintonAttributeSet::GetStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Amount;
	Amount.DataTag = TAG_BadmintonStaminaCost;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);
	Modifiers.Add(Modifier);
}

UBadmintonStaminaRegenEffect::UBadmintonStaminaRegenEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = .25f;
	bExecutePeriodicEffectOnApplication = false;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UBadmintonAttributeSet::GetStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Amount;
	Amount.DataTag = TAG_BadmintonStaminaCost;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);
	Modifiers.Add(Modifier);
}
