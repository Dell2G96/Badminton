#include "BadmintonShuttle.h"

#include "BadmintonGameMode.h"
#include "BadmintonNetMetrics.h"
#include "BadmintonPresentation.h"
#include "BadmintonPlayerController.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float Drag = .15f;
	const FVector Gravity(0, 0, -980.f);
}

ABadmintonShuttle::ABadmintonShuttle()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.f);
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(5.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	SetRootComponent(Collision);
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShuttleMesh(TEXT("/Game/Badminton/Models/Gameplay/SM_Badminton_Shuttle.SM_Badminton_Shuttle"));
	Visual->SetStaticMesh(ShuttleMesh.Object);
	Visual->SetRelativeScale3D(FVector(.14f));
	UpdateVisualOrientation(FVector::DownVector);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABadmintonShuttle::UpdateVisualOrientation(const FVector& Velocity)
{
	if (!Visual || Velocity.IsNearlyZero()) { return; }
	// Source model's cork points down (-Z). Keep its centre on the unchanged collision sphere.
	const FQuat Rotation = FQuat::FindBetweenNormals(FVector::DownVector, Velocity.GetSafeNormal());
	Visual->SetRelativeLocationAndRotation(Rotation.RotateVector(FVector(0.f, 0.f, 35.f * .14f)), Rotation);
}

void ABadmintonShuttle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABadmintonShuttle, Flight);
}

FVector ABadmintonShuttle::SolveVelocity(const FVector& Start, const FVector& Target, float FlightTime)
{
	FlightTime = FMath::Max(.1f, FlightTime);
	const float A = (1.f - FMath::Exp(-Drag * FlightTime)) / Drag;
	return (Target - Start - (Gravity / Drag) * (FlightTime - A)) / A;
}

void ABadmintonShuttle::AdvanceFlight(FVector& Position, FVector& Velocity, float DeltaTime)
{
	const float Decay = FMath::Exp(-Drag * DeltaTime);
	const float A = (1.f - Decay) / Drag;
	Position += Velocity * A + (Gravity / Drag) * (DeltaTime - A);
	Velocity = Velocity * Decay + (Gravity / Drag) * (1.f - Decay);
}

bool ABadmintonShuttle::PredictLanding(UWorld* World, FVector Position, FVector Velocity, int32 HitterSide, bool bLegalCrossing, FVector& OutLanding, const AActor* IgnoreActor)
{
	OutLanding = FVector::ZeroVector;
	if (!World || Position.ContainsNaN() || Velocity.ContainsNaN()) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BadmintonLanding), false, IgnoreActor);
	FCollisionResponseParams Response(ECR_Ignore);
	Response.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(5.f);
	for (int32 Step = 0; Step < 960; ++Step)
	{
		const FVector Previous = Position;
		FVector Next = Position;
		AdvanceFlight(Next, Velocity, 1.f / 120.f);
		FHitResult Hit;
		World->SweepSingleByChannel(Hit, Position, Next, FQuat::Identity, ECC_WorldDynamic, Shape, Query, Response);
		Position = Hit.bBlockingHit ? Hit.Location : Next;
		bool bInvalidCrossing = false;
		if (!bLegalCrossing && Previous.X * Badminton::ForwardSign(HitterSide) <= 0.f
			&& Position.X * Badminton::ForwardSign(HitterSide) > 0.f)
		{
			const float Alpha = FMath::Clamp(-Previous.X / (Position.X - Previous.X), 0.f, 1.f);
			const FVector Crossing = FMath::Lerp(Previous, Position, Alpha);
			bLegalCrossing = Crossing.Z >= 155.f && FMath::Abs(Crossing.Y) <= 305.f;
			bInvalidCrossing = !bLegalCrossing;
		}
		if (Hit.bBlockingHit)
		{
			if (Hit.Normal.Z > .5f)
			{
				OutLanding = FVector(Position.X, Position.Y, Hit.ImpactPoint.Z);
				return true;
			}
			Velocity = FVector(Hit.Normal.X * 35.f, Velocity.Y * .2f, -60.f);
			Position += Hit.Normal;
		}
		// Match server termination: an invalid crossing or leaving the floor has no future ground contact.
		if (bInvalidCrossing || Position.Z < -30.f) { return false; }
	}
	return false;
}

void ABadmintonShuttle::RefreshLanding()
{
	FVector Landing = FVector::ZeroVector;
	Flight.bHasLanding = Flight.bFlying && PredictLanding(GetWorld(), GetActorLocation(), SimVelocity,
		Flight.LastHitterSide, Flight.bLegalCrossing, Landing, this);
	Flight.Landing = Landing;
}

