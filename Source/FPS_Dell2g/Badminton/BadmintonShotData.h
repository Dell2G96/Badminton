#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BadmintonTypes.h"
#include "BadmintonShotData.generated.h"

struct FGameplayAbilityTargetDataHandle;

USTRUCT(BlueprintType)
struct FBadmintonShotParameters
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FlightTime = 1.7f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TargetDistance = 440.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TargetHeight = 160.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinimumHeight = 65.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaximumHeight = 360.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Reach = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Recovery = .35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaminaCost = 0.f;
};

UCLASS(BlueprintType)
class FPS_DELL2G_API UBadmintonShotData : public UDataAsset
{
	GENERATED_BODY()
public:
	UBadmintonShotData();
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBadmintonShotParameters Serve;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBadmintonShotParameters Clear;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBadmintonShotParameters Drop;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBadmintonShotParameters Smash;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBadmintonShotParameters Receive;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBadmintonShotParameters Hairpin;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DashCost = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DashSpeed = 950.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DashDuration = .18f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DashRecovery = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaminaPerSecond = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aim", meta=(ClampMin="0", ClampMax="219")) float AimWidth = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aim", meta=(ClampMin="0", ClampMax="200")) float AimDepth = 80.f;
	const FBadmintonShotParameters& Get(EBadmintonShot Shot) const;
	bool ResolveAimTarget(EBadmintonShot Shot, int32 Side, int32 Score, const FVector2D& Aim, FVector& Target) const;
	static bool ReadAim(const FGameplayAbilityTargetDataHandle& TargetData, FVector2D& Aim);
};
