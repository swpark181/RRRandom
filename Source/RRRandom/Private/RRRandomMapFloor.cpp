#include "RRRandomMapFloor.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace
{
	constexpr float MinFloorWidth = 1000.f;
}

UPrimitiveComponent* RRMapFloor::Find(const AActor* MapActor, float SearchUp, float SearchDown, float& OutZ)
{
	UWorld* World = MapActor ? MapActor->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	const FVector Start = MapActor->GetActorLocation() + FVector(0.f, 0.f, SearchUp);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomMapFloor), false, MapActor);
	if (World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, SearchUp + SearchDown), ECC_Visibility, Params) && Hit.GetComponent())
	{
		OutZ = Hit.ImpactPoint.Z;
		return Hit.GetComponent();
	}
	// No physics yet (a level loading in the editor): by bounds
	const FVector Spot = MapActor->GetActorLocation();
	UPrimitiveComponent* Best = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (*It == MapActor)
		{
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive->IsCollisionEnabled() || Primitive->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block)
			{
				continue;
			}
			// A floor is wide (not the player start's capsule, a lamp, ...)
			const FBox Box = Primitive->Bounds.GetBox();
			if (Box.GetSize().X < MinFloorWidth || Box.GetSize().Y < MinFloorWidth || Spot.X < Box.Min.X || Spot.X > Box.Max.X || Spot.Y < Box.Min.Y || Spot.Y > Box.Max.Y
				|| Box.Max.Z > Start.Z || Box.Max.Z < Spot.Z - SearchDown || (Best && Box.Max.Z <= OutZ))
			{
				continue;
			}
			Best = Primitive;
			OutZ = Box.Max.Z;
		}
	}
	return Best;
}
