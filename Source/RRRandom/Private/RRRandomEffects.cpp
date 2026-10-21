#include "RRRandomEffects.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// The meshes per shape (the order of EShape), and how many of each can be alive at once
	const TCHAR* const ShapeMeshes[] =
	{
		TEXT("/Game/Effects/DiceFX/SM_FXBall.SM_FXBall"),
		TEXT("/Game/Effects/DiceFX/SM_FXRing.SM_FXRing"),
		TEXT("/Game/Effects/Wings/SM_WingSparkle.SM_WingSparkle"),
		TEXT("/Game/Effects/DiceFX/SM_FXDie.SM_FXDie"),
	};
	// (Balls: the dragon's breath and the fire and embers it leaves take many at once)
	// (Balls and stars: also the shooting-star tails of falling ore, a dozen or more at once when a brawler drops a pile)
	constexpr int32 PoolSizes[] = { 3400, 100, 900, 120 };
	const TCHAR* const GlowMaterial = TEXT("/Game/Effects/DiceFX/M_FXGlow.M_FXGlow");
	// Custom data: linear color, brightness, opacity
	constexpr int32 DataFloats = 5;
	// Brightness over 1 is squeezed by this much before it reaches the material: far over 1, glows burn out to white
	// against the bright pastel sky and lose their color
	constexpr float GlowSqueeze = 0.3f;
	// Particles this close to the camera (past their own radius) fade out, so a burst round the player's own brawler
	// doesn't fill the screen; never below this share
	constexpr float NearFadeDistance = 500.f;
	constexpr float NearFadeFloor = 0.1f;

	const FLinearColor Cream(1.f, 0.93f, 0.82f);
	const FLinearColor Gold(1.f, 0.78f, 0.3f);
	// Fire, hottest first
	const FLinearColor FireYellow(1.f, 0.82f, 0.25f);
	const FLinearColor FireOrange(1.f, 0.42f, 0.05f);
	const FLinearColor FireRed(0.95f, 0.13f, 0.03f);
	const FLinearColor Candy[] = { FLinearColor(1.f, 0.45f, 0.62f), FLinearColor(1.f, 0.62f, 0.3f), FLinearColor(1.f, 0.88f, 0.3f),
		FLinearColor(0.35f, 0.9f, 0.6f), FLinearColor(0.35f, 0.75f, 1.f), FLinearColor(0.62f, 0.5f, 1.f) };

	/** Eases out: fast at first, settling at the end. */
	float EaseOut(float Alpha)
	{
		return 1.f - FMath::Square(1.f - FMath::Clamp(Alpha, 0.f, 1.f));
	}

	/** How many of something to make this frame at Rate per second: the whole part, and the rest by chance. */
	int32 CountThisFrame(float Rate, float DeltaSeconds)
	{
		const float Expected = Rate * DeltaSeconds;
		return FMath::FloorToInt(Expected) + (FMath::FRand() < FMath::Frac(Expected) ? 1 : 0);
	}
}

ARRRandomEffects::ARRRandomEffects()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
	// Each machine makes its own and plays the same effects from the replicated state
	bReplicates = false;
}

ARRRandomEffects* ARRRandomEffects::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || !World->IsGameWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ARRRandomEffects> It(World); It; ++It)
	{
		return *It;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	ARRRandomEffects* Effects = World->SpawnActor<ARRRandomEffects>(ARRRandomEffects::StaticClass(), FTransform::Identity, Params);
	if (!Effects)
	{
		return nullptr;
	}
	// The pools: every particle slot made up front, hidden (no size) until used
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, GlowMaterial);
	Effects->FreeSlots.SetNum(static_cast<int32>(EShape::Count));
	for (int32 Shape = 0; Shape < static_cast<int32>(EShape::Count); ++Shape)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, ShapeMeshes[Shape]);
		UInstancedStaticMeshComponent* Pool = nullptr;
		if (Mesh)
		{
			Pool = NewObject<UInstancedStaticMeshComponent>(Effects);
			Pool->SetStaticMesh(Mesh);
			for (int32 Slot = 0; Glow && Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				Pool->SetMaterial(Slot, Glow);
			}
			Pool->NumCustomDataFloats = DataFloats;
			Pool->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Pool->SetGenerateOverlapEvents(false);
			Pool->SetCanEverAffectNavigation(false);
			Pool->CastShadow = false;
			Pool->SetMobility(EComponentMobility::Movable);
			Pool->SetupAttachment(Effects->RootComponent);
			Pool->SetUsingAbsoluteLocation(true);
			Pool->SetUsingAbsoluteRotation(true);
			Pool->SetUsingAbsoluteScale(true);
			Pool->RegisterComponent();
			Pool->SetWorldTransform(FTransform::Identity);
			TArray<FTransform> Hidden;
			Hidden.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), PoolSizes[Shape]);
			Pool->AddInstances(Hidden, false);
			for (int32 Slot = PoolSizes[Shape] - 1; Slot >= 0; --Slot)
			{
				Effects->FreeSlots[Shape].Add(Slot);
			}
		}
		Effects->Pools.Add(Pool);
	}
	return Effects;
}

void ARRRandomEffects::Spawn(const FParticle& Particle)
{
	const int32 Shape = static_cast<int32>(Particle.Shape);
	if (!Pools.IsValidIndex(Shape) || !Pools[Shape] || FreeSlots[Shape].Num() == 0)
	{
		return;
	}
	FParticle& Added = Live.Add_GetRef(Particle);
	Added.Slot = FreeSlots[Shape].Pop(EAllowShrinking::No);
}

void ARRRandomEffects::Spray(EShape Shape, const FVector& Where, int32 Count, const FVector& Direction, float Cone, FVector2D Speed, FVector2D Size, FVector2D Life,
	const FLinearColor& Color, float Glow, float Gravity)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FParticle Particle;
		Particle.Shape = Shape;
		Particle.Location = Where;
		Particle.Velocity = FMath::VRandCone(Direction, FMath::DegreesToRadians(Cone)) * FMath::FRandRange(Speed.X, Speed.Y);
		Particle.Gravity = Gravity;
		Particle.Drag = 1.5f;
		Particle.Life = FMath::FRandRange(Life.X, Life.Y);
		const float Start = FMath::FRandRange(Size.X, Size.Y);
		Particle.StartSize = FVector(Start);
		Particle.EndSize = FVector(Start * 0.2f);
		Particle.Turn = FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f));
		Particle.Spin = FRotator(FMath::FRandRange(-360.f, 360.f), FMath::FRandRange(-360.f, 360.f), 0.f);
		Particle.Color = Color;
		Particle.Glow = Glow;
		Particle.bFacesCamera = Shape == EShape::Star;
		Spawn(Particle);
	}
}

void ARRRandomEffects::GiantBurst(const FVector& Feet, float Size, const FLinearColor& Tint)
{
	// Two shockwaves running out along the ground, the second tinted and a little behind
	FParticle Wave;
	Wave.Shape = EShape::Ring;
	Wave.Location = Feet + FVector(0.f, 0.f, 12.f);
	Wave.Life = 0.7f;
	Wave.StartSize = FVector(1.5f, 1.5f, 3.f);
	Wave.EndSize = FVector(7.f * Size, 7.f * Size, 2.f);
	Wave.Color = Gold;
	Wave.Glow = 6.f;
	Wave.Opacity = 1.f;
	Spawn(Wave);
	Wave.Life = 0.55f;
	Wave.StartSize = FVector(1.f, 1.f, 2.f);
	Wave.EndSize = FVector(4.5f * Size, 4.5f * Size, 1.5f);
	Wave.Color = Tint;
	Wave.Glow = 5.f;
	Spawn(Wave);

	// A ring of dust puffs rolling outward and lifting
	for (int32 Index = 0; Index < 18; ++Index)
	{
		const FVector Out = FVector(1.f, 0.f, 0.f).RotateAngleAxis(Index * 20.f + FMath::FRandRange(-8.f, 8.f), FVector::UpVector);
		FParticle Puff;
		Puff.Shape = EShape::Ball;
		Puff.Location = Feet + Out * 40.f * Size + FVector(0.f, 0.f, 20.f);
		Puff.Velocity = Out * FMath::FRandRange(300.f, 600.f) * Size * 0.5f + FVector(0.f, 0.f, FMath::FRandRange(40.f, 120.f));
		Puff.Drag = 2.5f;
		Puff.Gravity = -40.f;
		Puff.Life = FMath::FRandRange(0.8f, 1.2f);
		const float Start = FMath::FRandRange(0.45f, 0.75f) * Size * 0.5f;
		Puff.StartSize = FVector(Start, Start, Start * 0.7f);
		Puff.EndSize = FVector(Start * 1.8f, Start * 1.8f, Start * 1.1f);
		Puff.Color = Cream;
		Puff.Glow = 1.2f;
		Puff.Opacity = 0.4f;
		Puff.FadeIn = 0.08f;
		Spawn(Puff);
	}

	// Stars and candy dice thrown up, falling back
	const FVector Chest = Feet + FVector(0.f, 0.f, 90.f * Size);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Spray(EShape::Star, Chest, 10, FVector::UpVector, 55.f, FVector2D(500.f, 1100.f), FVector2D(6.f, 11.f), FVector2D(0.9f, 1.4f), Candy[Index + 1], 7.f, 900.f);
	}
	for (int32 Index = 0; Index < 14; ++Index)
	{
		Spray(EShape::Die, Chest, 1, FVector::UpVector, 50.f, FVector2D(600.f, 1000.f), FVector2D(0.25f, 0.4f), FVector2D(1.2f, 1.6f),
			Candy[Index % UE_ARRAY_COUNT(Candy)], 2.5f, 1400.f);
	}

	// A flash at the chest and a column of light
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Chest;
	Flash.Life = 0.3f;
	Flash.StartSize = FVector(1.f);
	Flash.EndSize = FVector(1.6f * Size);
	Flash.Color = Gold;
	Flash.Glow = 6.f;
	Flash.Opacity = 0.4f;
	Flash.FadeIn = 0.02f;
	Spawn(Flash);
	FParticle Column = Flash;
	Column.Location = Feet + FVector(0.f, 0.f, 250.f * Size);
	Column.Life = 0.5f;
	Column.StartSize = FVector(0.25f, 0.25f, 6.f) * Size;
	Column.EndSize = FVector(0.7f, 0.7f, 9.f) * Size;
	Column.Color = FMath::Lerp(Tint, Gold, 0.5f);
	Column.Glow = 4.f;
	Column.Opacity = 0.3f;
	Spawn(Column);
}

