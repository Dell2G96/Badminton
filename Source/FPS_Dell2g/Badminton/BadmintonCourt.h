#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BadmintonCourt.generated.h"

UCLASS()
class FPS_DELL2G_API ABadmintonCourt : public AActor
{
	GENERATED_BODY()

public:
	ABadmintonCourt();

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> CourtRoot;
};
