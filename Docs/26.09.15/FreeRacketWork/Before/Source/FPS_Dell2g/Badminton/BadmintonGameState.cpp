#include "BadmintonGameState.h"

#include "Net/UnrealNetwork.h"
#include "BadmintonShotData.h"
#include "UObject/ConstructorHelpers.h"

ABadmintonGameState::ABadmintonGameState()
{
	static ConstructorHelpers::FObjectFinder<UBadmintonShotData> Data(TEXT("/Game/Badminton/Data/DA_BadmintonShots"));
	if (Data.Succeeded()) { ShotData = Data.Object; }
}

void ABadmintonGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABadmintonGameState, Phase);
	DOREPLIFETIME(ABadmintonGameState, ConnectedPlayers);
	DOREPLIFETIME(ABadmintonGameState, RallyId);
	DOREPLIFETIME(ABadmintonGameState, ServingSide);
	DOREPLIFETIME(ABadmintonGameState, CompletedRallies);
	DOREPLIFETIME(ABadmintonGameState, Score0);
	DOREPLIFETIME(ABadmintonGameState, Score1);
	DOREPLIFETIME(ABadmintonGameState, WinnerSide);
	DOREPLIFETIME(ABadmintonGameState, MatchId);
	DOREPLIFETIME(ABadmintonGameState, LastPointReason);
	DOREPLIFETIME(ABadmintonGameState, ShotData);
}

const UBadmintonShotData* ABadmintonGameState::GetShotData() const
{
	return ShotData ? ShotData.Get() : GetDefault<UBadmintonShotData>();
}
