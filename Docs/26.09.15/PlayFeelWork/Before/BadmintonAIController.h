#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BadmintonTypes.h"
#include "BadmintonAIController.generated.h"

class ABadmintonShuttle;
class UBadmintonShotData;

/** Offline opponent using the same movement, GAS abilities and contact rules as the player. */
UCLASS()
class FPS_DELL2G_API ABadmintonAIController : public AAIController
{
	GENERATED_BODY()
public:
	ABadmintonAIController();
	virtual void Tick(float DeltaSeconds) override;
	static FVector FindReceivePosition(const ABadmintonShuttle* Shuttle, int32 Side, double WorldTime);

private:
	void PlayShot(EBadmintonShot Shot, const FVector2D& Aim);
	UPROPERTY(Transient) TObjectPtr<ABadmintonShuttle> Shuttle;
	UPROPERTY(EditDefaultsOnly, Category="Badminton|AI", meta=(ClampMin="0")) float ReactionSeconds = .18f;
	UPROPERTY(EditDefaultsOnly, Category="Badminton|AI", meta=(ClampMin="0")) float ServeDelay = 1.f;
	int32 ObservedRally = INDEX_NONE;
	int32 ObservedSequence = INDEX_NONE;
	int32 ReturnCount = 0;
	double ReactAt = 0.;
	double NextShotAt = 0.;
	FVector MoveTarget = FVector::ZeroVector;
};
