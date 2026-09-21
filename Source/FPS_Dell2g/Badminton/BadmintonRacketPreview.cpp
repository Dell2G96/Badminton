#include "BadmintonRacketPreview.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABadmintonRacketPreview::ABadmintonRacketPreview()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	SetActorEnableCollision(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> White(TEXT("/Game/Badminton/Materials/M_RacketFeedback.M_RacketFeedback"));
	auto Rod = [&](const FName Name, FVector A, FVector B, float Radius)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(GetRootComponent());
		Mesh->SetStaticMesh(Cylinder.Object);
		Mesh->SetMaterial(0, White.Object);
		Mesh->SetRelativeLocation((A + B) * .5);
		Mesh->SetRelativeRotation(FQuat::FindBetweenNormals(FVector::UpVector, (B - A).GetSafeNormal()));
		Mesh->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, (B - A).Size() / 100.f));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
		// This actor is spawned locally and never replicated.
		Mesh->SetTranslucentSortPriority(100);
	};
	Rod(TEXT("Handle"), FVector(0, 0, -24), FVector(0, 0, -51), 1.1f);
	for (int32 Index = 0; Index < 32; ++Index)
	{
		const float A = Index * 2.f * PI / 32.f, B = (Index + 1) * 2.f * PI / 32.f;
		Rod(*FString::Printf(TEXT("Rim%d"), Index), FVector(0, FMath::Cos(A) * 20.f, FMath::Sin(A) * 24.f),
			FVector(0, FMath::Cos(B) * 20.f, FMath::Sin(B) * 24.f), .7f);
	}
	for (int32 Index = -4; Index <= 4; ++Index)
	{
		const float Fraction = Index / 5.f;
		const float Span = FMath::Sqrt(1.f - Fraction * Fraction);
		Rod(*FString::Printf(TEXT("StringY%d"), Index + 4), FVector(0, Fraction * 20, -Span * 24), FVector(0, Fraction * 20, Span * 24), .20f);
		Rod(*FString::Printf(TEXT("StringZ%d"), Index + 4), FVector(0, -Span * 20, Fraction * 24), FVector(0, Span * 20, Fraction * 24), .20f);
	}
}

void ABadmintonRacketPreview::SetContactReady(bool bReady)
{
	if (!RimMaterial)
	{
		TArray<UStaticMeshComponent*> Parts;
		GetComponents(Parts);
		for (auto* Part : Parts)
		{
			if (!Part->GetName().StartsWith(TEXT("Rim")))
			{
				if (!BodyMaterial)
				{
					BodyMaterial = UMaterialInstanceDynamic::Create(Part->GetMaterial(0), this);
					BodyMaterial->SetScalarParameterValue(TEXT("RacketOpacity"), .055f);
				}
				Part->SetMaterial(0, BodyMaterial);
				continue;
			}
			if (!RimMaterial) { RimMaterial = UMaterialInstanceDynamic::Create(Part->GetMaterial(0), this); }
			Part->SetMaterial(0, RimMaterial);
		}
	}
	if (RimMaterial)
	{
		RimMaterial->SetVectorParameterValue(TEXT("RacketColor"), bReady ? FLinearColor(.15f, 1.f, .55f) : FLinearColor::White);
		RimMaterial->SetScalarParameterValue(TEXT("RacketOpacity"), bReady ? .26f : .12f);
	}
}
