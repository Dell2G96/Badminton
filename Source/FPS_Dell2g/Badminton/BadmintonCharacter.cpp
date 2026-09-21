#include "BadmintonCharacter.h"

#include "AbilitySystemComponent.h"
#include "BadmintonPlayerState.h"
#include "BadmintonPlayerController.h"
#include "BadmintonClearAbility.h"
#include "BadmintonThirdPersonAbility.h"
#include "BadmintonDashAbility.h"
#include "BadmintonGameState.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABadmintonCharacter::ABadmintonCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	GetCapsuleComponent()->InitCapsuleSize(30.f, 90.f);
	GetCharacterMovement()->MaxWalkSpeed = 450.f;
	GetCharacterMovement()->MaxAcceleration = 2200.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2400.f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	bUseControllerRotationYaw = false;
	PrototypeBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeBody"));
	PrototypeBody->SetupAttachment(GetRootComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Body(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	PrototypeBody->SetStaticMesh(Body.Object);
	PrototypeBody->SetRelativeScale3D(FVector(.5f, .5f, 1.7f));
	PrototypeBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeBody->SetOwnerNoSee(true);

	// Cosmetic geometry only. Racket movement never participates in hit validation.
	RacketPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RacketPivot"));
	RacketPivot->SetupAttachment(PrototypeBody);
	RacketPivot->SetAbsolute(false, false, true);
	RacketPivot->SetRelativeLocation(FVector(35, 60, 15));
	RacketPivot->SetRelativeRotation(FRotator(-20, 15, 0));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto AddRod = [&](FName Name, const FVector& Start, const FVector& End, float Radius)
	{
		UStaticMeshComponent* Rod = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Rod->SetupAttachment(RacketPivot);
		Rod->SetStaticMesh(Body.Object);
		Rod->SetRelativeLocation((Start + End) * .5f);
		Rod->SetRelativeRotation(FQuat::FindBetweenNormals(FVector::UpVector, (End - Start).GetSafeNormal()));
		Rod->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, (End - Start).Size() / 100.f));
		Rod->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rod->SetGenerateOverlapEvents(false);
		Rod->SetOwnerNoSee(true);
	};
	AddRod(TEXT("RacketHandle"), FVector::ZeroVector, FVector(43, 0, 0), 2.f);
	for (int32 Index = 0; Index < 16; ++Index)
	{
		const float A = Index * 2.f * PI / 16.f;
		const float B = (Index + 1) * 2.f * PI / 16.f;
		AddRod(*FString::Printf(TEXT("RacketFrame%d"), Index),
			FVector(65 + 22 * FMath::Cos(A), 17 * FMath::Sin(A), 0),
			FVector(65 + 22 * FMath::Cos(B), 17 * FMath::Sin(B), 0), 1.5f);
	}
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		for (int32 Index = -2; Index <= 2; ++Index)
		{
			const float Fraction = Index / 3.f;
			const float Span = FMath::Sqrt(1.f - Fraction * Fraction);
			UStaticMeshComponent* String = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("RacketString%d_%d"), Axis, Index + 2));
			String->SetupAttachment(RacketPivot);
			String->SetStaticMesh(Cube.Object);
			String->SetRelativeLocation(Axis == 0 ? FVector(65, 17 * Fraction, 0) : FVector(65 + 22 * Fraction, 0, 0));
			String->SetRelativeScale3D((Axis == 0 ? FVector(44 * Span, .55f, .55f) : FVector(.55f, 34 * Span, .55f)) / 100.f);
			String->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			String->SetGenerateOverlapEvents(false);
			String->SetCastShadow(false);
			String->SetOwnerNoSee(true);
		}
	}
}

void ABadmintonCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ABadmintonCharacter, SwingPresentation, COND_SkipOwner);
}

void ABadmintonCharacter::StartSwingPresentation(EBadmintonShot Shot, float Duration)
{
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	if (!Match || (!HasAuthority() && !IsLocallyControlled())) { return; }
	if (HasAuthority())
	{
		++SwingPresentation.Sequence;
		SwingPresentation.RallyId = Match->RallyId;
		SwingPresentation.ServerTime = Match->GetServerWorldTimeSeconds();
		SwingPresentation.Duration = Duration;
		SwingPresentation.Shot = Shot;
		ForceNetUpdate();
	}
	BeginSwing(Shot, Duration, 0.f, Match->RallyId);
}

void ABadmintonCharacter::BeginSwing(EBadmintonShot Shot, float Duration, float Age, int32 RallyId)
{
	bSwinging = true;
	SwingAge = FMath::Max(0.f, Age);
	SwingDuration = FMath::Max(.05f, Duration);
	SwingRallyId = RallyId;
	PresentedShot = Shot;
	++PresentedSwingCount;
}

