#include "RRRandomProjectile.h"
#include "RRRandomCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName ColorParameter(TEXT("Color"));

	enum class EImpact : uint8 { None, Spark, Burst, Explosion };

	/** Effects per weapon tier: the stronger the gun's damage, the flashier its shots. */
	struct FTierLook
	{
		float GlowIntensity;
		float GlowRadius;
		EImpact Muzzle;
		float MuzzleScale;
		bool bTrail;
		float TrailScale;
		EImpact Impact;
		float ImpactScale;
	};

	const FTierLook TierLooks[] =
	{
		// Common: just the ball
		{ 0.f,   0.f,   EImpact::None,  0.f,  false, 0.f,  EImpact::None,      0.f },
		// Rare: glow and a spark where it lands
		{ 20.f,  200.f, EImpact::None,  0.f,  false, 0.f,  EImpact::Spark,     0.6f },
		// Epic: brighter, muzzle flash, trail, bigger burst
		{ 40.f,  280.f, EImpact::Spark, 0.5f, true,  0.4f, EImpact::Burst,     0.7f },
		// Legendary: everything, ending in an explosion
		{ 70.f,  380.f, EImpact::Burst, 0.6f, true,  0.6f, EImpact::Explosion, 0.6f },
	};

	// Templates may loop for previewing, so one-shot effects stop spawning after this and let their particles finish
	constexpr float OneShotSpawnTime = 0.25f;

	void SpawnOneShot(const UObject* Context, UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation, float Scale)
	{
		if (!System || Scale <= 0.f)
		{
			return;
		}
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Context, System, Location, Rotation, FVector(Scale));
		if (Effect)
		{
			TWeakObjectPtr<UNiagaraComponent> WeakEffect(Effect);
			FTimerHandle Handle;
			Context->GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakEffect]()
			{
				if (WeakEffect.IsValid())
				{
					WeakEffect->Deactivate();
				}
			}), OneShotSpawnTime, false);
		}
	}
}

ARRRandomProjectile::ARRRandomProjectile()
{
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	// Keep shots out of the mouse cursor trace and the camera boom. Shots still hit WorldDynamic, since the level's
	// floor is movable and shots angled down from cover must land on it; other shots are skipped in BeginPlay.
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->OnComponentHit.AddDynamic(this, &ARRRandomProjectile::OnHit);
	RootComponent = Collision;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeScale3D(FVector(0.25f));
	if (SphereMesh.Succeeded())
	{
		Visual->SetStaticMesh(SphereMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Visual->SetMaterial(0, ShapeMaterial.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 5000.f;
	Movement->MaxSpeed = 5000.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;

	InitialLifeSpan = 2.f;

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetCastShadows(false);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetVisibility(false);

	// Engine Niagara templates, so effects need no project assets
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SparkSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurstLightweight.DirectionalBurstLightweight"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> BurstSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ExplosionSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion.SimpleExplosion"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> RingSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TrailSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight"));
	SparkEffect = SparkSystem.Object;
	BurstEffect = BurstSystem.Object;
	ExplosionEffect = ExplosionSystem.Object;
	RingEffect = RingSystem.Object;
	TrailEffect = TrailSystem.Object;

	// Ticks only to drag a trail along
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ARRRandomProjectile::BeginPlay()
{
	Super::BeginPlay();

	const FTierLook& Look = TierLooks[static_cast<int32>(Tier)];
	if (Look.GlowIntensity > 0.f)
	{
		Glow->SetLightColor(FRRWeapon::GetTierColor(Tier));
		Glow->SetIntensity(Look.GlowIntensity);
		Glow->SetAttenuationRadius(Look.GlowRadius);
		Glow->SetVisibility(true);
	}
	SpawnOneShot(this, Look.Muzzle == EImpact::Spark ? SparkEffect : (Look.Muzzle == EImpact::Burst ? BurstEffect : nullptr), GetActorLocation(), GetActorRotation(), Look.MuzzleScale);
	if (Look.bTrail && TrailEffect)
	{
		Trail = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, TrailEffect, GetActorLocation(), GetActorRotation(), FVector(Look.TrailScale));
		SetActorTickEnabled(Trail != nullptr);
	}

	// Don't hit the shooter on the way out, nor anyone on the shooter's team
	if (APawn* Shooter = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
		if (const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(Shooter))
		{
			for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
			{
				if (It->GetTeam() == Brawler->GetTeam())
				{
					Collision->IgnoreActorWhenMoving(*It, true);
				}
			}
		}
	}

	// Shots pass through each other: both ways, since shots already in the air don't know about this one
	for (TActorIterator<ARRRandomProjectile> It(GetWorld()); It; ++It)
	{
		if (*It != this)
		{
			Collision->IgnoreActorWhenMoving(*It, true);
			It->Collision->IgnoreActorWhenMoving(this, true);
		}
	}

	if (MaxRange > 0.f && GetSpeed() > 0.f)
	{
		SetLifeSpan(MaxRange / GetSpeed());
	}

	if (UMaterialInstanceDynamic* Material = Visual->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(ColorParameter, ShotColor);
	}
}

void ARRRandomProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Let the trail's particles die out where they are
	if (Trail)
	{
		Trail->Deactivate();
		Trail = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ARRRandomProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Trail)
	{
		Trail->SetWorldLocation(GetActorLocation());
	}
}

float ARRRandomProjectile::GetSpeed() const
{
	return Movement->InitialSpeed;
}

void ARRRandomProjectile::SetSpeedMultiplier(float Multiplier)
{
	// Before the movement component initializes, which turns InitialSpeed into the starting velocity
	Movement->InitialSpeed *= Multiplier;
	Movement->MaxSpeed *= Multiplier;
}

void ARRRandomProjectile::SpawnImpactEffects(const FVector& Location, const FVector& Normal)
{
	const FTierLook& Look = TierLooks[static_cast<int32>(Tier)];
	const FRotator AwayFromSurface = Normal.Rotation();
	switch (Look.Impact)
	{
	case EImpact::Spark:
		SpawnOneShot(this, SparkEffect, Location, AwayFromSurface, Look.ImpactScale);
		break;
	case EImpact::Burst:
		SpawnOneShot(this, BurstEffect, Location, AwayFromSurface, Look.ImpactScale);
		break;
	case EImpact::Explosion:
		SpawnOneShot(this, ExplosionEffect, Location, AwayFromSurface, Look.ImpactScale);
		SpawnOneShot(this, RingEffect, Location, FRotator::ZeroRotator, Look.ImpactScale);
		break;
	default:
		break;
	}
}

void ARRRandomProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor && OtherActor != GetInstigator())
	{
		const FVector ShotDirection = GetActorForwardVector();
		const float Damage = FMath::RandRange(MinDamage, MaxDamage) * DamageMultiplier;
		UGameplayStatics::ApplyPointDamage(OtherActor, Damage, ShotDirection, Hit, GetInstigatorController(), this, UDamageType::StaticClass());

		if (OtherComp && OtherComp->IsSimulatingPhysics())
		{
			OtherComp->AddImpulseAtLocation(ShotDirection * ImpactImpulse, Hit.ImpactPoint, Hit.BoneName);
		}
	}
	SpawnImpactEffects(Hit.ImpactPoint, Hit.ImpactNormal);
	Destroy();
}
