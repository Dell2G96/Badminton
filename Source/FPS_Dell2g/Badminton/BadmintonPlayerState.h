#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "BadmintonPlayerState.generated.h"

class UAbilitySystemComponent;
class UBadmintonAttributeSet;

UCLASS()
class FPS_DELL2G_API ABadmintonPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	ABadmintonPlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UBadmintonAttributeSet* GetAttributes() const { return Attributes; }

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton")
	int32 CourtSide = INDEX_NONE;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton")
	bool bReady = false;

private:
	UPROPERTY(VisibleAnywhere, Category="Badminton")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UBadmintonAttributeSet> Attributes;
};
