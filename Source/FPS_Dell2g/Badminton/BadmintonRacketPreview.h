#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BadmintonRacketPreview.generated.h"

/** Local, translucent first-person racket. Contact is checked separately by the game rules. */
UCLASS()
class FPS_DELL2G_API ABadmintonRacketPreview : public AActor
{
	GENERATED_BODY()
public:
	ABadmintonRacketPreview();
	void SetContactReady(bool bReady);
private:
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> RimMaterial;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> BodyMaterial;
};
