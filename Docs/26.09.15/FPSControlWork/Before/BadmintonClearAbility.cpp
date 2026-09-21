#include "BadmintonClearAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "BadmintonCharacter.h"
#include "BadmintonGameMode.h"
#include "BadmintonGameState.h"
#include "NativeGameplayTags.h"
#include "BadmintonPlayerState.h"
#include "BadmintonPlayerController.h"
#include "BadmintonAttributeSet.h"
#include "BadmintonShotData.h"
#include "BadmintonStaminaEffects.h"
#include "BadmintonShotDiagnostics.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_BadmintonSwinging, "State.Swinging");
UE_DEFINE_GAMEPLAY_TAG(TAG_BadmintonDashing, "State.Dashing");
UE_DEFINE_GAMEPLAY_TAG(TAG_BadmintonClearEvent, "Event.Badminton.Clear");
UE_DEFINE_GAMEPLAY_TAG(TAG_BadmintonDropEvent, "Event.Badminton.Drop");
UE_DEFINE_GAMEPLAY_TAG(TAG_BadmintonSmashEvent, "Event.Badminton.Smash");
UE_DEFINE_GAMEPLAY_TAG(TAG_BadmintonServeEvent, "Event.Badminton.Serve");

UBadmintonClearAbility::UBadmintonClearAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationOwnedTags.AddTag(TAG_BadmintonSwinging);
	ActivationBlockedTags.AddTag(TAG_BadmintonSwinging);
	ActivationBlockedTags.AddTag(TAG_BadmintonDashing);
	SetShotTrigger(EBadmintonShot::Clear, TAG_BadmintonClearEvent);
}

void UBadmintonClearAbility::SetShotTrigger(EBadmintonShot Shot, FGameplayTag Event)
{
	ShotType = Shot;
	AbilityTriggers.Reset();
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = Event;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

UBadmintonDropAbility::UBadmintonDropAbility() { SetShotTrigger(EBadmintonShot::Drop, TAG_BadmintonDropEvent); }
UBadmintonSmashAbility::UBadmintonSmashAbility() { SetShotTrigger(EBadmintonShot::Smash, TAG_BadmintonSmashEvent); }
UBadmintonServeAbility::UBadmintonServeAbility() { SetShotTrigger(EBadmintonShot::Serve, TAG_BadmintonServeEvent); }

bool UBadmintonClearAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const ABadmintonGameState* Match = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld()->GetGameState<ABadmintonGameState>() : nullptr;
	return Match && ActorInfo->AbilitySystemComponent.IsValid() && ActorInfo->AbilitySystemComponent->GetNumericAttribute(UBadmintonAttributeSet::GetStaminaAttribute()) >= GetStaminaCost(ActorInfo);
}

float UBadmintonClearAbility::GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const ABadmintonGameState* Match = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld()->GetGameState<ABadmintonGameState>() : nullptr;
	return (Match ? Match->GetShotData() : GetDefault<UBadmintonShotData>())->Get(ShotType).StaminaCost;
}

void UBadmintonClearAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const float Cost = GetStaminaCost(ActorInfo);
	if (Cost <= 0.f) { return; }
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UBadmintonStaminaCostEffect::StaticClass());
	Spec.Data->SetSetByCallerMagnitude(TAG_BadmintonStaminaCost, -Cost);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

bool UBadmintonClearAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}
	const ABadmintonGameState* Match = ActorInfo->AvatarActor->GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* Player = Cast<ABadmintonPlayerState>(ActorInfo->OwnerActor.Get());
	return Match && Player && Player->bReady && Match->ConnectedPlayers == 2
		&& ((Match->Phase == EBadmintonPhase::ReadyToServe && Player->CourtSide == Match->ServingSide && (ShotType == EBadmintonShot::Serve || ShotType == EBadmintonShot::Clear))
			|| (Match->Phase == EBadmintonPhase::Rally && ShotType != EBadmintonShot::Serve));
}

void UBadmintonClearAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Badminton::TraceShot(TEXT("Activate"), ActorInfo->AbilitySystemComponent.Get(), Cast<APawn>(ActorInfo->AvatarActor.Get()), ShotType, ActivationInfo.GetActivationPredictionKey().Current);
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (!TriggerEventData || !Match || !FMath::IsFinite(TriggerEventData->EventMagnitude)
		|| TriggerEventData->EventMagnitude != static_cast<float>(Match->RallyId))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	FVector2D Aim;
	if (!UBadmintonShotData::ReadAim(TriggerEventData->TargetData, Aim))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (ActorInfo->IsNetAuthority())
	{
		ABadmintonGameMode* Mode = GetWorld()->GetAuthGameMode<ABadmintonGameMode>();
		if (Mode && TriggerEventData && FMath::IsFinite(TriggerEventData->EventMagnitude)
			&& TriggerEventData->EventMagnitude >= 0.f && TriggerEventData->EventMagnitude < 16000000.f)
		{
			const bool bContact = Mode->TryShot(Cast<ABadmintonCharacter>(ActorInfo->AvatarActor.Get()), static_cast<int32>(TriggerEventData->EventMagnitude), ShotType, Aim);
			Badminton::TraceShot(TEXT("Contact"), ActorInfo->AbilitySystemComponent.Get(), Cast<APawn>(ActorInfo->AvatarActor.Get()), ShotType, ActivationInfo.GetActivationPredictionKey().Current, bContact);
			if (ABadmintonPlayerController* Controller = Cast<ABadmintonPlayerController>(ActorInfo->PlayerController.Get()))
			{
				Controller->ClientShotFeedback(ShotType, bContact, Match->RallyId, Match->GetServerWorldTimeSeconds());
			}
			if (!bContact)
			{
				UE_LOG(LogTemp, Display, TEXT("BADMINTON_SHOT_REJECTED avatar=%s requestedRally=%.0f"), *ActorInfo->AvatarActor->GetName(), TriggerEventData->EventMagnitude);
			}
		}
	}
	if (ABadmintonCharacter* Character = Cast<ABadmintonCharacter>(ActorInfo->AvatarActor.Get()))
	{
		Character->StartSwingPresentation(ShotType, Match->GetShotData()->Get(ShotType).Recovery);
	}
	// Prototype swing recovery; replace presentation with montage/tasks in the animation stage.
	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, Match->GetShotData()->Get(ShotType).Recovery);
	Wait->OnFinish.AddDynamic(this, &ThisClass::FinishSwing);
	Wait->ReadyForActivation();
}

void UBadmintonClearAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActorInfo)
	{
		Badminton::TraceShot(TEXT("End"), ActorInfo->AbilitySystemComponent.Get(), Cast<APawn>(ActorInfo->AvatarActor.Get()), ShotType, ActivationInfo.GetActivationPredictionKey().Current, bWasCancelled);
		if (ABadmintonCharacter* Character = Cast<ABadmintonCharacter>(ActorInfo->AvatarActor.Get()))
		{
			Character->StopSwingPresentation();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBadmintonClearAbility::FinishSwing()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