void ABadmintonCharacter::OnRep_SwingPresentation()
{
	if (IsLocallyControlled() || SwingPresentation.Sequence <= LastPresentedSequence) { return; }
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	// The character and GameState arrive on separate actor channels. Wait for a newer rally.
	if (!Match || Match->RallyId < SwingPresentation.RallyId) { return; }
	LastPresentedSequence = SwingPresentation.Sequence;
	if (Match->RallyId != SwingPresentation.RallyId) { return; }
	const float Age = FMath::Max(0.f, static_cast<float>(Match->GetServerWorldTimeSeconds() - SwingPresentation.ServerTime));
	if (Age < SwingPresentation.Duration)
	{
		BeginSwing(SwingPresentation.Shot, SwingPresentation.Duration, Age, SwingPresentation.RallyId);
	}
}

void ABadmintonCharacter::StopSwingPresentation()
{
	bSwinging = false;
	RacketPivot->SetRelativeRotation(FRotator(-20, 15, 0));
}

void ABadmintonCharacter::Tick(float DeltaSeconds)
{
	const auto* PlayerController = Cast<ABadmintonPlayerController>(GetController());
	const bool bShowBody = IsLocallyControlled() && PlayerController && PlayerController->UsesThirdPersonControl();
	if (bShowBody != bThirdPersonBodyVisible)
	{
		bThirdPersonBodyVisible = bShowBody;
		TArray<UStaticMeshComponent*> Parts;
		GetComponents(Parts);
		for (auto* Part : Parts) { Part->SetOwnerNoSee(!bShowBody); }
	}
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() && !IsLocallyControlled() && SwingPresentation.Sequence > LastPresentedSequence)
	{
		OnRep_SwingPresentation();
	}
	if (!bSwinging) { return; }
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	SwingAge += DeltaSeconds;
	if (!Match || Match->RallyId != SwingRallyId || SwingAge >= SwingDuration
		|| (Match->Phase != EBadmintonPhase::ReadyToServe && Match->Phase != EBadmintonPhase::Rally))
	{
		StopSwingPresentation();
		return;
	}
	const float Alpha = FMath::Clamp(SwingAge / SwingDuration, 0.f, 1.f);
	const FRotator Rest(-20, 15, 0);
	const FRotator Start = PresentedShot == EBadmintonShot::Smash ? FRotator(65, 35, 0)
		: PresentedShot == EBadmintonShot::Serve ? FRotator(-45, 30, 0) : FRotator(10, 65, 0);
	const FRotator End = PresentedShot == EBadmintonShot::Smash ? FRotator(-55, -30, 0)
		: PresentedShot == EBadmintonShot::Drop ? FRotator(0, -10, 0) : FRotator(30, -65, 0);
	const FQuat Rotation = Alpha < .3f ? FQuat::Slerp(Start.Quaternion(), End.Quaternion(), Alpha / .3f)
		: FQuat::Slerp(End.Quaternion(), Rest.Quaternion(), (Alpha - .3f) / .7f);
	RacketPivot->SetRelativeRotation(Rotation);
}

UAbilitySystemComponent* ABadmintonCharacter::GetAbilitySystemComponent() const
{
	const ABadmintonPlayerState* State = GetPlayerState<ABadmintonPlayerState>();
	return State ? State->GetAbilitySystemComponent() : nullptr;
}

void ABadmintonCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void ABadmintonCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem();
}

void ABadmintonCharacter::UnPossessed()
{
	StopSwingPresentation();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent(); ASC && ASC->GetAvatarActor() == this)
	{
		ASC->CancelAllAbilities();
		ASC->ClearActorInfo();
	}
	Super::UnPossessed();
}

void ABadmintonCharacter::InitializeAbilitySystem()
{
	// OwnerNoSee hides the body only in the owning first-person view.
	// Keep geometry visible to the independent third-person capture.
	PrototypeBody->SetVisibility(true, true);
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->InitAbilityActorInfo(GetPlayerState(), this);
		if (HasAuthority())
		{
			TArray<TSubclassOf<UGameplayAbility>> Classes = { UBadmintonClearAbility::StaticClass(), UBadmintonServeAbility::StaticClass(),
				UBadmintonDropAbility::StaticClass(), UBadmintonSmashAbility::StaticClass(), UBadmintonReceiveAbility::StaticClass(), UBadmintonHairpinAbility::StaticClass(), UBadmintonDashAbility::StaticClass() };
			const auto* PlayerController = Cast<ABadmintonPlayerController>(GetController());
			if (PlayerController && PlayerController->UsesThirdPersonControl())
			{
				Classes = { UBadmintonThirdPersonClearAbility::StaticClass(), UBadmintonThirdPersonServeAbility::StaticClass(),
					UBadmintonThirdPersonDropAbility::StaticClass(), UBadmintonThirdPersonSmashAbility::StaticClass(),
					UBadmintonThirdPersonReceiveAbility::StaticClass(), UBadmintonThirdPersonHairpinAbility::StaticClass(), UBadmintonDashAbility::StaticClass() };
			}
			for (const TSubclassOf<UGameplayAbility> Ability : Classes)
			{
				if (!ASC->FindAbilitySpecFromClass(Ability)) { ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1)); }
			}
		}
	}
}
