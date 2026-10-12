#include "RRRandomFogOfWar.h"
#include "RRRandomCharacter.h"
#include "RRRandomCover.h"
#include "RRRandomGameState.h"
#include "RRRandomIsland.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// How far above and below the viewer to look for the floor
	constexpr float FloorSearchUp = 500.f;
	constexpr float FloorSearchDown = 3000.f;
	const FName FogTextureParameter(TEXT("FogTex"));
	const FName FogOriginParameter(TEXT("FogOrigin"));
	const FName FogSizeParameter(TEXT("FogSize"));

	UMaterialInterface* FogMaterialAsset = nullptr;
}

ARRRandomFogOfWar::ARRRandomFogOfWar()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the brawlers have moved this frame
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Fog(TEXT("/Game/PostProcess/M_PP_Fog.M_PP_Fog"));
	if (Fog.Succeeded())
	{
		FogMaterialAsset = Fog.Object;
	}
}

void ARRRandomFogOfWar::BeginPlay()
{
	Super::BeginPlay();
}

void ARRRandomFogOfWar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Give hidden opponents back their looks, and take the fog off the screen
	ShowAll();
	if (Volume)
	{
		Volume->Destroy();
		Volume = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool ARRRandomFogOfWar::Setup(const FVector& Near)
{
	UWorld* World = GetWorld();
	if (!FogMaterialAsset || Resolution < 2)
	{
		return false;
	}

	// The floor under the viewer, all of it
	FHitResult Hit;
	const FVector Start = Near + FVector(0.f, 0.f, FloorSearchUp);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomFog), false);
	Params.AddIgnoredActor(Cast<AActor>(GetOwner()));
	// The arena floor itself: brawlers, cover and the island's slabs and hills stand on it
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->IsA<ARRRandomCharacter>() || It->IsA<ARRRandomCover>() || It->IsA<ARRRandomIsland>())
		{
			Params.AddIgnoredActor(*It);
		}
	}
	if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, FloorSearchUp + FloorSearchDown), ECC_Visibility, Params) || !Hit.GetComponent())
	{
		return false;
	}
	const FBox Floor = Hit.GetComponent()->Bounds.GetBox();
	Size = FMath::Max(Floor.Max.X - Floor.Min.X, Floor.Max.Y - Floor.Min.Y);
	Origin = FVector2D(Floor.GetCenter().X - Size * 0.5f, Floor.GetCenter().Y - Size * 0.5f);

	Visible.Init(0.f, Resolution * Resolution);
	Explored.Init(0.f, Resolution * Resolution);

	FogTexture = UTexture2D::CreateTransient(Resolution, Resolution, PF_B8G8R8A8);
	// The material samples it as a plain color texture, like its default (the engine's black)
	FogTexture->SRGB = true;
	FogTexture->Filter = TF_Bilinear;
	FogTexture->AddressX = TA_Clamp;
	FogTexture->AddressY = TA_Clamp;
	FogTexture->UpdateResource();

	FogMaterial = UMaterialInstanceDynamic::Create(FogMaterialAsset, this);
	FogMaterial->SetTextureParameterValue(FogTextureParameter, FogTexture);
	FogMaterial->SetVectorParameterValue(FogOriginParameter, FLinearColor(Origin.X, Origin.Y, 0.f, 0.f));
	FogMaterial->SetScalarParameterValue(FogSizeParameter, Size);

	// A post process volume covering the whole world, on this machine only
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	Volume = World->SpawnActor<APostProcessVolume>(SpawnParams);
	if (Volume)
	{
		Volume->SetReplicates(false);
		Volume->bUnbound = true;
		Volume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, FogMaterial));
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: fog of war %d x %d over %.0f units from (%.0f, %.0f)."), Resolution, Resolution, Size, Origin.X, Origin.Y);
	return true;
}

void ARRRandomFogOfWar::Stamp(const ARRRandomCharacter& Brawler)
{
	const float CellSize = Size / Resolution;
	const FVector Location = Brawler.GetActorLocation();
	const float CenterX = (Location.X - Origin.X) / CellSize;
	const float CenterY = (Location.Y - Origin.Y) / CellSize;
	// The soft edge is centered on the radius, so half-seen is exactly where ARRRandomCharacter::IsSeenByTeam stops
	const float Reach = Brawler.GetVisionRadius() + EdgeSoftness * 0.5f;
	const float RadiusCells = Reach / CellSize;

	// How far the brawler sees in each direction: anything taller than half of it cuts the circle short and shades behind it
	const FVector Eye = Brawler.GetEyeLocation();
	const int32 Rays = FMath::Max(8, SightRays);
	SightRayLengths.SetNum(Rays);
	for (int32 Ray = 0; Ray < Rays; ++Ray)
	{
		const float Angle = 2.f * PI * Ray / Rays;
		SightRayLengths[Ray] = ARRRandomCharacter::GetSightDistance(GetWorld(), Eye, FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f), Reach);
	}
	auto SightAlong = [&](float DX, float DY)
	{
		// Between the two nearest rays
		const float Turn = FMath::Fmod(FMath::Atan2(DY, DX) + 2.f * PI, 2.f * PI) / (2.f * PI) * Rays;
		const int32 Low = FMath::FloorToInt(Turn) % Rays;
		return FMath::Lerp(SightRayLengths[Low], SightRayLengths[(Low + 1) % Rays], Turn - FMath::FloorToFloat(Turn));
	};
	const int32 MinX = FMath::Max(0, FMath::FloorToInt(CenterX - RadiusCells));
	const int32 MaxX = FMath::Min(Resolution - 1, FMath::CeilToInt(CenterX + RadiusCells));
	const int32 MinY = FMath::Max(0, FMath::FloorToInt(CenterY - RadiusCells));
	const int32 MaxY = FMath::Min(Resolution - 1, FMath::CeilToInt(CenterY + RadiusCells));
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			// Distance from the cell's middle; full sight inside, fading over the soft edge, and dark past whatever blocks
			// the view that way (a cell into the obstacle still counts, so its face shows)
			const float DX = (X + 0.5f - CenterX) * CellSize;
			const float DY = (Y + 0.5f - CenterY) * CellSize;
			const float Distance = FMath::Sqrt(DX * DX + DY * DY);
			const float Open = FMath::Clamp((SightAlong(DX, DY) + CellSize - Distance) / CellSize, 0.f, 1.f);
			const float Sight = FMath::Min(Open, FMath::Clamp((Reach - Distance) / FMath::Max(EdgeSoftness, 1.f), 0.f, 1.f));
			float& Cell = Visible[Y * Resolution + X];
			Cell = FMath::Max(Cell, Sight);
		}
	}
}