void ARRRandomEffects::GiantShrink(const FVector& Feet, float Size)
{
	FParticle Wave;
	Wave.Shape = EShape::Ring;
	Wave.Location = Feet + FVector(0.f, 0.f, 10.f);
	Wave.Life = 0.45f;
	Wave.StartSize = FVector(1.f, 1.f, 2.f);
	Wave.EndSize = FVector(3.f * Size, 3.f * Size, 1.f);
	Wave.Color = Cream;
	Wave.Glow = 2.f;
	Wave.Opacity = 0.7f;
	Spawn(Wave);
	for (int32 Index = 0; Index < 12; ++Index)
	{
		FParticle Puff;
		Puff.Shape = EShape::Ball;
		Puff.Location = Feet + FMath::VRand() * 60.f * Size + FVector(0.f, 0.f, 90.f * Size);
		Puff.Velocity = FMath::VRand() * 150.f;
		Puff.Drag = 2.f;
		Puff.Gravity = -30.f;
		Puff.Life = FMath::FRandRange(0.6f, 0.9f);
		Puff.StartSize = FVector(0.3f);
		Puff.EndSize = FVector(0.7f);
		Puff.Color = Cream;
		Puff.Glow = 1.f;
		Puff.Opacity = 0.3f;
		Spawn(Puff);
	}
	Spray(EShape::Star, Feet + FVector(0.f, 0.f, 100.f), 10, FVector::UpVector, 80.f, FVector2D(200.f, 500.f), FVector2D(2.5f, 4.f), FVector2D(0.6f, 0.9f), Gold, 3.f, 400.f);
}

void ARRRandomEffects::GiantAura(const FVector& Feet, float Size, float Height, const FLinearColor& Tint, float DeltaSeconds)
{
	// A ring swelling out under the feet about twice a second
	if (FMath::FRand() < DeltaSeconds * 2.2f)
	{
		FParticle Ring;
		Ring.Shape = EShape::Ring;
		Ring.Location = Feet + FVector(0.f, 0.f, 8.f);
		Ring.Life = 0.7f;
		Ring.StartSize = FVector(0.9f * Size, 0.9f * Size, 2.f);
		Ring.EndSize = FVector(2.4f * Size, 2.4f * Size, 1.f);
		Ring.Color = FMath::Lerp(Tint, Gold, 0.45f);
		Ring.Glow = 4.f;
		Ring.Opacity = 0.85f;
		Ring.FadeIn = 0.15f;
		Spawn(Ring);
	}
	// Motes of light rising round the body, and a star now and then
	for (int32 Index = CountThisFrame(16.f, DeltaSeconds); Index > 0; --Index)
	{
		const FVector Side = FVector(1.f, 0.f, 0.f).RotateAngleAxis(FMath::FRandRange(0.f, 360.f), FVector::UpVector);
		FParticle Mote;
		Mote.Shape = EShape::Ball;
		Mote.Location = Feet + Side * FMath::FRandRange(35.f, 60.f) * Size + FVector(0.f, 0.f, FMath::FRandRange(0.f, Height * 0.6f));
		Mote.Velocity = FVector(0.f, 0.f, FMath::FRandRange(80.f, 170.f)) + Side * 20.f;
		Mote.Life = FMath::FRandRange(0.9f, 1.5f);
		const float Start = FMath::FRandRange(0.1f, 0.18f) * Size * 0.6f;
		Mote.StartSize = FVector(Start);
		Mote.EndSize = FVector(Start * 0.3f);
		Mote.Color = FMath::Lerp(Tint, Gold, 0.6f);
		Mote.Glow = 6.f;
		Mote.Opacity = 0.9f;
		Mote.FadeIn = 0.2f;
		Spawn(Mote);
	}
	if (FMath::FRand() < DeltaSeconds * 5.f)
	{
		Spray(EShape::Star, Feet + FVector(0.f, 0.f, FMath::FRandRange(0.4f, 1.f) * Height), 1, FVector::UpVector, 90.f, FVector2D(40.f, 120.f),
			FVector2D(4.f, 7.f), FVector2D(0.6f, 1.f), Gold, 7.f, -50.f);
	}
}

void ARRRandomEffects::WingsBurst(const FVector& Back, const FLinearColor& Tint)
{
	FParticle Ring;
	Ring.Shape = EShape::Ring;
	Ring.Location = Back;
	Ring.Life = 0.45f;
	Ring.StartSize = FVector(0.6f, 0.6f, 2.f);
	Ring.EndSize = FVector(4.5f, 4.5f, 1.f);
	Ring.Color = FLinearColor(0.55f, 0.85f, 1.f);
	Ring.Glow = 6.f;
	Ring.Opacity = 1.f;
	Ring.bFacesCamera = true;
	Spawn(Ring);
	FParticle Flat = Ring;
	Flat.bFacesCamera = false;
	Flat.EndSize = FVector(3.5f, 3.5f, 1.f);
	Flat.Color = FMath::Lerp(Tint, Gold, 0.4f);
	Spawn(Flat);
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Back;
	Flash.Life = 0.25f;
	Flash.StartSize = FVector(0.5f);
	Flash.EndSize = FVector(2.5f);
	Flash.Color = FLinearColor(0.7f, 0.9f, 1.f);
	Flash.Glow = 6.f;
	Flash.Opacity = 0.7f;
	Flash.FadeIn = 0.02f;
	Spawn(Flash);
	Spray(EShape::Star, Back, 18, FVector::UpVector, 100.f, FVector2D(300.f, 700.f), FVector2D(5.f, 9.f), FVector2D(0.6f, 1.f), FLinearColor(0.55f, 0.85f, 1.f), 7.f, 250.f);
	Spray(EShape::Star, Back, 14, FVector::UpVector, 100.f, FVector2D(300.f, 700.f), FVector2D(5.f, 9.f), FVector2D(0.6f, 1.f), Gold, 7.f, 250.f);
}

