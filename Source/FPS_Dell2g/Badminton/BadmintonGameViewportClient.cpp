#include "BadmintonGameViewportClient.h"

#include "BadmintonPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

void UBadmintonGameViewportClient::LayoutPlayers()
{
	Super::LayoutPlayers();
	if (!GetGameInstance() || GetGameInstance()->GetNumLocalPlayers() != 1) { return; }
	ULocalPlayer* Local = GetGameInstance()->GetFirstGamePlayer();
	const auto* Controller = Local ? Cast<ABadmintonPlayerController>(Local->PlayerController) : nullptr;
	if (Controller && Controller->IsDualViewEnabled())
	{
		Local->Origin = FVector2D(.5, 0.);
		Local->Size = FVector2D(.5, 1.);
	}
}
