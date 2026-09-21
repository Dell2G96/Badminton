#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BadmintonTypes.h"
#include "BadmintonGameState.generated.h"

class UBadmintonShotData;

UCLASS()
class FPS_DELL2G_API ABadmintonGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABadmintonGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton")
	EBadmintonPhase Phase = EBadmintonPhase::WaitingForPlayers;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton")
	int32 ConnectedPlayers = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton")
	int32 RallyId = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton")
	int32 ServingSide = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton")
	int32 CompletedRallies = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton") int32 Score0 = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton") int32 Score1 = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton") int32 WinnerSide = INDEX_NONE;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton") int32 MatchId = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Badminton") EBadmintonPointReason LastPointReason = EBadmintonPointReason::None;
	UPROPERTY(Replicated, EditDefaultsOnly, Category="Badminton") TObjectPtr<UBadmintonShotData> ShotData;
	int32 GetScore(int32 Side) const { return Side == 0 ? Score0 : Score1; }
	const UBadmintonShotData* GetShotData() const;
};
