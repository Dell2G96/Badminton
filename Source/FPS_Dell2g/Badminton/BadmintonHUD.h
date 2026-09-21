#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BadmintonHUD.generated.h"

class ABadmintonGameState;
class ABadmintonShuttle;
class UFont;

UCLASS()
class FPS_DELL2G_API ABadmintonHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UFont> KoreanFont;
	void DrawThirdPersonHUD(const ABadmintonGameState* Match);
	void DrawDualViewHUD(const ABadmintonGameState* Match);
	bool ProjectFirstPerson(const FVector& Position, FVector2D& Screen) const;
	void Panel(float X, float Y, float Width, float Height, const FLinearColor& Color);
	void Label(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float MaxWidth = 0.f, bool bCentered = false);
	float UIScale = 1.f;
	bool DrawOnlineLobby();
	void DrawShuttleTracking(const ABadmintonGameState* Match);
	void DrawAimPreview();
	void DrawMinimap(const ABadmintonGameState* Match, int32 LocalSide, float Width);
	bool bMinimapTestReported = false;
	bool bDirectionPreviewTestReported = false;
	bool bDualViewTestReported = false;
	TWeakObjectPtr<ABadmintonShuttle> TrackedShuttle;
	TArray<FVector> ShuttleTrail;
	int32 TrailRallyId = INDEX_NONE;
	int32 TrailSequence = INDEX_NONE;
	double LastTrailSampleTime = 0;
};
