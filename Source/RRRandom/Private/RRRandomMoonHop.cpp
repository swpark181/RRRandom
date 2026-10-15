#include "RRRandomMoonHop.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor MoonSilver(0.75f, 0.85f, 1.f);

	UAnimSequence* FindRabbitAnim(const TCHAR* Name)
	{
		// The finder keeps the path's pointer and looks it up later: the string has to outlive it
		const FString Path = FString::Printf(TEXT("/Game/Characters/Rabbit/Anims/A_Rabbit_%s.A_Rabbit_%s"), Name, Name);
		ConstructorHelpers::FObjectFinderOptional<UAnimSequence> Finder(*Path);
		return Finder.Succeeded() ? Finder.Get() : nullptr;
	}
}

ARRRandomMoonHop::ARRRandomMoonHop()
{
	AreaRadius = 380.f;
	CursorHoloColor = FLinearColor(0.35f, 0.6f, 1.f);
	TargetHoloColor = FLinearColor(0.55f, 0.25f, 1.f);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	// The rig faces +Y; the actor faces +X
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// Mixamo, on the rabbit's own rig (char-pipeline/scripts/ue_import_extra_anims.py)
	JumpAnimation = FindRabbitAnim(TEXT("JumpUp"));
	FlipAnimation = FindRabbitAnim(TEXT("FrontFlip"));
	FallAnimation = FindRabbitAnim(TEXT("FallIdle"));
	LandAnimation = FindRabbitAnim(TEXT("HardLanding"));
	IdleAnimation = FindRabbitAnim(TEXT("Idle"));
}

void ARRRandomMoonHop::BeginPlay()
{
	Body->SetVisibility(false);
	Super::BeginPlay();
}

void ARRRandomMoonHop::OnStarted()
{
	// The rabbit's own body and look, a size bigger so it reads from high up
	USkeletalMeshComponent* RabbitBody = Brawler->GetMesh();
	Body->SetSkeletalMeshAsset(RabbitBody->GetSkeletalMeshAsset());
	for (int32 Slot = 0; Slot < RabbitBody->GetNumMaterials(); ++Slot)
	{
		Body->SetMaterial(Slot, RabbitBody->GetMaterial(Slot));
	}
	Body->SetRelativeScale3D(FVector(RabbitBody->GetComponentScale().Z * BodyScale));
	Body->SetVisibility(true);
	Feet = StartFeet;
	Yaw = Brawler->GetActorRotation().Yaw;
	SetActorLocationAndRotation(Feet, FRotator(0.f, Yaw, 0.f));
	PlayBody(JumpAnimation, false, 0.6f);
	Phase = EPhase::GetReady;
	PhaseTime = 0.f;
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->MoonFlash(Feet + FVector(0.f, 0.f, 150.f), 0.6f);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s calls the moon hop for %d hops."), *Brawler->GetName(), Left);
}

void ARRRandomMoonHop::PlayBody(UAnimSequence* Animation, bool bLoop, float Rate, float From)
{
	if (!Animation)
	{
		return;
	}
	Body->PlayAnimation(Animation, bLoop);
	Body->SetPlayRate(Rate);
	Body->SetPosition(From, false);
}

FVector ARRRandomMoonHop::GetDefaultSpot() const
{
	return FindGround(Feet + FRotator(0.f, Yaw, 0.f).Vector() * AreaRadius * 2.f);
}

void ARRRandomMoonHop::StartAction(const FVector& Target)
{
	--Left;
	MulticastHop(Target);
}

void ARRRandomMoonHop::MulticastHop_Implementation(FVector_NetQuantize Target)
{
	if (!HasStarted() || Phase == EPhase::Gone)
	{
		return;
	}
	HopFrom = Feet;
	HopTo = FindGround(Target);
	ShowTargetHolo(HopTo, AreaRadius);
	const FVector Toward = (HopTo - HopFrom) * FVector(1.f, 1.f, 0.f);
	if (Toward.Size() > 50.f)
	{
		Yaw = Toward.Rotation().Yaw;
	}
	Phase = EPhase::Hop;
	PhaseTime = 0.f;
	bFalling = false;
	bMoonShown = false;
	// The flip fills the way up
	PlayBody(FlipAnimation, false, FlipAnimation ? FlipAnimation->GetPlayLength() / (HopTime * 0.55f) : 1.f);
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->DiceLandBurst(HopFrom + FVector(0.f, 0.f, 30.f), 40.f);
	}
}