void ABadmintonShuttle::ResetForServe(const FVector& Position, int32 RallyId)
{
	if (!HasAuthority())
	{
		return;
	}
	Flight.bFlying = false;
	Flight.bHasLanding = false;
	++TraceSegment;
	Flight.bLegalCrossing = false;
	Flight.RallyId = RallyId;
	Flight.LastHitterSide = INDEX_NONE;
	SimVelocity = FVector::ZeroVector;
	SetActorLocation(Position);
	UpdateVisualOrientation(FVector::DownVector);
	PublishState();
}

void ABadmintonShuttle::Launch(const FVector& Position, const FVector& Target, int32 HitterSide, int32 RallyId, float Duration, EBadmintonShot Shot, const FVector2D& Aim, const FVector* LaunchVelocity)
{
	if (!HasAuthority())
	{
		return;
	}
	SetActorLocation(Position);
	SimVelocity = LaunchVelocity ? *LaunchVelocity : SolveVelocity(Position, Target, Duration);
	UpdateVisualOrientation(SimVelocity);
	++TraceSegment;
	Flight.Shot = Shot;
	Flight.Aim = FVector(Aim.X, Aim.Y, 0);
	Flight.Target = Target;
	Flight.bLegalCrossing = false;
	Flight.bFlying = true;
	Flight.LastHitterSide = HitterSide;
	Flight.RallyId = RallyId;
	++Flight.ShotSequence;
	FlightAge = 0.f;
	RefreshLanding();
	PublishState();
}

void ABadmintonShuttle::PublishState()
{
	Flight.Position = GetActorLocation();
	Flight.Velocity = SimVelocity;
	Flight.ServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	ForceNetUpdate();
}

void ABadmintonShuttle::OnRep_Flight()
{
	FlightReceivedAt = GetWorld()->GetTimeSeconds();
	UpdateVisualOrientation(Flight.bFlying ? FVector(Flight.Velocity) : FVector::DownVector);
	if (PresentedSequence != Flight.ShotSequence || PresentedRally != Flight.RallyId || !Flight.bFlying)
	{
		if (Badminton::NetMetricsEnabled() && PresentedRally != INDEX_NONE)
		{
			UE_LOG(LogTemp, Display, TEXT("BADMINTON_METRIC_RESET rally=%d sequence=%d flying=%d snap_cm=%.3f"),
				Flight.RallyId, Flight.ShotSequence, Flight.bFlying, FVector::Dist(GetActorLocation(), Flight.Position));
		}
		SetActorLocation(Flight.Position);
		PresentedSequence = Flight.ShotSequence;
		PresentedRally = Flight.RallyId;
	}
}

void ABadmintonShuttle::TracePosition() const
{
#if PLATFORM_WINDOWS && !UE_BUILD_SHIPPING
	static const FString Run = []()
	{
		FString Value;
		FGuid Guid;
		return FParse::Value(FCommandLine::Get(), TEXT("BadmintonTraceRun="), Value) && FGuid::Parse(Value, Guid)
			? Guid.ToString(EGuidFormats::Digits) : FString();
	}();
	if (Run.IsEmpty()) { return; }
	static bool bAnnounced = false;
	if (!bAnnounced)
	{
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_TRACE_BEGIN run=%s clock=windows_qpc machine=%s model=%s"), *Run, FPlatformProcess::ComputerName(),
			Badminton::UseRoundTripClock() ? TEXT("RoundTrip") : TEXT("Legacy"));
		bAnnounced = true;
	}
	const FVector Position = GetActorLocation();
	const double Time = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_TRACE_POS authority=%d t=%.9f rally=%d sequence=%d flying=%d segment=%u x=%.3f y=%.3f z=%.3f"),
		HasAuthority(), Time, Flight.RallyId, Flight.ShotSequence, Flight.bFlying, TraceSegment, Position.X, Position.Y, Position.Z);
#endif
}

