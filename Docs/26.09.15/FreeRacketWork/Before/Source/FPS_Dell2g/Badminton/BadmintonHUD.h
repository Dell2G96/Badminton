#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BadmintonHUD.generated.h"

class ABadmintonGameState;
class ABadmintonShuttle;

UCLASS()
class FPS_DELL2G_API ABadmintonHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void Panel(float X, float Y, float Width, float Height, const FLinearColor& Color);
	void Label(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float MaxWidth = 0.f, bool bCentered = false);
	float UIScale = 1.f;
	void DrawShuttleTracking(const ABadmintonGameState* Match);
	void DrawAimPreview();
	TWeakObjectPtr<ABadmintonShuttle> TrackedShuttle;
	TArray<FVector> ShuttleTrail;
	int32 TrailRallyId = INDEX_NONE;
	int32 TrailSequence = INDEX_NONE;
	double LastTrailSampleTime = 0;
};