void ARRRandomEffects::FlightTrail(const FVector& Back, const FVector& Velocity, const FLinearColor& Tint, float DeltaSeconds)
{
	// Stars and soft glows left behind, more the faster the flight
	const float Speed = Velocity.Size();
	for (int32 Index = CountThisFrame(18.f + Speed * 0.04f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Star;
		Star.Shape = EShape::Star;
		Star.Location = Back + FMath::VRand() * FMath::FRandRange(0.f, 45.f);
		Star.Velocity = -Velocity * 0.12f + FMath::VRand() * 40.f;
		Star.Gravity = 60.f;
		Star.Drag = 1.f;
		Star.Life = FMath::FRandRange(0.6f, 1.f);
		const float Start = FMath::FRandRange(3.5f, 6.5f);
		Star.StartSize = FVector(Start);
		Star.EndSize = FVector(Start * 0.2f);
		Star.Spin = FRotator(0.f, 0.f, FMath::FRandRange(-200.f, 200.f));
		Star.Color = FMath::FRand() < 0.5f ? FLinearColor(0.55f, 0.85f, 1.f) : Gold;
		Star.Glow = 6.f;
		Star.bFacesCamera = true;
		Spawn(Star);
	}
	for (int32 Index = CountThisFrame(10.f + Speed * 0.02f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Glow;
		Glow.Shape = EShape::Ball;
		Glow.Location = Back + FMath::VRand() * FMath::FRandRange(0.f, 30.f);
		Glow.Velocity = -Velocity * 0.05f;
		Glow.Life = FMath::FRandRange(0.4f, 0.6f);
		Glow.StartSize = FVector(FMath::FRandRange(0.22f, 0.32f));
		Glow.EndSize = FVector(0.04f);
		Glow.Color = FMath::Lerp(Tint, FLinearColor(0.55f, 0.85f, 1.f), 0.5f);
		Glow.Glow = 3.f;
		Glow.Opacity = 0.6f;
		Spawn(Glow);
	}
}

void ARRRandomEffects::StarTail(const FVector& From, const FVector& Head, float Width, float Length, const FLinearColor& Tint, float DeltaSeconds)
{
	const FVector Path = Head - From;
	const float Moved = Path.Size();
	if (Moved < 1.f || DeltaSeconds <= 0.f)
	{
		return;
	}
	const FVector Way = Path / Moved;
	const FRotator Along = Way.Rotation();
	const float Speed = Moved / DeltaSeconds;
	// The tail is glowing pieces left lying along the path, each thinning and fading as it ages: it reaches as far back as
	// the head goes in a piece's life
	const float Life = FMath::Clamp(Length / Speed, 0.2f, 1.2f);
	const int32 Pieces = FMath::Clamp(FMath::CeilToInt(Moved / FMath::Max(Width * 0.35f, 8.f)), 1, 24);
	const float Step = Moved / Pieces;
	const FLinearColor Hot = FMath::Lerp(Tint, FLinearColor::White, 0.6f);
	for (int32 Index = 0; Index < Pieces; ++Index)
	{
		const float Share = (Index + 0.5f) / Pieces;
		FParticle Piece;
		Piece.Shape = EShape::Ball;
		Piece.Location = From + Path * Share;
		// Laid earlier this frame further back: as old as the time since the head went by
		Piece.Age = (1.f - Share) * DeltaSeconds;
		Piece.Turn = Along;
		Piece.FadeIn = 0.f;
		// Long ovals overlapping their neighbours, so the tail is one smooth streak
		const float Long = FMath::Max(Step * 2.6f, Width * 0.8f) / 100.f;
		// The colored glow round it...
		Piece.Life = Life;
		Piece.StartSize = FVector(Long, Width / 100.f, Width / 100.f);
		Piece.EndSize = FVector(Long, Width * 0.04f / 100.f, Width * 0.04f / 100.f);
		Piece.Color = Tint;
		Piece.Glow = 2.5f;
		Piece.Opacity = 0.7f;
		Spawn(Piece);
		// ...and a white-hot core, shorter
		Piece.Life = Life * 0.6f;
		Piece.StartSize = FVector(Long, Width * 0.3f / 100.f, Width * 0.3f / 100.f);
		Piece.EndSize = FVector(Long, Width * 0.02f / 100.f, Width * 0.02f / 100.f);
		Piece.Color = Hot;
		Piece.Glow = 5.f;
		Piece.Opacity = 0.85f;
		Spawn(Piece);
	}
	// A soft halo round the head
	FParticle Halo;
	Halo.Shape = EShape::Ball;
	Halo.Location = Head;
	Halo.Life = 0.08f;
	Halo.StartSize = FVector(Width * 1.1f / 100.f);
	Halo.EndSize = FVector(Width * 0.9f / 100.f);
	Halo.Color = Hot;
	Halo.Glow = 5.f;
	Halo.Opacity = 0.25f;
	Halo.FadeIn = 0.f;
	Spawn(Halo);
	// Stars shed all along it, twinkling a while after the tail has gone
	const float StarSize = FMath::Clamp(Width * 0.35f, 25.f, 90.f) / 10.f;
	for (int32 Index = CountThisFrame(Moved / FMath::Max(Width * 0.5f, 18.f), 1.f); Index > 0; --Index)
	{
		FParticle Star;
		Star.Shape = EShape::Star;
		Star.Location = From + Path * FMath::FRand() + FMath::VRand() * Width * FMath::FRandRange(0.2f, 0.7f);
		Star.Velocity = Way * Speed * 0.04f + FMath::VRand() * FMath::FRandRange(20.f, 90.f);
		Star.Gravity = 90.f;
		Star.Drag = 2.f;
		Star.Life = FMath::FRandRange(0.55f, 1.1f);
		const float Start = StarSize * FMath::FRandRange(0.6f, 1.3f);
		Star.StartSize = FVector(Start);
		Star.EndSize = FVector(Start * 0.15f);
		Star.Spin = FRotator(0.f, 0.f, FMath::FRandRange(-240.f, 240.f));
		const float Pick = FMath::FRand();
		Star.Color = Pick < 0.4f ? Tint : Pick < 0.65f ? Gold : Candy[FMath::RandRange(0, UE_ARRAY_COUNT(Candy) - 1)];
		Star.Glow = 6.f;
		Star.FadeIn = 0.04f;
		Star.bFacesCamera = true;
		Spawn(Star);
	}
}

void ARRRandomEffects::StarImpact(const FVector& Where, const FVector& Way, float Size, const FLinearColor& Tint)
{
	const FVector Ground = Where + FVector(0.f, 0.f, 8.f);
	const FVector Ahead = Way.GetSafeNormal2D();
	const FLinearColor Hot = FMath::Lerp(Tint, FLinearColor::White, 0.6f);
	// A flash, flattened on the floor (small enough not to white out a close camera)
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Ground + FVector(0.f, 0.f, 25.f * Size);
	Flash.Life = 0.22f;
	Flash.StartSize = FVector(0.9f * Size, 0.9f * Size, 0.6f * Size);
	Flash.EndSize = FVector(2.f * Size, 2.f * Size, 0.9f * Size);
	Flash.Color = Hot;
	Flash.Glow = 6.f;
	Flash.Opacity = 0.4f;
	Flash.FadeIn = 0.01f;
	Spawn(Flash);
	// Two rings rushing out along the floor, white then the star's color
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FParticle Wave;
		Wave.Shape = EShape::Ring;
		Wave.Location = Ground + FVector(0.f, 0.f, Index * 4.f);
		Wave.Life = 0.4f + Index * 0.15f;
		Wave.StartSize = FVector(0.3f * Size, 0.3f * Size, 3.f);
		Wave.EndSize = FVector((3.4f + Index * 1.2f) * Size, (3.4f + Index * 1.2f) * Size, 1.5f);
		Wave.Color = Index == 0 ? FLinearColor::White : Tint;
		Wave.Glow = 7.f;
		Wave.FadeIn = 0.01f;
		Spawn(Wave);
	}
	// Rays of light shooting out flat along the floor, like the points of a star
	const int32 Rays = 10;
	const float RayStart = FMath::FRandRange(0.f, 360.f);
	for (int32 Index = 0; Index < Rays; ++Index)
	{
		const FVector Out = FVector(1.f, 0.f, 0.f).RotateAngleAxis(RayStart + Index * 360.f / Rays + FMath::FRandRange(-8.f, 8.f), FVector::UpVector);
		const float Reach = (Index % 2 == 0 ? 1.f : 0.6f) * FMath::FRandRange(0.85f, 1.15f);
		FParticle Ray;
		Ray.Shape = EShape::Ball;
		Ray.Location = Ground + Out * 30.f * Size + FVector(0.f, 0.f, 6.f);
		Ray.Velocity = Out * 900.f * Size * Reach;
		Ray.Drag = 5.f;
		Ray.Life = 0.32f;
		Ray.StartSize = FVector(0.5f * Size, 0.09f * Size, 0.05f * Size);
		Ray.EndSize = FVector(2.2f * Size * Reach, 0.015f * Size, 0.01f * Size);
		Ray.Turn = Out.Rotation();
		Ray.Color = Index % 2 == 0 ? FMath::Lerp(Tint, FLinearColor::White, 0.25f) : Gold;
		Ray.Glow = 8.f;
		Ray.Opacity = 0.9f;
		Ray.FadeIn = 0.01f;
		Spawn(Ray);
	}
	// A glow left on the floor where it hit, cooling off
	FParticle Mark;
	Mark.Shape = EShape::Ball;
	Mark.Location = Ground;
	Mark.Life = 1.4f;
	Mark.StartSize = FVector(1.4f * Size, 1.4f * Size, 0.04f * Size);
	Mark.EndSize = FVector(0.7f * Size, 0.7f * Size, 0.02f * Size);
	Mark.Color = Tint;
	Mark.Glow = 3.f;
	Mark.Opacity = 0.5f;
	Mark.FadeIn = 0.03f;
	Spawn(Mark);
	// Stars splashed up (most) and on the way it was going, raining back down
	const int32 Stars = FMath::Clamp(FMath::RoundToInt(16.f * FMath::Sqrt(Size)), 10, 40);
	for (int32 Index = 0; Index < Stars; ++Index)
	{
		const bool bOn = !Ahead.IsNearlyZero() && Index % 3 == 0;
		const FVector Direction = bOn ? (Ahead + FVector(0.f, 0.f, 0.7f)).GetSafeNormal() : FVector::UpVector;
		Spray(EShape::Star, Ground + FVector(0.f, 0.f, 20.f * Size), 1, Direction, bOn ? 30.f : 55.f, FVector2D(300.f, 700.f) * FMath::Sqrt(Size),
			FVector2D(3.f, 5.5f) * FMath::Min(Size, 2.5f), FVector2D(0.6f, 1.1f), Index % 2 == 0 ? Tint : Candy[Index % UE_ARRAY_COUNT(Candy)], 7.f, 900.f);
	}
	// A little dust round the foot
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FVector Out = FVector(1.f, 0.f, 0.f).RotateAngleAxis(Index * 45.f + FMath::FRandRange(-12.f, 12.f), FVector::UpVector);
		FParticle Puff;
		Puff.Shape = EShape::Ball;
		Puff.Location = Ground + Out * 25.f * Size + FVector(0.f, 0.f, 15.f * Size);
		Puff.Velocity = Out * FMath::FRandRange(250.f, 450.f) * Size + FVector(0.f, 0.f, FMath::FRandRange(40.f, 120.f));
		Puff.Drag = 3.f;
		Puff.Gravity = -40.f;
		Puff.Life = FMath::FRandRange(0.6f, 0.9f);
		const float Start = FMath::FRandRange(0.12f, 0.2f) * Size;
		Puff.StartSize = FVector(Start, Start, Start * 0.7f);
		Puff.EndSize = FVector(Start * 1.8f, Start * 1.8f, Start * 1.1f);
		Puff.Color = Cream;
		Puff.Glow = 1.2f;
		Puff.Opacity = 0.25f;
		Puff.FadeIn = 0.05f;
		Spawn(Puff);
	}
}

