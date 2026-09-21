#include "BadmintonThirdPersonAbility.h"

UBadmintonThirdPersonClearAbility::UBadmintonThirdPersonClearAbility()
{
	// Timing is accepted on the server; GAS then starts the owning client presentation.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

UBadmintonThirdPersonServeAbility::UBadmintonThirdPersonServeAbility()
{
	SetShotTrigger(EBadmintonShot::Serve, TAG_BadmintonServeEvent);
}

UBadmintonThirdPersonDropAbility::UBadmintonThirdPersonDropAbility()
{
	SetShotTrigger(EBadmintonShot::Drop, TAG_BadmintonDropEvent);
}

UBadmintonThirdPersonSmashAbility::UBadmintonThirdPersonSmashAbility()
{
	SetShotTrigger(EBadmintonShot::Smash, TAG_BadmintonSmashEvent);
}

UBadmintonThirdPersonReceiveAbility::UBadmintonThirdPersonReceiveAbility()
{
	SetShotTrigger(EBadmintonShot::Receive, TAG_BadmintonReceiveEvent);
}

UBadmintonThirdPersonHairpinAbility::UBadmintonThirdPersonHairpinAbility()
{
	SetShotTrigger(EBadmintonShot::Hairpin, TAG_BadmintonHairpinEvent);
}
