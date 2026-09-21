#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FPS_Dell2g/Interface/PlayerInterface.h"
#include "GameFramework/Character.h"
#include "ShooterCharacter.generated.h"

UCLASS()
class FPS_DELL2G_API AShooterCharacter : public ACharacter , public IPlayerInterface
{
	GENERATED_BODY()

public:
	AShooterCharacter();
	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	
	//PlayerInterface
	virtual FName GetWeaponAttachPoint_Implementation(const FGameplayTag& WeaponType) const override;
	virtual USkeletalMeshComponent* GetMesh1P_Implementation() const override;
	virtual USkeletalMeshComponent* GetMesh3P_Implementation() const override;
	

protected:
	virtual void BeginPlay() override;

private:
	
	void Input_CycleWeapon();
	void Input_ReloadWeapon();
	void Input_FireWeapon_Pressed();
	void Input_Firweapon_Released();
	void Input_Aim_Pressed();
	void Input_Aim_Released();
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UCombatComponent> Combat;
	
	// 1인칭
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class USkeletalMeshComponent> Mesh1P;
	
	// 3인칭
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class USkeletalMeshComponent> Mesh3P;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class USpringArmComponent> SpringArm;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UCameraComponent> FirstPersonCamera;
	
	UPROPERTY(EditAnywhere, Category="Dell2g|Input")
	TObjectPtr<class UInputAction> CycleWeaponAction;
	
	UPROPERTY(EditAnywhere, Category="Dell2g|Input")
	TObjectPtr<class UInputAction> FireWeaponAction;
	
	UPROPERTY(EditAnywhere, Category="Dell2g|Input")
	TObjectPtr<class UInputAction> ReloadWeaponAction;
	
	UPROPERTY(EditAnywhere, Category="Dell2g|Input")
	TObjectPtr<class UInputAction> AimWeaponAction;
};