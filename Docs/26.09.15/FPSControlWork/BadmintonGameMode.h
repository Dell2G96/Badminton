#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BadmintonTypes.h"
#include "BadmintonGameMode.generated.h"

class ABadmintonCharacter;
class ABadmintonShuttle;
class ABadmintonAIController;

UCLASS()
class FPS_DELL2G_API ABadmintonGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABadmintonGameMode();
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	void RefreshLobby();
	bool TryBasicShot(ABadmintonCharacter* Player, int32 RequestedRallyId);
	bool TryShot(ABadmintonCharacter* Player, int32 RequestedRallyId, EBadmintonShot Shot, const FVector2D& Aim = FVector2D::ZeroVector);
	void FinishPracticeRally(const FVector& Position, int32 RallyId, int32 LastHitterSide);

private:
	void PreparePracticeServe();
	void StartMatch();
	void ResetPlayerPositions();
	void SpawnSoloOpponent();
	UPROPERTY() TObjectPtr<ABadmintonAIController> SoloOpponent;
	UPROPERTY()
	TObjectPtr<ABadmintonShuttle> Shuttle;
	FTimerHandle ServeResetTimer;
};
