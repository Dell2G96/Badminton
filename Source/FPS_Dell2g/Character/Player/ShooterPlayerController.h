// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class FPS_DELL2G_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AShooterPlayerController();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	
private:
	void Input_Crouch();
	void Input_Jump();
	void Input_Move(const struct FInputActionValue& InputActionValue);
	void Input_Look(const struct FInputActionValue& InputActionValue);
	
	
private:
	
	UPROPERTY(EditAnywhere, Category= "FPS|Input")
	TObjectPtr<class UInputMappingContext> ShooterIMC;
	
	UPROPERTY(EditAnywhere, Category= "FPS|Input")
	TObjectPtr<class UInputAction> MoveAction;
	
	UPROPERTY(EditAnywhere, Category= "FPS|Input")
	TObjectPtr<class UInputAction> LookAction;
	
	UPROPERTY(EditAnywhere, Category= "FPS|Input")
	TObjectPtr<class UInputAction> CrouchAction;
	
	UPROPERTY(EditAnywhere, Category= "FPS|Input")
	TObjectPtr<class UInputAction> JumpAction;
	
};