void ARRRandomEffects::DiceSpinSparkle(const FVector& Where, float Radius, float DeltaSeconds)
{
	// Glitter in the rainbow's colors, shed just off the tumbling die and drifting out, twinkling as it goes
	for (int32 Index = CountThisFrame(65.f, DeltaSeconds); Index > 0; --Index)
	{
		const FVector Out = FMath::VRand();
		FParticle Star;
		Star.Shape = EShape::Star;
		Star.Location = Where + Out * Radius * FMath::FRandRange(0.9f, 1.4f);
		Star.Velocity = Out * FMath::FRandRange(40.f, 110.f);
		Star.Gravity = 40.f;
		Star.Drag = 2.f;
		Star.Life = FMath::FRandRange(0.3f, 0.6f);
		const float Start = FMath::FRandRange(2.5f, 4.5f) * Radius / 27.5f;
		Star.StartSize = FVector(Start);
		Star.EndSize = FVector(Start * 0.15f);
		Star.Spin = FRotator(0.f, 0.f, FMath::FRandRange(-360.f, 360.f));
		Star.Color = Candy[FMath::RandRange(0, UE_ARRAY_COUNT(Candy) - 1)];
		Star.Glow = 6.f;
		Star.FadeIn = 0.04f;
		Star.bFacesCamera = true;
		Spawn(Star);
	}
	for (int32 Index = CountThisFrame(12.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Mote;
		Mote.Shape = EShape::Ball;
		Mote.Location = Where + FMath::VRand() * Radius * 1.1f;
		Mote.Life = FMath::FRandRange(0.25f, 0.4f);
		Mote.StartSize = FVector(0.05f * Radius / 27.5f);
		Mote.EndSize = FVector(0.12f * Radius / 27.5f);
		Mote.Color = FLinearColor::White;
		Mote.Glow = 5.f;
		Mote.Opacity = 0.8f;
		Spawn(Mote);
	}
}

void ARRRandomEffects::DiceLandBurst(const FVector& Where, float Radius)
{
	const float Size = Radius / 27.5f;
	FParticle Ring;
	Ring.Shape = EShape::Ring;
	Ring.Location = Where;
	Ring.Life = 0.35f;
	Ring.StartSize = FVector(0.4f * Size, 0.4f * Size, 2.f);
	Ring.EndSize = FVector(1.5f * Size, 1.5f * Size, 1.f);
	Ring.Color = Gold;
	Ring.Glow = 6.f;
	Ring.FadeIn = 0.02f;
	Ring.bFacesCamera = true;
	Spawn(Ring);
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where;
	Flash.Life = 0.18f;
	Flash.StartSize = FVector(0.5f * Size);
	Flash.EndSize = FVector(1.1f * Size);
	Flash.Color = FLinearColor::White;
	Flash.Glow = 5.f;
	Flash.Opacity = 0.5f;
	Flash.FadeIn = 0.02f;
	Spawn(Flash);
	for (int32 Index = 0; Index < 14; ++Index)
	{
		Spray(EShape::Star, Where, 1, FMath::VRand(), 30.f, FVector2D(150.f, 320.f) * Size, FVector2D(2.5f, 4.f) * Size, FVector2D(0.35f, 0.6f),
			Candy[Index % UE_ARRAY_COUNT(Candy)], 6.f, 150.f);
	}
}

void ARRRandomEffects::UltimateDieSpin(const FVector& Where, float Radius, const FVector& Feet, float Progress, float DeltaSeconds)
{
	const FLinearColor TigerOrange(1.f, 0.5f, 0.08f);
	const float Size = Radius / 27.5f;
	const float Now = GetWorld()->GetTimeSeconds();
	// Gold sparks in three arms swirling round the die, quicker as it nears landing
	const float Turn = Now * FMath::Lerp(400.f, 1100.f, Progress);
	for (int32 Index = CountThisFrame(150.f, DeltaSeconds); Index > 0; --Index)
	{
		const float Angle = Turn + 120.f * FMath::RandRange(0, 2) + FMath::FRandRange(-15.f, 15.f);
		const FVector Round = FVector(1.f, 0.f, 0.f).RotateAngleAxis(Angle, FVector::UpVector);
		FParticle Star;
		Star.Shape = EShape::Star;
		Star.Location = Where + Round * Radius * FMath::FRandRange(1.4f, 2.2f) + FVector(0.f, 0.f, FMath::FRandRange(-0.8f, 0.8f) * Radius);
		Star.Velocity = FVector::CrossProduct(FVector::UpVector, Round) * FMath::FRandRange(150.f, 300.f) * Size + FVector(0.f, 0.f, 60.f);
		Star.Drag = 1.5f;
		Star.Life = FMath::FRandRange(0.3f, 0.55f);
		const float Start = FMath::FRandRange(3.f, 5.5f) * Size;
		Star.StartSize = FVector(Start);
		Star.EndSize = FVector(Start * 0.1f);
		Star.Spin = FRotator(0.f, 0.f, FMath::FRandRange(-540.f, 540.f));
		Star.Color = FMath::FRand() < 0.65f ? Gold : (FMath::FRand() < 0.5f ? TigerOrange : FLinearColor::White);
		Star.Glow = 8.f;
		Star.FadeIn = 0.03f;
		Star.bFacesCamera = true;
		Spawn(Star);
	}
	// The beam: thin glowing columns from the feet up to the die, flickering
	const float Height = FMath::Max(Where.Z - Feet.Z, 50.f);
	for (int32 Index = CountThisFrame(40.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Beam;
		Beam.Shape = EShape::Ball;
		Beam.Location = FVector(Feet.X, Feet.Y, Feet.Z + Height * 0.5f) + FMath::VRand() * FVector(8.f, 8.f, 0.f);
		Beam.Life = FMath::FRandRange(0.08f, 0.16f);
		const float Width = FMath::FRandRange(0.25f, 0.5f) * FMath::Lerp(0.6f, 1.2f, Progress);
		Beam.StartSize = FVector(Width, Width, Height / 100.f);
		Beam.EndSize = FVector(Width * 0.5f, Width * 0.5f, Height / 100.f);
		Beam.Color = FMath::Lerp(Gold, FLinearColor::White, 0.3f);
		Beam.Glow = 6.f;
		Beam.Opacity = 0.35f;
		Beam.FadeIn = 0.02f;
		Spawn(Beam);
	}
	// Sparks rising up the beam
	for (int32 Index = CountThisFrame(60.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Mote;
		Mote.Shape = EShape::Ball;
		Mote.Location = Feet + FVector(FMath::FRandRange(-45.f, 45.f), FMath::FRandRange(-45.f, 45.f), 10.f);
		Mote.Velocity = FVector(0.f, 0.f, FMath::FRandRange(500.f, 900.f));
		Mote.Life = FMath::FRandRange(0.4f, 0.7f);
		Mote.StartSize = FVector(0.08f);
		Mote.EndSize = FVector(0.03f);
		Mote.Color = FMath::FRand() < 0.5f ? Gold : TigerOrange;
		Mote.Glow = 7.f;
		Spawn(Mote);
	}
	// Rings pulsing out along the ground
	for (int32 Index = CountThisFrame(4.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Ring;
		Ring.Shape = EShape::Ring;
		Ring.Location = Feet + FVector(0.f, 0.f, 8.f);
		Ring.Life = 0.6f;
		Ring.StartSize = FVector(0.5f, 0.5f, 2.f);
		Ring.EndSize = FVector(3.5f, 3.5f, 1.f);
		Ring.Color = Gold;
		Ring.Glow = 6.f;
		Ring.FadeIn = 0.02f;
		Spawn(Ring);
	}
}

void ARRRandomEffects::UltimateDieBurst(const FVector& Where, float Radius, const FVector& Feet)
{
	const FLinearColor TigerOrange(1.f, 0.5f, 0.08f);
	const float Size = Radius / 27.5f;
	const FLinearColor Colors[] = { FLinearColor::White, Gold, TigerOrange };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FParticle Ring;
		Ring.Shape = EShape::Ring;
		Ring.Location = Where;
		Ring.Life = 0.45f + Index * 0.15f;
		Ring.StartSize = FVector(0.5f * Size, 0.5f * Size, 3.f);
		Ring.EndSize = FVector((3.f + Index * 1.5f) * Size, (3.f + Index * 1.5f) * Size, 1.f);
		Ring.Color = Colors[Index];
		Ring.Glow = 8.f;
		Ring.FadeIn = 0.01f;
		Ring.bFacesCamera = true;
		Spawn(Ring);
	}
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where;
	Flash.Life = 0.3f;
	Flash.StartSize = FVector(0.8f * Size);
	Flash.EndSize = FVector(2.4f * Size);
	Flash.Color = FMath::Lerp(Gold, FLinearColor::White, 0.5f);
	Flash.Glow = 7.f;
	Flash.Opacity = 0.7f;
	Flash.FadeIn = 0.01f;
	Spawn(Flash);
	// A pillar of light through the brawler
	FParticle Pillar;
	Pillar.Shape = EShape::Ball;
	Pillar.Location = Feet + FVector(0.f, 0.f, 600.f);
	Pillar.Life = 0.7f;
	Pillar.StartSize = FVector(0.9f, 0.9f, 14.f);
	Pillar.EndSize = FVector(0.1f, 0.1f, 18.f);
	Pillar.Color = FMath::Lerp(Gold, FLinearColor::White, 0.35f);
	Pillar.Glow = 6.f;
	Pillar.Opacity = 0.55f;
	Pillar.FadeIn = 0.02f;
	Spawn(Pillar);
	// A fountain of stars and little gold dice, and a shockwave along the ground
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Spray(EShape::Star, Where, 18, FVector::UpVector, 75.f, FVector2D(250.f, 650.f) * Size, FVector2D(3.f, 5.5f) * Size, FVector2D(0.7f, 1.2f),
			Index == 0 ? FLinearColor::White : Index == 1 ? Gold : TigerOrange, 8.f, 500.f);
	}
	Spray(EShape::Die, Where, 10, FVector::UpVector, 70.f, FVector2D(300.f, 600.f) * Size, FVector2D(0.08f, 0.14f) * Size, FVector2D(0.9f, 1.3f), Gold, 5.f, 700.f);
	GiantBurst(Feet, 1.6f, TigerOrange);
}

void ARRRandomEffects::SpeedTrail(const FVector& Middle, const FVector& Feet, const FVector& Velocity, const FLinearColor& Tint, float DeltaSeconds)
{
	const float Speed = Velocity.Size2D();
	if (Speed < 1.f)
	{
		return;
	}
	const FVector Way = Velocity.GetSafeNormal2D();
	const FRotator Along = Way.Rotation();
	const FLinearColor Lightning(1.f, 0.82f, 0.25f);
	// Long thin streaks of light left hanging along the way, round the body
	for (int32 Index = CountThisFrame(70.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Streak;
		Streak.Shape = EShape::Ball;
		Streak.Location = Middle + FVector(0.f, FMath::FRandRange(-45.f, 45.f), FMath::FRandRange(-70.f, 70.f)).RotateAngleAxis(Along.Yaw, FVector::UpVector) - Way * FMath::FRandRange(0.f, 60.f);
		Streak.Life = FMath::FRandRange(0.12f, 0.22f);
		const float Length = FMath::Clamp(Speed * 0.12f, 60.f, 320.f) / 100.f;
		Streak.StartSize = FVector(Length, 0.035f, 0.035f);
		Streak.EndSize = FVector(Length * 1.4f, 0.01f, 0.01f);
		Streak.Turn = Along;
		Streak.Color = FMath::FRand() < 0.6f ? Lightning : FMath::Lerp(Tint, FLinearColor::White, 0.5f);
		Streak.Glow = 8.f;
		Streak.Opacity = 0.9f;
		Streak.FadeIn = 0.01f;
		Spawn(Streak);
	}
	// Crackling sparks jumping off the body
	for (int32 Index = CountThisFrame(55.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Spark;
		Spark.Shape = EShape::Star;
		Spark.Location = Middle + FMath::VRand() * FMath::FRandRange(20.f, 60.f);
		Spark.Velocity = FMath::VRand() * FMath::FRandRange(150.f, 400.f) - Way * Speed * 0.1f;
		Spark.Drag = 4.f;
		Spark.Life = FMath::FRandRange(0.1f, 0.22f);
		const float Start = FMath::FRandRange(2.f, 4.f);
		Spark.StartSize = FVector(Start);
		Spark.EndSize = FVector(Start * 0.2f);
		Spark.Spin = FRotator(0.f, 0.f, FMath::FRandRange(-720.f, 720.f));
		Spark.Color = Lightning;
		Spark.Glow = 8.f;
		Spark.FadeIn = 0.01f;
		Spark.bFacesCamera = true;
		Spawn(Spark);
	}
	// Dust kicked up behind the feet
	for (int32 Index = CountThisFrame(30.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Dust;
		Dust.Shape = EShape::Ball;
		Dust.Location = Feet + FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), 10.f);
		Dust.Velocity = -Way * FMath::FRandRange(100.f, 250.f) + FVector(0.f, 0.f, FMath::FRandRange(40.f, 120.f));
		Dust.Drag = 3.f;
		Dust.Life = FMath::FRandRange(0.35f, 0.6f);
		Dust.StartSize = FVector(0.15f);
		Dust.EndSize = FVector(0.5f, 0.5f, 0.35f);
		Dust.Color = Cream;
		Dust.Glow = 1.f;
		Dust.Opacity = 0.45f;
		Dust.FadeIn = 0.04f;
		Spawn(Dust);
	}
}

void ARRRandomEffects::SonicBoom(const FVector& Where, const FVector& Direction, const FLinearColor& Tint)
{
	const FVector Way = Direction.IsNearlyZero() ? FVector::ForwardVector : Direction.GetSafeNormal();
	// The ring stands across the way it goes (its axis along it)
	const FRotator Across = FRotationMatrix::MakeFromZ(Way).Rotator();
	const FLinearColor Lightning(1.f, 0.82f, 0.25f);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FParticle Ring;
		Ring.Shape = EShape::Ring;
		Ring.Location = Where + Way * (Index * 40.f);
		Ring.Life = 0.35f + Index * 0.1f;
		Ring.StartSize = FVector(0.6f, 0.6f, 2.f);
		Ring.EndSize = FVector(3.2f - Index, 3.2f - Index, 1.f);
		Ring.Turn = Across;
		Ring.Color = Index == 0 ? Lightning : FMath::Lerp(Tint, FLinearColor::White, 0.4f);
		Ring.Glow = 7.f;
		Ring.FadeIn = 0.01f;
		Spawn(Ring);
	}
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where;
	Flash.Life = 0.15f;
	Flash.StartSize = FVector(0.5f);
	Flash.EndSize = FVector(1.6f);
	Flash.Color = Lightning;
	Flash.Glow = 6.f;
	Flash.Opacity = 0.6f;
	Flash.FadeIn = 0.01f;
	Spawn(Flash);
	Spray(EShape::Star, Where, 18, -Way, 70.f, FVector2D(300.f, 800.f), FVector2D(3.f, 5.5f), FVector2D(0.25f, 0.45f), Lightning, 8.f, 300.f);
}

void ARRRandomEffects::FoxPoof(const FVector& Where, float Size)
{
	// A thick cloud of smoke...
	for (int32 Index = 0; Index < FMath::RoundToInt(14 * Size); ++Index)
	{
		FParticle Puff;
		Puff.Shape = EShape::Ball;
		Puff.Location = Where + FMath::VRand() * FVector(60.f, 60.f, 50.f) * Size;
		Puff.Velocity = FMath::VRand() * FMath::FRandRange(120.f, 320.f) * Size + FVector(0.f, 0.f, 80.f);
		Puff.Drag = 2.5f;
		Puff.Gravity = -60.f;
		Puff.Life = FMath::FRandRange(0.6f, 1.1f);
		const float Start = FMath::FRandRange(0.6f, 1.1f) * Size;
		Puff.StartSize = FVector(Start);
		Puff.EndSize = FVector(Start * 2.f);
		Puff.Color = FMath::FRand() < 0.5f ? FLinearColor(0.95f, 0.92f, 0.88f) : FLinearColor(0.85f, 0.8f, 0.78f);
		Puff.Glow = 1.1f;
		Puff.Opacity = 0.55f;
		Puff.FadeIn = 0.03f;
		Spawn(Puff);
	}
	// ...leaves whirling out of it, and a few orange sparks
	const FLinearColor Leaves[] = { FLinearColor(0.35f, 0.75f, 0.2f), FLinearColor(0.95f, 0.55f, 0.1f), FLinearColor(0.6f, 0.85f, 0.25f) };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Spray(EShape::Star, Where, FMath::RoundToInt(6 * Size), FVector::UpVector, 80.f, FVector2D(250.f, 550.f) * Size, FVector2D(3.f, 5.f), FVector2D(0.8f, 1.3f), Leaves[Index], 3.f, 350.f);
	}
	FParticle Ring;
	Ring.Shape = EShape::Ring;
	Ring.Location = Where - FVector(0.f, 0.f, 70.f);
	Ring.Life = 0.4f;
	Ring.StartSize = FVector(0.5f * Size, 0.5f * Size, 2.f);
	Ring.EndSize = FVector(3.f * Size, 3.f * Size, 1.f);
	Ring.Color = FLinearColor(1.f, 0.55f, 0.15f);
	Ring.Glow = 5.f;
	Spawn(Ring);
}

void ARRRandomEffects::MoonFlash(const FVector& Where, float Size)
{
	const FLinearColor Moon(1.f, 0.95f, 0.72f);
	const FLinearColor Silver(0.75f, 0.85f, 1.f);
	FParticle Disc;
	Disc.Shape = EShape::Ball;
	Disc.Location = Where;
	Disc.Life = 0.9f;
	Disc.StartSize = FVector(2.5f * Size);
	Disc.EndSize = FVector(3.2f * Size);
	Disc.Color = Moon;
	Disc.Glow = 3.f;
	Disc.Opacity = 0.45f;
	Disc.FadeIn = 0.12f;
	Spawn(Disc);
	FParticle Halo;
	Halo.Shape = EShape::Ring;
	Halo.Location = Where;
	Halo.Life = 0.8f;
	Halo.StartSize = FVector(2.6f * Size, 2.6f * Size, 2.f);
	Halo.EndSize = FVector(4.2f * Size, 4.2f * Size, 1.f);
	Halo.Color = Silver;
	Halo.Glow = 5.f;
	Halo.FadeIn = 0.05f;
	Halo.bFacesCamera = true;
	Spawn(Halo);
	Spray(EShape::Star, Where, FMath::RoundToInt(12 * Size), FVector::UpVector, 180.f, FVector2D(150.f, 400.f) * Size, FVector2D(3.f, 5.f) * FMath::Sqrt(Size), FVector2D(0.6f, 1.f), Silver, 7.f, 0.f);
}

void ARRRandomEffects::AcornPop(const FVector& Where, float Size)
{
	const FLinearColor Brown(0.55f, 0.32f, 0.12f);
	for (int32 Index = 0; Index < FMath::RoundToInt(4 * Size); ++Index)
	{
		FParticle Dust;
		Dust.Shape = EShape::Ball;
		Dust.Location = Where;
		Dust.Velocity = FMath::VRandCone(FVector::UpVector, 1.2f) * FMath::FRandRange(80.f, 200.f) * Size;
		Dust.Drag = 3.f;
		Dust.Life = FMath::FRandRange(0.3f, 0.5f);
		const float Start = 0.18f * Size;
		Dust.StartSize = FVector(Start);
		Dust.EndSize = FVector(Start * 2.2f);
		Dust.Color = Cream;
		Dust.Glow = 1.f;
		Dust.Opacity = 0.4f;
		Spawn(Dust);
	}
	Spray(EShape::Die, Where, FMath::RoundToInt(2 * Size), FVector::UpVector, 50.f, FVector2D(150.f, 300.f) * Size, FVector2D(0.04f, 0.07f) * Size, FVector2D(0.4f, 0.6f), Brown, 1.5f, 800.f);
	Spray(EShape::Star, Where, FMath::RoundToInt(2 * Size), FVector::UpVector, 60.f, FVector2D(120.f, 260.f) * Size, FVector2D(2.f, 3.f) * Size, FVector2D(0.3f, 0.5f), Gold, 5.f, 300.f);
}

void ARRRandomEffects::TigerStrike(const FVector& Where, float Radius, const FLinearColor& Tint)
{
	// The rings are the 100 unit mesh: Radius / 50 spans the strike
	const float Span = Radius / 50.f;
	const FLinearColor Colors[] = { FLinearColor::White, Tint, Gold };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FParticle Wave;
		Wave.Shape = EShape::Ring;
		Wave.Location = Where + FVector(0.f, 0.f, 15.f + Index * 6.f);
		Wave.Life = 0.45f + Index * 0.2f;
		Wave.StartSize = FVector(Span * 0.15f, Span * 0.15f, 6.f);
		Wave.EndSize = FVector(Span * (1.f + Index * 0.25f), Span * (1.f + Index * 0.25f), 3.f);
		Wave.Color = Colors[Index];
		Wave.Glow = 7.f;
		Wave.FadeIn = 0.01f;
		Spawn(Wave);
	}
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where + FVector(0.f, 0.f, 60.f);
	Flash.Life = 0.3f;
	Flash.StartSize = FVector(Span * 0.3f, Span * 0.3f, Span * 0.15f);
	Flash.EndSize = FVector(Span * 0.7f, Span * 0.7f, Span * 0.3f);
	Flash.Color = FMath::Lerp(Tint, FLinearColor::White, 0.5f);
	Flash.Glow = 6.f;
	Flash.Opacity = 0.7f;
	Flash.FadeIn = 0.01f;
	Spawn(Flash);
	// Dust rolling out from the edge
	for (int32 Index = 0; Index < 28; ++Index)
	{
		const FVector Out = FVector(1.f, 0.f, 0.f).RotateAngleAxis(Index * (360.f / 28.f) + FMath::FRandRange(-6.f, 6.f), FVector::UpVector);
		FParticle Puff;
		Puff.Shape = EShape::Ball;
		Puff.Location = Where + Out * Radius * FMath::FRandRange(0.5f, 0.9f) + FVector(0.f, 0.f, 40.f);
		Puff.Velocity = Out * FMath::FRandRange(500.f, 900.f) + FVector(0.f, 0.f, FMath::FRandRange(80.f, 260.f));
		Puff.Drag = 2.2f;
		Puff.Gravity = -60.f;
		Puff.Life = FMath::FRandRange(1.f, 1.6f);
		const float Start = FMath::FRandRange(0.6f, 1.f);
		Puff.StartSize = FVector(Start, Start, Start * 0.6f);
		Puff.EndSize = FVector(Start * 1.8f, Start * 1.8f, Start);
		Puff.Color = Cream;
		Puff.Glow = 1.2f;
		Puff.Opacity = 0.3f;
		Puff.FadeIn = 0.06f;
		Spawn(Puff);
	}
	// Stars and candy dice thrown high
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Spray(EShape::Star, Where + FVector(0.f, 0.f, 80.f), 14, FVector::UpVector, 60.f, FVector2D(800.f, 1700.f), FVector2D(8.f, 14.f), FVector2D(1.f, 1.6f), Candy[Index + 1], 7.f, 1100.f);
	}
	Spray(EShape::Die, Where + FVector(0.f, 0.f, 80.f), 16, FVector::UpVector, 55.f, FVector2D(700.f, 1500.f), FVector2D(0.25f, 0.5f), FVector2D(1.2f, 1.8f), Tint, 3.f, 1400.f);
}

