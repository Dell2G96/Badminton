#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "BadmintonTypes.h"
#include "BadmintonCharacter.generated.h"

class UStaticMeshComponent;

USTRUCT()
struct FBadmintonSwingPresentation
{
	GENERATED_BODY()
	UPROPERTY() int32 Sequence = 0;
	UPROPERTY() int32 RallyId = INDEX_NONE;
	UPROPERTY() double ServerTime = 0;
	UPROPERTY() float Duration = .35f;
	UPROPERTY() EBadmintonShot Shot = EBadmintonShot::Clear;
};

UCLASS()
class FPS_DELL2G_API ABadmintonCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABadmintonCharacter();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void UnPossessed() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void StartSwingPresentation(EBadmintonShot Shot, float Duration);
	void StopSwingPresentation();
	int32 GetPresentedSwingCount() const { return PresentedSwingCount; }

private:
	void InitializeAbilitySystem();
	void BeginSwing(EBadmintonShot Shot, float Duration, float Age, int32 RallyId);
	UFUNCTION() void OnRep_SwingPresentation();
	UPROPERTY(ReplicatedUsing=OnRep_SwingPresentation)
	FBadmintonSwingPresentation SwingPresentation;
	UPROPERTY(VisibleAnywhere, Category="Badminton")
	TObjectPtr<USceneComponent> RacketPivot;
	bool bSwinging = false;
	float SwingAge = 0.f;
	float SwingDuration = .35f;
	EBadmintonShot PresentedShot = EBadmintonShot::Clear;
	int32 SwingRallyId = INDEX_NONE;
	int32 LastPresentedSequence = 0;
	int32 PresentedSwingCount = 0;

	UPROPERTY(VisibleAnywhere, Category="Badminton")
	TObjectPtr<UStaticMeshComponent> PrototypeBody;
};
