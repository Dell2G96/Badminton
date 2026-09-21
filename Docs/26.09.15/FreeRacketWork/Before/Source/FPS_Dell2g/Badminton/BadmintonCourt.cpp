#include "BadmintonCourt.h"

#include "BadmintonTypes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABadmintonCourt::ABadmintonCourt()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	CourtRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CourtRoot"));
	SetRootComponent(CourtRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(TEXT("/Game/Badminton/Prototype/M_Court_PlayingSurface.M_Court_PlayingSurface"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lines(TEXT("/Game/Badminton/Prototype/M_Court_Lines.M_Court_Lines"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NetMesh(TEXT("/Game/Badminton/Models/Gameplay/SM_Badminton_Net_Detailed.SM_Badminton_Net_Detailed"));

	auto AddBox = [&](const FName Name, const FVector& Location, const FVector& Size, UMaterialInterface* Material, const bool bCollide)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(CourtRoot);
		Part->SetStaticMesh(Cube.Object);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Size / 100.f);
		Part->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		if (Material)
		{
			Part->SetMaterial(0, Material);
		}
		return Part;
	};

	AddBox(TEXT("Floor"), FVector(0, 0, -10), FVector(1500, 800, 20), Surface.Object, true);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const float Sign = Index == 0 ? -1.f : 1.f;
		AddBox(*FString::Printf(TEXT("SideLine%d"), Index), FVector(0, Sign * Badminton::HalfWidth, 1), FVector(1340, 4, 2), Lines.Object, false);
		AddBox(*FString::Printf(TEXT("EndLine%d"), Index), FVector(Sign * Badminton::HalfLength, 0, 1), FVector(4, 518, 2), Lines.Object, false);
		AddBox(*FString::Printf(TEXT("ServiceLine%d"), Index), FVector(Sign * 198, 0, 1), FVector(4, 518, 2), Lines.Object, false);
		AddBox(*FString::Printf(TEXT("CenterLine%d"), Index), FVector(Sign * 434, 0, 1), FVector(472, 4, 2), Lines.Object, false);
		AddBox(*FString::Printf(TEXT("NetPost%d"), Index), FVector(0, Sign * 305, 77.5), FVector(8, 8, 155), Lines.Object, true)->SetVisibility(false);
	}
	UStaticMeshComponent* DetailedNet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DetailedNet"));
	DetailedNet->SetupAttachment(CourtRoot);
	DetailedNet->SetStaticMesh(NetMesh.Object);
	// The imported grid/posts are visual only; retain the established net and player collision below.
	DetailedNet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetailedNet->SetGenerateOverlapEvents(false);

	auto AddBoundary = [&](const FName Name, const FVector& Location, const FVector& Extent)
	{
		UBoxComponent* Boundary = CreateDefaultSubobject<UBoxComponent>(Name);
		Boundary->SetupAttachment(CourtRoot);
		Boundary->SetRelativeLocation(Location);
		Boundary->SetBoxExtent(Extent);
		Boundary->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Boundary->SetCollisionResponseToAllChannels(ECR_Ignore);
		Boundary->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	};
	AddBoundary(TEXT("NetPlayerBoundary"), FVector(0, 0, 200), FVector(5, 400, 200));
	UBoxComponent* NetCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("ShuttleNetCollision"));
	NetCollision->SetupAttachment(CourtRoot);
	NetCollision->SetRelativeLocation(FVector(0, 0, 115));
	NetCollision->SetBoxExtent(FVector(2, 305, 39));
	NetCollision->SetCollisionObjectType(ECC_WorldStatic);
	NetCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NetCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	NetCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	AddBoundary(TEXT("LeftBoundary"), FVector(0, -Badminton::HalfWidth - 30, 200), FVector(750, 10, 200));
	AddBoundary(TEXT("RightBoundary"), FVector(0, Badminton::HalfWidth + 30, 200), FVector(750, 10, 200));
	AddBoundary(TEXT("NearBoundary"), FVector(-Badminton::HalfLength - 30, 0, 200), FVector(10, 400, 200));
	AddBoundary(TEXT("FarBoundary"), FVector(Badminton::HalfLength + 30, 0, 200), FVector(10, 400, 200));
}
