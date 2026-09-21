#include "BadmintonPlayerState.h"

#include "AbilitySystemComponent.h"
#include "BadmintonAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "BadmintonGameState.h"
#include "BadmintonShotData.h"
#include "BadmintonStaminaEffects.h"

ABadmintonPlayerState::ABadmintonPlayerState()
{
	SetNetUpdateFrequency(30.f);
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	Attributes = CreateDefaultSubobject<UBadmintonAttributeSet>(TEXT("Attributes"));
}

UAbilitySystemComponent* ABadmintonPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ABadmintonPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
		const UBadmintonShotData* Data = Match ? Match->GetShotData() : GetDefault<UBadmintonShotData>();
		FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UBadmintonStaminaRegenEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(TAG_BadmintonStaminaCost, Data->StaminaPerSecond * .25f);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void ABadmintonPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABadmintonPlayerState, CourtSide);
	DOREPLIFETIME(ABadmintonPlayerState, bReady);
}