void ARRRandomEffects::TeleportPoof(const FVector& Where, const FLinearColor& Tint)
{
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where;
	Flash.Life = 0.3f;
	Flash.StartSize = FVector(0.6f, 0.6f, 1.6f);
	Flash.EndSize = FVector(1.6f, 1.6f, 2.6f);
	Flash.Color = FMath::Lerp(Tint, FLinearColor::White, 0.4f);
	Flash.Glow = 5.f;
	Flash.Opacity = 0.6f;
	Flash.FadeIn = 0.02f;
	Spawn(Flash);
	FParticle Ring;
	Ring.Shape = EShape::Ring;
	Ring.Location = Where - FVector(0.f, 0.f, 80.f);
	Ring.Life = 0.45f;
	Ring.StartSize = FVector(0.5f, 0.5f, 2.f);
	Ring.EndSize = FVector(2.2f, 2.2f, 1.f);
	Ring.Color = Gold;
	Ring.Glow = 5.f;
	Spawn(Ring);
	for (int32 Index = 0; Index < 16; ++Index)
	{
		Spray(EShape::Star, Where + FMath::VRand() * 40.f, 1, FVector::UpVector, 80.f, FVector2D(150.f, 450.f), FVector2D(3.f, 5.5f), FVector2D(0.5f, 0.9f),
			Candy[Index % UE_ARRAY_COUNT(Candy)], 6.f, 200.f);
	}
}

