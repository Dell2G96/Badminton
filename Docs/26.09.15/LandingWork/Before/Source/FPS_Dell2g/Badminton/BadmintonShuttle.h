#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BadmintonTypes.h"
#include "BadmintonShuttle.generated.h"

class USphereComponent;
class UStaticMeshComponent;

USTRUCT()
struct FBadmintonFlightState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize100 Position = FVector::ZeroVector;
	UPROPERTY()
	FVector_NetQuantize100 Velocity = FVector::ZeroVector;
	UPROPERTY()
	double ServerTime = 0;
	UPROPERTY()
	int32 ShotSequence = 0;
	UPROPERTY()
	int32 RallyId = 0;
	UPROPERTY()
	int32 LastHitterSide = INDEX_NONE;
	UPROPERTY()
	bool bFlying = false;
	UPROPERTY() bool bLegalCrossing = false;
	UPROPERTY() EBadmintonShot Shot = EBadmintonShot::Clear;
	UPROPERTY() FVector_NetQuantize100 Aim = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize100 Target = FVector::ZeroVector;
};

/** Initial server flight model. Position presentation is independent from hit authority. */
UCLASS()
class FPS_DELL2G_API ABadmintonShuttle : public AActor
{
	GENERATED_BODY()

public:
	ABadmintonShuttle();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void Launch(const FVector& Position, const FVector& Target, int32 HitterSide, int32 RallyId, float Duration = 1.7f, EBadmintonShot Shot = EBadmintonShot::Clear, const FVector2D& Aim = FVector2D::ZeroVector);
	void ResetForServe(const FVector& Position, int32 RallyId);
	const FBadmintonFlightState& GetFlight() const { return Flight; }
	static FVector SolveVelocity(const FVector& Start, const FVector& Target, float FlightTime);
	static void AdvanceFlight(FVector& Position, FVector& Velocity, float DeltaTime);

private:
	void PublishState();
	void UpdateVisualOrientation(const FVector& Velocity);
	void TracePosition() const;
	uint32 TraceSegment = 0;
	UFUNCTION()
	void OnRep_Flight();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(ReplicatedUsing=OnRep_Flight)
	FBadmintonFlightState Flight;

	FVector SimVelocity = FVector::ZeroVector;
	float PublishAccumulator = 0.f;
	float FlightAge = 0.f;
	float MetricsAccumulator = 0.f;
	double FlightReceivedAt = 0.;
	int32 PresentedSequence = INDEX_NONE;
	int32 PresentedRally = INDEX_NONE;
};