float ARRRandomFogOfWar::GetSightAt(const FVector& WorldLocation, bool* OutExplored) const
{
	if (!bReady || Size <= 0.f || ARRRandomGameState::IsMatchOver(this))
	{
		if (OutExplored)
		{
			*OutExplored = true;
		}
		return 1.f;
	}
	const int32 X = FMath::Clamp(FMath::FloorToInt((WorldLocation.X - Origin.X) / Size * Resolution), 0, Resolution - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt((WorldLocation.Y - Origin.Y) / Size * Resolution), 0, Resolution - 1);
	const int32 Index = Y * Resolution + X;
	if (OutExplored)
	{
		*OutExplored = Explored.IsValidIndex(Index) && Explored[Index] > 0.f;
	}
	return Visible.IsValidIndex(Index) ? Visible[Index] : 0.f;
}

void ARRRandomFogOfWar::Upload()
{
	// Red: seen now, green: ever seen. The render thread frees the copy when it is done with it.
	const int32 Count = Resolution * Resolution;
	FColor* Pixels = new FColor[Count];
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Pixels[Index] = FColor(FMath::RoundToInt(Visible[Index] * 255.f), FMath::RoundToInt(Explored[Index] * 255.f), 0, 255);
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Resolution, Resolution);
	FogTexture->UpdateTextureRegions(0, 1, Region, Resolution * sizeof(FColor), sizeof(FColor), reinterpret_cast<uint8*>(Pixels),
		[](uint8* Data, const FUpdateTextureRegion2D* Regions)
		{
			delete[] reinterpret_cast<FColor*>(Data);
			delete Regions;
		});
}

void ARRRandomFogOfWar::HideBrawler(ARRRandomCharacter* Brawler)
{
	// Hidden-in-game on the components is local; the actor's own hidden flag would replicate from a host
	TArray<TWeakObjectPtr<UPrimitiveComponent>>& Parts = Hidden.FindOrAdd(Brawler);
	TInlineComponentArray<UPrimitiveComponent*> All(Brawler);
	for (UPrimitiveComponent* Part : All)
	{
		if (!Part->bHiddenInGame)
		{
			Part->SetHiddenInGame(true);
			Parts.Add(Part);
		}
	}
}

void ARRRandomFogOfWar::ShowBrawler(ARRRandomCharacter* Brawler)
{
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Parts;
	if (Hidden.RemoveAndCopyValue(Brawler, Parts))
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Part : Parts)
		{
			if (Part.IsValid())
			{
				Part->SetHiddenInGame(false);
			}
		}
	}
}

void ARRRandomFogOfWar::ShowAll()
{
	for (const TPair<TWeakObjectPtr<ARRRandomCharacter>, TArray<TWeakObjectPtr<UPrimitiveComponent>>>& Entry : Hidden)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Part : Entry.Value)
		{
			if (Part.IsValid())
			{
				Part->SetHiddenInGame(false);
			}
		}
	}
	Hidden.Reset();
}

void ARRRandomFogOfWar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The results: everyone on show (the MVP may be an opponent) and the fog fading away
	if (ARRRandomGameState::IsMatchOver(this))
	{
		ShowAll();
		if (Volume)
		{
			Volume->BlendWeight = FMath::FInterpConstantTo(Volume->BlendWeight, 0.f, DeltaSeconds, ResultsFadeSpeed);
		}
		return;
	}

	const APlayerController* Viewer = Cast<APlayerController>(GetOwner());
	const ARRRandomCharacter* Self = Viewer ? Cast<ARRRandomCharacter>(Viewer->GetPawn()) : nullptr;
	if (!Self)
	{
		return;
	}
	if (!bReady)
	{
		bReady = Setup(Self->GetActorLocation());
		if (!bReady)
		{
			return;
		}
	}

	// What the team sees right now, and what it has ever seen
	const int32 Team = Self->GetTeam();
	FMemory::Memzero(Visible.GetData(), Visible.Num() * sizeof(float));
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() == Team && It->IsAlive())
		{
			Stamp(**It);
		}
	}
	for (int32 Index = 0; Index < Visible.Num(); ++Index)
	{
		Explored[Index] = FMath::Max(Explored[Index], Visible[Index]);
	}
	Upload();

	// Opponents show only inside the team's sight
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Brawler = *It;
		if (Brawler->GetTeam() == Team)
		{
			continue;
		}
		// The same test the bots use, so nobody sees more on screen than the bots act on (or the other way round)
		const bool bShow = ARRRandomCharacter::IsSeenByTeam(GetWorld(), Team, Brawler->GetActorLocation());
		const bool bWasHidden = Hidden.Contains(Brawler);
		if (bShow && bWasHidden)
		{
			ShowBrawler(Brawler);
		}
		else if (!bShow && !bWasHidden)
		{
			HideBrawler(Brawler);
		}
	}
}
