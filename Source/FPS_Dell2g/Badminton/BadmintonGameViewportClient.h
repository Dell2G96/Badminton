#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "BadmintonGameViewportClient.generated.h"

UCLASS()
class FPS_DELL2G_API UBadmintonGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
public:
	virtual void LayoutPlayers() override;
};
