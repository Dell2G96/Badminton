#pragma once

#include "BadmintonClearAbility.h"
#include "BadmintonThirdPersonAbility.generated.h"

UCLASS()
class FPS_DELL2G_API UBadmintonThirdPersonClearAbility : public UBadmintonClearAbility
{
	GENERATED_BODY()
public:
	UBadmintonThirdPersonClearAbility();
};

UCLASS()
class FPS_DELL2G_API UBadmintonThirdPersonServeAbility : public UBadmintonThirdPersonClearAbility
{
	GENERATED_BODY()
public:
	UBadmintonThirdPersonServeAbility();
};

UCLASS()
class FPS_DELL2G_API UBadmintonThirdPersonDropAbility : public UBadmintonThirdPersonClearAbility
{
	GENERATED_BODY()
public:
	UBadmintonThirdPersonDropAbility();
};

UCLASS()
class FPS_DELL2G_API UBadmintonThirdPersonSmashAbility : public UBadmintonThirdPersonClearAbility
{
	GENERATED_BODY()
public:
	UBadmintonThirdPersonSmashAbility();
};

UCLASS()
class FPS_DELL2G_API UBadmintonThirdPersonReceiveAbility : public UBadmintonThirdPersonClearAbility
{
	GENERATED_BODY()
public:
	UBadmintonThirdPersonReceiveAbility();
};

UCLASS()
class FPS_DELL2G_API UBadmintonThirdPersonHairpinAbility : public UBadmintonThirdPersonClearAbility
{
	GENERATED_BODY()
public:
	UBadmintonThirdPersonHairpinAbility();
};