void ABadmintonShuttle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		if (Flight.bFlying && GetWorld()->GetGameState())
		{
			const double EngineServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
			double PresentationServerTime = EngineServerTime;
			double ClockRTT = -1.;
			const ABadmintonPlayerController* LocalController = Cast<ABadmintonPlayerController>(GetWorld()->GetFirstPlayerController());
			const bool bClockSynced = LocalController && LocalController->GetShuttleServerTime(PresentationServerTime, ClockRTT);
			const Badminton::FPresentationAge Age = Badminton::GetPresentationAge(
				PresentationServerTime, Flight.ServerTime,
				GetWorld()->GetTimeSeconds(), FlightReceivedAt);
			FVector Target = Flight.Position;
			FVector Velocity = Flight.Velocity;
			AdvanceFlight(Target, Velocity, Age.Seconds);
			UpdateVisualOrientation(Velocity);
			const FVector Before = GetActorLocation();
			SetActorLocation(FMath::VInterpTo(Before, Target, DeltaSeconds, 25.f));
			if (Badminton::NetMetricsEnabled())
			{
				MetricsAccumulator += DeltaSeconds;
				if (MetricsAccumulator >= .1f)
				{
					MetricsAccumulator = 0.f;
					const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
					const APlayerState* Player = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
					const float Ping = Player ? Player->GetPingInMilliseconds() : 0.f;
					UE_LOG(LogTemp, Display, TEXT("BADMINTON_METRIC_FLIGHT rally=%d sequence=%d age_ms=%.3f gap_cm=%.3f step_cm=%.3f residual_cm=%.3f dt_ms=%.3f engine_ping_ms=%.3f capped=%d receipt_ms=%.3f present_ms=%.3f floor=%d sync_age_ms=%.3f clock_rtt_ms=%.3f clock_synced=%d"),
						Flight.RallyId, Flight.ShotSequence, (EngineServerTime - Flight.ServerTime) * 1000., FVector::Dist(Before, Target),
						FVector::Dist(Before, GetActorLocation()), FVector::Dist(GetActorLocation(), Target), DeltaSeconds * 1000.f,
						Ping > 0.f ? Ping : -1.f, Age.Requested > .15, Age.SinceReceipt * 1000.,
						Age.Seconds * 1000.f, Age.Raw < Age.SinceReceipt, Age.Raw * 1000., ClockRTT * 1000., bClockSynced);
				}
			}
		}
		TracePosition();
		return;
	}
	if (!Flight.bFlying)
	{
		TracePosition();
		return;
	}
	FlightAge += DeltaSeconds;
	bool bLandingChanged = false;
	float Remaining = FMath::Min(DeltaSeconds, .25f);
	while (Remaining > SMALL_NUMBER && Flight.bFlying)
	{
		const float Step = FMath::Min(Remaining, 1.f / 120.f);
		Remaining -= Step;
		const FVector Previous = GetActorLocation();
		FVector Next = Previous;
		AdvanceFlight(Next, SimVelocity, Step);
		FHitResult Hit;
		SetActorLocation(Next, true, &Hit);
		const FVector Current = GetActorLocation();
		if (!Flight.bLegalCrossing && Previous.X * Badminton::ForwardSign(Flight.LastHitterSide) <= 0.f
			&& Current.X * Badminton::ForwardSign(Flight.LastHitterSide) > 0.f)
		{
			const float Alpha = FMath::Clamp(-Previous.X / (Current.X - Previous.X), 0.f, 1.f);
			const FVector Crossing = FMath::Lerp(Previous, Current, Alpha);
			Flight.bLegalCrossing = Crossing.Z >= 155.f && FMath::Abs(Crossing.Y) <= 305.f;
			if (!Flight.bLegalCrossing) { Flight.bFlying = false; SimVelocity = FVector::ZeroVector; }
		}
		if (Hit.bBlockingHit)
		{
			++TraceSegment;
			if (Hit.Normal.Z > .5f)
			{
#if !UE_BUILD_SHIPPING
				if (FParse::Param(FCommandLine::Get(), TEXT("BadmintonLandingTest")))
				{
					const float Error = FVector::Dist2D(GetActorLocation(), Flight.Landing);
					UE_LOG(LogTemp, Display, TEXT("BADMINTON_LANDING_CHECK %s shot=%d error_cm=%.3f predicted=%s actual=%s"),
						Flight.bHasLanding && Error < 2.f ? TEXT("PASS") : TEXT("FAIL"), static_cast<int32>(Flight.Shot), Error,
						*FVector(Flight.Landing).ToString(), *GetActorLocation().ToString());
				}
#endif
				Flight.bFlying = false;
				SimVelocity = FVector::ZeroVector;
			}
			else
			{
				// A net touch is not an immediate fault: let the shuttle fall on its resulting side.
				SimVelocity = FVector(Hit.Normal.X * 35.f, SimVelocity.Y * .2f, -60.f);
				SetActorLocation(GetActorLocation() + Hit.Normal);
				bLandingChanged = true;
			}
		}
		if (GetActorLocation().Z < -30.f || FlightAge > 8.f)
		{
			Flight.bFlying = false;
		}
	}
	if (bLandingChanged) { RefreshLanding(); PublishAccumulator = 1.f; }
	if (!Flight.bFlying) { Flight.bHasLanding = false; }
	UpdateVisualOrientation(SimVelocity);
	PublishAccumulator += DeltaSeconds;
	if (PublishAccumulator >= 1.f / 30.f || !Flight.bFlying)
	{
		PublishAccumulator = 0.f;
		PublishState();
	}
	if (!Flight.bFlying)
	{
		if (ABadmintonGameMode* Mode = GetWorld()->GetAuthGameMode<ABadmintonGameMode>())
		{
			Mode->FinishPracticeRally(GetActorLocation(), Flight.RallyId, Flight.LastHitterSide);
		}
	}
	TracePosition();
}