void ARRRandomMoonHop::OnEnding()
{
	if (Phase == EPhase::Gone)
	{
		return;
	}
	Phase = EPhase::Gone;
	Body->SetVisibility(false);
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->TeleportPoof(Feet + FVector(0.f, 0.f, 90.f), MoonSilver);
	}
	// The rabbit is where the last hop came down
	if (HasAuthority() && Brawler && Brawler->IsAlive())
	{
		const float HalfHeight = Brawler->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Brawler->TeleportTo(Feet + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, Yaw, 0.f), false, true);
	}
	Done();
}

void ARRRandomMoonHop::TickUltimate(float DeltaSeconds)
{
	PhaseTime += DeltaSeconds;
	switch (Phase)
	{
	case EPhase::GetReady:
		if (PhaseTime >= ReadyTime)
		{
			Phase = EPhase::Ready;
			PlayBody(IdleAnimation, true);
		}
		break;
	case EPhase::Hop:
	{
		const float Alpha = FMath::Clamp(PhaseTime / HopTime, 0.f, 1.f);
		// Up quickly, a moment's hang at the top, down hard: a parabola eased in time
		const float Height = HopHeight * 4.f * Alpha * (1.f - Alpha);
		Feet = FMath::Lerp(HopFrom, HopTo, FMath::InterpEaseInOut(0.f, 1.f, Alpha, 1.6f)) + FVector(0.f, 0.f, Height);
		SetActorLocationAndRotation(Feet, FRotator(0.f, Yaw, 0.f));
		ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
		if (!bMoonShown && Alpha >= 0.45f)
		{
			// The full moon behind it at the top
			bMoonShown = true;
			if (Effects)
			{
				Effects->MoonFlash(Feet + FVector(0.f, 0.f, 120.f), 2.2f);
			}
		}
		if (!bFalling && Alpha >= 0.55f)
		{
			bFalling = true;
			PlayBody(FallAnimation, true);
		}
		if (Effects)
		{
			Effects->FlightTrail(Feet + FVector(0.f, 0.f, 120.f), (HopTo - HopFrom) / HopTime, MoonSilver, DeltaSeconds);
		}
		if (Alpha >= 1.f)
		{
			Feet = HopTo;
			SetActorLocationAndRotation(Feet, FRotator(0.f, Yaw, 0.f));
			Land();
			Phase = EPhase::Land;
			PhaseTime = 0.f;
			PlayBody(LandAnimation, false, 1.8f, 0.55f);
		}
		break;
	}
	case EPhase::Land:
		if (PhaseTime >= LandTime)
		{
			Phase = EPhase::Ready;
			PlayBody(IdleAnimation, true);
		}
		break;
	default:
		break;
	}
}

void ARRRandomMoonHop::Land()
{
	ReleaseTargetHolo();
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->TigerStrike(Feet, AreaRadius, MoonSilver);
	}
	if (!HasAuthority() || !Brawler)
	{
		return;
	}
	const TArray<ARRRandomCharacter*> Victims = FindEnemiesNear(Feet, AreaRadius, HopHeight);
	for (ARRRandomCharacter* Victim : Victims)
	{
		const FVector Where = Victim->GetActorLocation();
		const float Distance = FVector::Dist2D(Where, Feet);
		const float Reach = AreaRadius + Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float HealthBefore = Victim->GetHealth();
		const FVector Out = Distance > 1.f ? ((Where - Feet) * FVector(1.f, 1.f, 0.f)).GetSafeNormal() : FRotator(0.f, Yaw, 0.f).Vector();
		const float Damage = StompDamage * FMath::Lerp(1.f, EdgeDamageShare, FMath::Clamp(Distance / Reach, 0.f, 1.f));
		FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Where, -Out);
		UGameplayStatics::ApplyPointDamage(Victim, Damage, Out, Hit, Brawler->GetController(), this, UDamageType::StaticClass());
		if (Victim->IsAlive())
		{
			Victim->LaunchCharacter(Out * StompKnock.X + FVector(0.f, 0.f, StompKnock.Y), true, true);
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: moon hop hits %s %.0f away: %.0f -> %.0f%s."), *Victim->GetName(), Distance, HealthBefore,
			Victim->GetHealth(), Victim->IsAlive() ? TEXT("") : TEXT(", knocked out"));
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the moon hop lands at %s, hitting %d (%d hops left)."), *Feet.ToCompactString(), Victims.Num(), Left);
}