void ARRRandomEffects::HitBurst(const FVector& Where, const FVector& FromDirection, float Strength, const FLinearColor& Tint)
{
	const FVector Back = FromDirection.IsNearlyZero() ? FVector::UpVector : FromDirection.GetSafeNormal();
	const float Grow = FMath::Sqrt(FMath::Max(Strength, 0.3f));
	// Sparks and stars thrown back toward the shooter, a quick ring turned to the camera, a flash
	Spray(EShape::Ball, Where, FMath::Clamp(FMath::RoundToInt(10.f * Strength), 6, 30), Back, 65.f, FVector2D(400.f, 900.f) * Grow, FVector2D(0.06f, 0.11f),
		FVector2D(0.25f, 0.5f), Tint, 8.f, 900.f);
	Spray(EShape::Star, Where, FMath::Clamp(FMath::RoundToInt(6.f * Strength), 4, 18), Back, 80.f, FVector2D(200.f, 500.f) * Grow, FVector2D(4.5f, 8.f) * Grow,
		FVector2D(0.35f, 0.6f), Tint, 7.f, 300.f);
	FParticle Ring;
	Ring.Shape = EShape::Ring;
	Ring.Location = Where;
	Ring.Life = 0.22f;
	Ring.StartSize = FVector(0.3f, 0.3f, 2.f);
	Ring.EndSize = FVector(1.8f * Grow, 1.8f * Grow, 1.f);
	Ring.Color = Tint;
	Ring.Glow = 7.f;
	Ring.Opacity = 0.9f;
	Ring.FadeIn = 0.01f;
	Ring.bFacesCamera = true;
	Spawn(Ring);
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where;
	Flash.Life = 0.12f;
	Flash.StartSize = FVector(0.2f);
	Flash.EndSize = FVector(0.9f * Grow);
	Flash.Color = FLinearColor::White;
	Flash.Glow = 6.f;
	Flash.Opacity = 0.8f;
	Flash.FadeIn = 0.01f;
	Spawn(Flash);
}

void ARRRandomEffects::ShotFizzle(const FVector& Where, const FVector& Direction, float Size, const FLinearColor& Tint)
{
	const FVector Way = Direction.IsNearlyZero() ? FVector::UpVector : Direction.GetSafeNormal();
	// The ball itself (a quarter of the 100 unit mesh, like the shot) carrying on a little, swelling soft as it fades
	FParticle Puff;
	Puff.Shape = EShape::Ball;
	Puff.Location = Where;
	Puff.Velocity = Way * 500.f * Size;
	Puff.Drag = 6.f;
	Puff.Life = 0.35f;
	Puff.StartSize = FVector(0.25f * Size);
	Puff.EndSize = FVector(0.75f * Size);
	Puff.Color = FMath::Lerp(Tint, FLinearColor::White, 0.35f);
	Puff.Glow = 4.f;
	Puff.Opacity = 0.55f;
	Puff.FadeIn = 0.01f;
	Spawn(Puff);
	FParticle Ring;
	Ring.Shape = EShape::Ring;
	Ring.Location = Where;
	Ring.Life = 0.25f;
	Ring.StartSize = FVector(0.15f * Size, 0.15f * Size, 1.5f);
	Ring.EndSize = FVector(0.8f * Size, 0.8f * Size, 1.f);
	Ring.Color = Tint;
	Ring.Glow = 5.f;
	Ring.Opacity = 0.7f;
	Ring.FadeIn = 0.01f;
	Ring.bFacesCamera = true;
	Spawn(Ring);
	// Glitter scattering on ahead and drifting down
	Spray(EShape::Ball, Where, 8, Way, 70.f, FVector2D(120.f, 320.f) * Size, FVector2D(0.03f, 0.06f) * Size, FVector2D(0.3f, 0.55f), Tint, 6.f, 250.f);
	Spray(EShape::Star, Where, 3, Way, 90.f, FVector2D(80.f, 220.f), FVector2D(2.5f, 4.f) * FMath::Sqrt(Size), FVector2D(0.35f, 0.55f), Tint, 6.f, 150.f);
}

void ARRRandomEffects::ShotSplat(const FVector& Where, const FVector& Normal, float Size, const FLinearColor& Tint)
{
	const FVector Out = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	const float Grow = FMath::Sqrt(FMath::Max(Size, 0.3f));
	// A ring spreading flat over the surface it hit
	FParticle Ring;
	Ring.Shape = EShape::Ring;
	Ring.Location = Where + Out * 2.f;
	Ring.Life = 0.3f;
	Ring.StartSize = FVector(0.2f * Grow, 0.2f * Grow, 1.5f);
	Ring.EndSize = FVector(1.2f * Grow, 1.2f * Grow, 1.f);
	Ring.Turn = FRotationMatrix::MakeFromZ(Out).Rotator();
	Ring.Color = Tint;
	Ring.Glow = 6.f;
	Ring.Opacity = 0.85f;
	Ring.FadeIn = 0.01f;
	Spawn(Ring);
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where + Out * 5.f;
	Flash.Life = 0.1f;
	Flash.StartSize = FVector(0.15f * Grow);
	Flash.EndSize = FVector(0.6f * Grow);
	Flash.Color = FLinearColor::White;
	Flash.Glow = 6.f;
	Flash.Opacity = 0.8f;
	Flash.FadeIn = 0.01f;
	Spawn(Flash);
	// Bits and stars bouncing off the surface and falling back
	Spray(EShape::Ball, Where, FMath::Clamp(FMath::RoundToInt(8.f * Size), 6, 20), Out, 60.f, FVector2D(300.f, 750.f) * Grow, FVector2D(0.05f, 0.09f),
		FVector2D(0.25f, 0.5f), Tint, 7.f, 900.f);
	Spray(EShape::Star, Where, FMath::Clamp(FMath::RoundToInt(4.f * Size), 3, 10), Out, 70.f, FVector2D(150.f, 400.f) * Grow, FVector2D(3.5f, 6.f) * Grow,
		FVector2D(0.3f, 0.5f), Tint, 6.f, 300.f);
	// A soft puff of dust
	FParticle Puff;
	Puff.Shape = EShape::Ball;
	Puff.Location = Where + Out * 10.f;
	Puff.Velocity = Out * 120.f;
	Puff.Drag = 3.f;
	Puff.Life = 0.45f;
	Puff.StartSize = FVector(0.2f * Grow);
	Puff.EndSize = FVector(0.7f * Grow);
	Puff.Color = FMath::Lerp(Tint, Cream, 0.6f);
	Puff.Glow = 1.5f;
	Puff.Opacity = 0.45f;
	Spawn(Puff);
}

void ARRRandomEffects::DragonBreath(const FVector& Mouth, const FVector& Impact, float Radius, float DeltaSeconds, const FVector& Carry)
{
	// The stream: balls of fire leaving the mouth small and hot, swelling and reddening on their way to where it lands
	const float Spread = Radius / 50.f;
	for (int32 Index = CountThisFrame(110.f + Radius * 0.15f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Fire;
		Fire.Shape = EShape::Ball;
		Fire.Location = Mouth + FMath::VRand() * 15.f;
		const FVector2D Disc = FMath::RandPointInCircle(Radius * 0.75f);
		const float Travel = FMath::FRandRange(0.26f, 0.38f);
		Fire.Velocity = (Impact + Carry * Travel + FVector(Disc.X, Disc.Y, 30.f) - Fire.Location) / Travel;
		Fire.Life = Travel * FMath::FRandRange(0.95f, 1.2f);
		Fire.StartSize = FVector(FMath::FRandRange(0.3f, 0.5f));
		Fire.EndSize = FVector(Spread * FMath::FRandRange(0.35f, 0.6f));
		Fire.Turn = FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f);
		// Mostly orange and red: too bright or too yellow and it reads as white against the sunny map
		const float Heat = FMath::FRand();
		Fire.Color = Heat < 0.15f ? FireYellow : (Heat < 0.65f ? FireOrange : FireRed);
		Fire.Glow = FMath::FRandRange(1.6f, 2.4f);
		Fire.Opacity = 0.85f;
		Fire.FadeIn = 0.02f;
		Spawn(Fire);
	}
	// Where it lands: fire splashing up and out, and sparks
	for (int32 Index = CountThisFrame(45.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Splash;
		Splash.Shape = EShape::Ball;
		const FVector2D Disc = FMath::RandPointInCircle(Radius);
		Splash.Location = Impact + FVector(Disc.X, Disc.Y, 20.f);
		Splash.Velocity = FVector(Disc.X, Disc.Y, 0.f).GetSafeNormal() * FMath::FRandRange(80.f, 260.f) + FVector(0.f, 0.f, FMath::FRandRange(250.f, 550.f));
		Splash.Gravity = 300.f;
		Splash.Drag = 1.2f;
		Splash.Life = FMath::FRandRange(0.45f, 0.75f);
		Splash.StartSize = FVector(Spread * FMath::FRandRange(0.25f, 0.45f));
		Splash.EndSize = FVector(0.05f);
		Splash.Color = FMath::FRand() < 0.5f ? FireOrange : FireRed;
		Splash.Glow = 1.8f;
		Splash.Opacity = 0.85f;
		Spawn(Splash);
	}
	Spray(EShape::Star, Impact + FVector(0.f, 0.f, 30.f), CountThisFrame(30.f, DeltaSeconds), FVector::UpVector, 60.f, FVector2D(400.f, 900.f),
		FVector2D(4.f, 7.f), FVector2D(0.5f, 0.9f), FireYellow, 6.f, 500.f);
}

void ARRRandomEffects::GroundFire(const FVector& Where, float Radius, float Strength, float DeltaSeconds)
{
	// Flames rising and shrinking, smaller and fewer as the fire dies down
	const float Spread = Radius / 50.f;
	for (int32 Index = CountThisFrame(16.f * Strength, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Flame;
		Flame.Shape = EShape::Ball;
		const FVector2D Disc = FMath::RandPointInCircle(Radius);
		Flame.Location = Where + FVector(Disc.X, Disc.Y, 15.f);
		Flame.Velocity = FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(140.f, 320.f));
		// Rising faster as it goes, like a flame
		Flame.Gravity = -250.f;
		Flame.Life = FMath::FRandRange(0.45f, 0.8f);
		Flame.StartSize = FVector(Spread * FMath::FRandRange(0.18f, 0.32f) * (0.5f + 0.5f * Strength));
		Flame.EndSize = FVector(0.03f);
		const float Heat = FMath::FRand();
		Flame.Color = Heat < 0.15f ? FireYellow : (Heat < 0.65f ? FireOrange : FireRed);
		Flame.Glow = 1.8f;
		Flame.Opacity = 0.8f;
		Flame.FadeIn = 0.08f;
		Spawn(Flame);
	}
	if (FMath::FRand() < Strength * DeltaSeconds * 6.f)
	{
		Spray(EShape::Star, Where + FVector(0.f, 0.f, 20.f), 1, FVector::UpVector, 40.f, FVector2D(200.f, 450.f), FVector2D(3.f, 5.f),
			FVector2D(0.5f, 0.9f), FireYellow, 6.f, -100.f);
	}
}

void ARRRandomEffects::GroundEmbers(const FVector& Where, float Radius, float Strength, float DeltaSeconds)
{
	// Coals scattered over the floor (the scorch decal glows under them), each glowing up and dimming again (a flicker),
	// fewer as they go out; little flames licking up off them, and sparks drifting up
	for (int32 Index = CountThisFrame(26.f * Strength, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Ember;
		Ember.Shape = EShape::Ball;
		const FVector2D Disc = FMath::RandPointInCircle(Radius);
		Ember.Location = Where + FVector(Disc.X, Disc.Y, 10.f);
		Ember.Velocity = FVector(0.f, 0.f, FMath::FRandRange(0.f, 30.f));
		Ember.Drag = 2.f;
		Ember.Life = FMath::FRandRange(0.7f, 1.2f);
		const float Size = FMath::FRandRange(0.26f, 0.42f) * (0.6f + 0.4f * Strength);
		Ember.StartSize = FVector(Size, Size, Size * 0.7f);
		Ember.EndSize = FVector(Size * 0.4f, Size * 0.4f, Size * 0.3f);
		const float Heat = FMath::FRand();
		// Mostly orange and red: brighter or yellower and they wash out to white against the sunny map
		Ember.Color = Heat < 0.12f ? FireYellow : (Heat < 0.65f ? FireOrange : FireRed);
		Ember.Glow = FMath::FRandRange(2.4f, 3.4f);
		Ember.Opacity = 1.f;
		Ember.FadeIn = Ember.Life * 0.35f;
		Spawn(Ember);
	}
	for (int32 Index = CountThisFrame(5.f * Strength, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Flame;
		Flame.Shape = EShape::Ball;
		const FVector2D Disc = FMath::RandPointInCircle(Radius * 0.9f);
		Flame.Location = Where + FVector(Disc.X, Disc.Y, 15.f);
		Flame.Velocity = FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(90.f, 180.f));
		Flame.Gravity = -200.f;
		Flame.Life = FMath::FRandRange(0.35f, 0.6f);
		Flame.StartSize = FVector(FMath::FRandRange(0.3f, 0.5f));
		Flame.EndSize = FVector(0.03f);
		Flame.Color = FMath::FRand() < 0.5f ? FireOrange : FireRed;
		Flame.Glow = 2.5f;
		Flame.Opacity = 0.85f;
		Flame.FadeIn = 0.08f;
		Spawn(Flame);
	}
	if (FMath::FRand() < Strength * DeltaSeconds * 6.f)
	{
		const FVector2D Disc = FMath::RandPointInCircle(Radius);
		Spray(EShape::Star, Where + FVector(Disc.X, Disc.Y, 10.f), 1, FVector::UpVector, 30.f, FVector2D(100.f, 260.f), FVector2D(4.f, 6.f),
			FVector2D(0.7f, 1.2f), FireYellow, 6.f, -60.f);
	}
}

void ARRRandomEffects::FireBurst(const FVector& Where, float Size)
{
	// A flash, a ring of fire running out, flame balls thrown up and out, sparks
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where;
	Flash.Life = 0.3f;
	Flash.StartSize = FVector(1.2f * Size);
	Flash.EndSize = FVector(3.f * Size);
	Flash.Color = FireYellow;
	Flash.Glow = 6.f;
	Flash.Opacity = 0.7f;
	Spawn(Flash);
	FParticle Ring;
	Ring.Shape = EShape::Ring;
	Ring.Location = Where - FVector(0.f, 0.f, 70.f * Size);
	Ring.Life = 0.5f;
	Ring.StartSize = FVector(0.5f * Size, 0.5f * Size, 1.f);
	Ring.EndSize = FVector(4.5f * Size, 4.5f * Size, 1.f);
	Ring.Color = FireOrange;
	Ring.Glow = 4.f;
	Spawn(Ring);
	for (int32 Index = FMath::RoundToInt(36.f * Size); Index > 0; --Index)
	{
		FParticle Fire;
		Fire.Shape = EShape::Ball;
		Fire.Location = Where + FMath::VRand() * 30.f * Size;
		Fire.Velocity = FMath::VRandCone(FVector::UpVector, FMath::DegreesToRadians(80.f)) * FMath::FRandRange(250.f, 650.f) * Size;
		Fire.Gravity = -150.f;
		Fire.Drag = 2.f;
		Fire.Life = FMath::FRandRange(0.5f, 0.9f);
		Fire.StartSize = FVector(FMath::FRandRange(0.4f, 0.8f) * Size);
		Fire.EndSize = FVector(0.05f);
		const float Heat = FMath::FRand();
		Fire.Color = Heat < 0.3f ? FireYellow : (Heat < 0.75f ? FireOrange : FireRed);
		Fire.Glow = 4.f;
		Fire.Opacity = 0.85f;
		Spawn(Fire);
	}
	Spray(EShape::Star, Where, FMath::RoundToInt(24.f * Size), FVector::UpVector, 100.f, FVector2D(300.f, 800.f) * Size, FVector2D(5.f, 9.f),
		FVector2D(0.6f, 1.f), FireYellow, 7.f, 250.f);
}

void ARRRandomEffects::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Stars and camera-facing rings turn toward this machine's camera
	FQuat Facing = FQuat::Identity;
	FVector Eye = FVector(UE_BIG_NUMBER);
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Eye = Camera->GetCameraLocation();
		const FQuat View = Camera->GetCameraRotation().Quaternion();
		Facing = FRotationMatrix::MakeFromXZ(-View.GetForwardVector(), View.GetUpVector()).ToQuat();
	}
	// A ring lies in its own XY plane; facing the camera, its axis (Z) points at it
	const FQuat RingUp(FVector::RightVector, -HALF_PI);

	TArray<bool> Touched;
	Touched.Init(false, Pools.Num());
	for (int32 Index = Live.Num() - 1; Index >= 0; --Index)
	{
		FParticle& Particle = Live[Index];
		const int32 Shape = static_cast<int32>(Particle.Shape);
		UInstancedStaticMeshComponent* Pool = Pools[Shape];
		Touched[Shape] = true;
		Particle.Age += DeltaSeconds;
		if (Particle.Age >= Particle.Life)
		{
			Pool->UpdateInstanceTransform(Particle.Slot, FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), true, false, true);
			FreeSlots[Shape].Add(Particle.Slot);
			Live.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}
		Particle.Velocity.Z -= Particle.Gravity * DeltaSeconds;
		Particle.Velocity *= FMath::Max(0.f, 1.f - Particle.Drag * DeltaSeconds);
		Particle.Location += Particle.Velocity * DeltaSeconds;
		Particle.Turn += Particle.Spin * DeltaSeconds;

		const float Alpha = Particle.Age / Particle.Life;
		const FVector Size = FMath::Lerp(Particle.StartSize, Particle.EndSize, EaseOut(Alpha));
		FQuat Rotation = Particle.Turn.Quaternion();
		if (Particle.bFacesCamera)
		{
			Rotation = Particle.Shape == EShape::Ring ? Facing * RingUp : Facing * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Particle.Turn.Roll));
		}
		Pool->UpdateInstanceTransform(Particle.Slot, FTransform(Rotation, Particle.Location, Size.ComponentMax(FVector(KINDA_SMALL_NUMBER))), true, false, true);

		// Pops in, then fades out over the rest of its life
		const float In = Particle.FadeIn > 0.f ? FMath::Clamp(Particle.Age / Particle.FadeIn, 0.f, 1.f) : 1.f;
		const float Out = 1.f - FMath::Clamp((Particle.Age - Particle.FadeIn) / FMath::Max(Particle.Life - Particle.FadeIn, KINDA_SMALL_NUMBER), 0.f, 1.f);
		const float Glow = Particle.Glow > 1.f ? 1.f + (Particle.Glow - 1.f) * GlowSqueeze : Particle.Glow;
		const float Radius = Size.GetMax() * 50.f;
		const float Near = FMath::Clamp((FVector::Dist(Particle.Location, Eye) - Radius) / NearFadeDistance, NearFadeFloor, 1.f);
		const float Data[DataFloats] = { Particle.Color.R, Particle.Color.G, Particle.Color.B, Glow, Particle.Opacity * In * FMath::Sqrt(Out) * Near };
		Pool->SetCustomData(Particle.Slot, MakeArrayView(Data, DataFloats), false);
	}
	for (int32 Shape = 0; Shape < Pools.Num(); ++Shape)
	{
		if (Touched[Shape] && Pools[Shape])
		{
			Pools[Shape]->MarkRenderStateDirty();
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The star robot's rocket fists and steps (round 61)

namespace
{
	const FLinearColor Smoke(1.f, 0.95f, 0.88f);
	const FLinearColor SmokeDark(0.78f, 0.74f, 0.82f);
	const FLinearColor Dust(0.95f, 0.86f, 0.72f);

	FLinearColor FireColor(float Heat)
	{
		return Heat < 0.3f ? FireYellow : (Heat < 0.75f ? FireOrange : FireRed);
	}
}

void ARRRandomEffects::RocketLaunch(const FVector& Where, const FVector& Direction, const FLinearColor& Tint)
{
	const FVector Way = Direction.IsNearlyZero() ? FVector::ForwardVector : Direction.GetSafeNormal();
	const FRotator Across = FRotationMatrix::MakeFromZ(Way).Rotator();
	// A hot flash where the forearm lets go
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where;
	Flash.Life = 0.18f;
	Flash.StartSize = FVector(1.2f);
	Flash.EndSize = FVector(2.6f);
	Flash.Color = FMath::Lerp(FireYellow, FLinearColor::White, 0.5f);
	Flash.Glow = 7.f;
	Flash.Opacity = 0.75f;
	Flash.FadeIn = 0.01f;
	Spawn(Flash);
	// The jet out of the elbow, behind: hot core first, flames, then a cloud of smoke rolling back
	for (int32 Index = 0; Index < 26; ++Index)
	{
		FParticle Fire;
		Fire.Shape = EShape::Ball;
		Fire.Location = Where - Way * FMath::FRandRange(20.f, 80.f);
		Fire.Velocity = FMath::VRandCone(-Way, FMath::DegreesToRadians(28.f)) * FMath::FRandRange(500.f, 1300.f);
		Fire.Drag = 4.f;
		Fire.Gravity = -80.f;
		Fire.Life = FMath::FRandRange(0.25f, 0.5f);
		Fire.StartSize = FVector(FMath::FRandRange(0.6f, 1.1f));
		Fire.EndSize = FVector(0.1f);
		Fire.Color = FireColor(FMath::FRand());
		Fire.Glow = 4.5f;
		Fire.Opacity = 0.9f;
		Fire.FadeIn = 0.01f;
		Spawn(Fire);
	}
	for (int32 Index = 0; Index < 14; ++Index)
	{
		FParticle Puff;
		Puff.Shape = EShape::Ball;
		Puff.Location = Where - Way * FMath::FRandRange(40.f, 140.f) + FMath::VRand() * 30.f;
		Puff.Velocity = FMath::VRandCone(-Way, FMath::DegreesToRadians(55.f)) * FMath::FRandRange(150.f, 450.f) + FVector(0.f, 0.f, 60.f);
		Puff.Drag = 2.5f;
		Puff.Gravity = -60.f;
		Puff.Life = FMath::FRandRange(0.7f, 1.2f);
		Puff.StartSize = FVector(FMath::FRandRange(0.7f, 1.1f));
		Puff.EndSize = FVector(FMath::FRandRange(1.8f, 2.6f));
		Puff.Color = FMath::FRand() < 0.6f ? Smoke : SmokeDark;
		Puff.Glow = 0.75f;
		Puff.Opacity = 0.5f;
		Puff.FadeIn = 0.06f;
		Spawn(Puff);
	}
	// A ring of smoke round the arm, and a gold shock ring running ahead with the fist
	FParticle SmokeRing;
	SmokeRing.Shape = EShape::Ring;
	SmokeRing.Location = Where - Way * 40.f;
	SmokeRing.Life = 0.6f;
	SmokeRing.StartSize = FVector(1.f, 1.f, 3.f);
	SmokeRing.EndSize = FVector(4.f, 4.f, 2.f);
	SmokeRing.Turn = Across;
	SmokeRing.Color = Smoke;
	SmokeRing.Glow = 1.2f;
	SmokeRing.Opacity = 0.7f;
	Spawn(SmokeRing);
	SonicBoom(Where + Way * 60.f, Way, Tint);
	Spray(EShape::Star, Where, 16, -Way, 60.f, FVector2D(400.f, 900.f), FVector2D(3.f, 6.f), FVector2D(0.3f, 0.6f), FireYellow, 8.f, 400.f);
}

void ARRRandomEffects::RocketExhaust(const FVector& Nozzle, const FVector& Back, float Size, float DeltaSeconds)
{
	const FVector Way = Back.IsNearlyZero() ? -FVector::ForwardVector : Back.GetSafeNormal();
	// The hot core right at the nozzle
	for (int32 Index = CountThisFrame(60.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Core;
		Core.Shape = EShape::Ball;
		Core.Location = Nozzle + FMath::VRand() * 6.f * Size;
		Core.Velocity = Way * 600.f * Size;
		Core.Drag = 6.f;
		Core.Life = FMath::FRandRange(0.06f, 0.12f);
		Core.StartSize = FVector(0.75f * Size);
		Core.EndSize = FVector(0.3f * Size);
		Core.Color = FMath::Lerp(FireYellow, FLinearColor::White, 0.55f);
		Core.Glow = 7.f;
		Core.Opacity = 0.9f;
		Core.FadeIn = 0.f;
		Spawn(Core);
	}
	// Flames streaming back, cooling from yellow to red
	for (int32 Index = CountThisFrame(90.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Fire;
		Fire.Shape = EShape::Ball;
		Fire.Location = Nozzle + FMath::VRand() * 12.f * Size;
		Fire.Velocity = FMath::VRandCone(Way, FMath::DegreesToRadians(12.f)) * FMath::FRandRange(350.f, 700.f) * Size;
		Fire.Drag = 3.f;
		Fire.Gravity = -100.f;
		Fire.Life = FMath::FRandRange(0.14f, 0.3f);
		Fire.StartSize = FVector(FMath::FRandRange(0.5f, 0.8f) * Size);
		Fire.EndSize = FVector(0.12f * Size);
		Fire.Color = FireColor(FMath::FRand());
		Fire.Glow = 4.f;
		Fire.Opacity = 0.85f;
		Fire.FadeIn = 0.01f;
		Spawn(Fire);
	}
	// A trail of soft smoke puffs left hanging in the air, swelling as they fade
	for (int32 Index = CountThisFrame(28.f, DeltaSeconds); Index > 0; --Index)
	{
		FParticle Puff;
		Puff.Shape = EShape::Ball;
		Puff.Location = Nozzle + Way * 30.f * Size + FMath::VRand() * 15.f * Size;
		Puff.Velocity = Way * 80.f + FMath::VRand() * 40.f + FVector(0.f, 0.f, 40.f);
		Puff.Drag = 1.5f;
		Puff.Life = FMath::FRandRange(0.6f, 1.f);
		Puff.StartSize = FVector(0.55f * Size);
		Puff.EndSize = FVector(FMath::FRandRange(1.4f, 2.f) * Size);
		Puff.Color = FMath::FRand() < 0.7f ? Smoke : SmokeDark;
		Puff.Glow = 0.75f;
		Puff.Opacity = 0.42f;
		Puff.FadeIn = 0.08f;
		Spawn(Puff);
	}
	if (FMath::FRand() < 14.f * DeltaSeconds)
	{
		Spray(EShape::Star, Nozzle, 1, Way, 35.f, FVector2D(200.f, 500.f), FVector2D(2.5f, 4.5f), FVector2D(0.25f, 0.45f), FireYellow, 8.f, 200.f);
	}
}

void ARRRandomEffects::RocketBlast(const FVector& Where, const FVector& Normal, float Size)
{
	const FVector Up = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	const FRotator Across = FRotationMatrix::MakeFromZ(Up).Rotator();
	// A white flash and a cartoon fireball: balls of flame swelling out from the hit
	FParticle Flash;
	Flash.Shape = EShape::Ball;
	Flash.Location = Where + Up * 30.f * Size;
	Flash.Life = 0.16f;
	Flash.StartSize = FVector(1.5f * Size);
	Flash.EndSize = FVector(3.4f * Size);
	Flash.Color = FLinearColor::White;
	Flash.Glow = 7.f;
	Flash.Opacity = 0.6f;
	Flash.FadeIn = 0.01f;
	Spawn(Flash);
	for (int32 Index = FMath::RoundToInt(16.f * Size); Index > 0; --Index)
	{
		FParticle Ball;
		Ball.Shape = EShape::Ball;
		const FVector Out = FMath::VRandCone(Up, FMath::DegreesToRadians(75.f));
		Ball.Location = Where + Out * FMath::FRandRange(20.f, 70.f) * Size;
		Ball.Velocity = Out * FMath::FRandRange(250.f, 520.f) * Size;
		Ball.Drag = 4.5f;
		Ball.Gravity = -120.f;
		Ball.Life = FMath::FRandRange(0.45f, 0.75f);
		Ball.StartSize = FVector(FMath::FRandRange(0.9f, 1.5f) * Size);
		Ball.EndSize = FVector(FMath::FRandRange(1.6f, 2.4f) * Size);
		Ball.Color = FireColor(FMath::FRand() * 0.9f);
		Ball.Glow = 4.f;
		Ball.Opacity = 0.9f;
		Ball.FadeIn = 0.02f;
		Spawn(Ball);
	}
	// Smoke rolling up after the fire
	for (int32 Index = FMath::RoundToInt(12.f * Size); Index > 0; --Index)
	{
		FParticle Puff;
		Puff.Shape = EShape::Ball;
		const FVector Out = FMath::VRandCone(Up, FMath::DegreesToRadians(70.f));
		Puff.Location = Where + Out * 60.f * Size;
		Puff.Velocity = Out * FMath::FRandRange(150.f, 350.f) * Size + FVector(0.f, 0.f, 120.f);
		Puff.Drag = 2.f;
		Puff.Gravity = -90.f;
		Puff.Life = FMath::FRandRange(1.f, 1.6f);
		Puff.StartSize = FVector(1.f * Size);
		Puff.EndSize = FVector(FMath::FRandRange(2.4f, 3.4f) * Size);
		Puff.Color = FMath::FRand() < 0.5f ? Smoke : SmokeDark;
		Puff.Glow = 0.7f;
		Puff.Opacity = 0.5f;
		Puff.FadeIn = 0.25f;
		Spawn(Puff);
	}
	// Shock rings along the surface: hot, then smoke
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FParticle Ring;
		Ring.Shape = EShape::Ring;
		Ring.Location = Where + Up * (8.f + Index * 6.f);
		Ring.Life = 0.45f + Index * 0.25f;
		Ring.StartSize = FVector(1.f * Size, 1.f * Size, 3.f);
		Ring.EndSize = FVector((7.f - Index * 2.f) * Size, (7.f - Index * 2.f) * Size, 1.5f);
		Ring.Turn = Across;
		Ring.Color = Index == 0 ? FireYellow : Smoke;
		Ring.Glow = Index == 0 ? 6.f : 1.2f;
		Ring.Opacity = Index == 0 ? 1.f : 0.6f;
		Ring.FadeIn = 0.01f;
		Spawn(Ring);
	}
	StarImpact(Where, Up, 1.6f * Size, FireOrange);
	Spray(EShape::Star, Where + Up * 20.f, FMath::RoundToInt(26.f * Size), Up, 70.f, FVector2D(500.f, 1100.f) * Size, FVector2D(4.f, 8.f),
		FVector2D(0.5f, 0.9f), FireYellow, 8.f, 900.f);
	Spray(EShape::Die, Where + Up * 20.f, FMath::RoundToInt(5.f * Size), Up, 60.f, FVector2D(400.f, 800.f), FVector2D(0.12f, 0.2f),
		FVector2D(0.7f, 1.1f), Candy[FMath::RandRange(0, UE_ARRAY_COUNT(Candy) - 1)], 1.5f, 1300.f);
}

void ARRRandomEffects::Footstep(const FVector& Where, float Size)
{
	// Dust puffing out low along the ground, round the foot
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const float Angle = UE_TWO_PI * (Index + FMath::FRand() * 0.6f) / 12.f;
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		FParticle Puff;
		Puff.Shape = EShape::Ball;
		Puff.Location = Where + Out * 50.f * Size + FVector(0.f, 0.f, 15.f * Size);
		Puff.Velocity = (Out * FMath::FRandRange(220.f, 420.f) + FVector(0.f, 0.f, FMath::FRandRange(30.f, 110.f))) * Size;
		Puff.Drag = 3.5f;
		Puff.Life = FMath::FRandRange(0.5f, 0.85f);
		Puff.StartSize = FVector(0.45f * Size);
		Puff.EndSize = FVector(FMath::FRandRange(1.f, 1.4f) * Size);
		Puff.Color = Dust;
		Puff.Glow = 0.7f;
		Puff.Opacity = 0.45f;
		Puff.FadeIn = 0.05f;
		Spawn(Puff);
	}
	FParticle Ring;
	Ring.Shape = EShape::Ring;
	Ring.Location = Where + FVector(0.f, 0.f, 6.f);
	Ring.Life = 0.4f;
	Ring.StartSize = FVector(0.8f * Size, 0.8f * Size, 2.f);
	Ring.EndSize = FVector(3.2f * Size, 3.2f * Size, 1.f);
	Ring.Color = Dust;
	Ring.Glow = 1.2f;
	Ring.Opacity = 0.55f;
	Ring.FadeIn = 0.01f;
	Spawn(Ring);
}
