#include "RRRandomTigerSpirit.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomWreckage.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The jump attack on the tiger's rig (scale 1, mesh space): both paws hit the ground PawReach ahead of the rig's origin
	// and PawHeight up at PawDownAt seconds in, and the hips end HipsLeap ahead (char-pipeline: measured in Blender)
	constexpr float PawReach = 77.4f;
	constexpr float PawHeight = 9.5f;
	constexpr float PawDownAt = 1.7f;
	constexpr float HipsLeap = 50.f;
	// The tiger's model is 100 units tall at scale 1
	constexpr float ModelHeight = 100.f;
	// How much of the paws' height the giant crouches into the ground so they meet it
	constexpr float PawSink = 0.6f;
	const FLinearColor SpiritOrange(1.f, 0.55f, 0.1f);
	const FName GlowParameter(TEXT("HitFlash"));
	const FName GlowColorParameter(TEXT("HitFlashColor"));

	float Ease(float Alpha)
	{
		return FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(Alpha, 0.f, 1.f), 2.f);
	}
}

ARRRandomTigerSpirit::ARRRandomTigerSpirit()
{
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	// The rig faces +Y; the actor faces +X
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Roar(TEXT("/Game/Characters/Tiger/Anims/A_Tiger_Roar.A_Tiger_Roar"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack(TEXT("/Game/Characters/Tiger/Anims/A_Tiger_JumpAttack.A_Tiger_JumpAttack"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Tiger/Anims/A_Tiger_Idle.A_Tiger_Idle"));
	RoarAnimation = Roar.Succeeded() ? Roar.Object : nullptr;
	AttackAnimation = Attack.Succeeded() ? Attack.Object : nullptr;
	IdleAnimation = Idle.Succeeded() ? Idle.Object : nullptr;
}

void ARRRandomTigerSpirit::BeginPlay()
{
	FullScale = GiantHeight / ModelHeight;
	Body->SetVisibility(false);
	Super::BeginPlay();
}

void ARRRandomTigerSpirit::OnStarted()
{
	Body->SetVisibility(true);
	PhaseTime = 0.f;
	// It grows out of the tiger where it stands, the tiger's body and look
	USkeletalMeshComponent* TigerBody = Brawler->GetMesh();
	Body->SetSkeletalMeshAsset(TigerBody->GetSkeletalMeshAsset());
	for (int32 Slot = 0; Slot < TigerBody->GetNumMaterials(); ++Slot)
	{
		UMaterialInterface* Material = TigerBody->GetMaterial(Slot);
		UMaterialInstanceDynamic* TigerMaterial = Cast<UMaterialInstanceDynamic>(Material);
		UMaterialInstanceDynamic* Own = Body->CreateDynamicMaterialInstance(Slot, TigerMaterial && TigerMaterial->Parent ? TigerMaterial->Parent.Get() : Material);
		if (Own)
		{
			// Dice Land's softer palette, if the tiger has it
			if (TigerMaterial)
			{
				for (const TCHAR* Name : { TEXT("Pastel"), TEXT("PastelDesaturate"), TEXT("PastelLift") })
				{
					float Value;
					if (TigerMaterial->GetScalarParameterValue(FName(Name), Value))
					{
						Own->SetScalarParameterValue(FName(Name), Value);
					}
				}
			}
			Own->SetVectorParameterValue(GlowColorParameter, SpiritOrange);
			Materials.Add(Own);
		}
	}
	if (UMaterialInterface* Ghost = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/DiceFX/M_FXGhost.M_FXGhost")))
	{
		for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
		{
			UMaterialInstanceDynamic* Own = UMaterialInstanceDynamic::Create(Ghost, this);
			Own->SetVectorParameterValue(TEXT("Color"), SpiritOrange);
			Own->SetScalarParameterValue(TEXT("Opacity"), 0.35f);
			Own->SetScalarParameterValue(TEXT("Glow"), 1.5f);
			GhostMaterials.Add(Own);
		}
	}
	StartScale = TigerBody->GetComponentScale().Z;
	Feet = StartFeet;
	Yaw = Brawler->GetActorRotation().Yaw;
	Place(StartScale);
	PlayBody(RoarAnimation, false);
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->GiantBurst(Feet, 6.f, SpiritOrange);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s calls the tiger spirit for %d strikes."), *Brawler->GetName(), Left);
}

void ARRRandomTigerSpirit::PlayBody(UAnimSequence* Animation, bool bLoop, float Rate, float From)
{
	if (!Animation)
	{
		return;
	}
	Body->PlayAnimation(Animation, bLoop);
	Body->SetPlayRate(Rate);
	Body->SetPosition(From, false);
}

void ARRRandomTigerSpirit::Place(float Scale)
{
	SetActorLocationAndRotation(Feet, FRotator(0.f, Yaw, 0.f));
	Body->SetRelativeScale3D(FVector(Scale));
}

FVector ARRRandomTigerSpirit::GetDefaultSpot() const
{
	return Feet + FRotator(0.f, Yaw, 0.f).Vector() * PawReach * FullScale * 1.5f;
}

void ARRRandomTigerSpirit::StartAction(const FVector& Target)
{
	// Facing the spot from where it stands (straight on if the spot is right under it)
	const FVector Toward = (Target - Feet) * FVector(1.f, 1.f, 0.f);
	const float NewYaw = Toward.Size() > PawReach * FullScale * 0.3f ? Toward.Rotation().Yaw : Yaw;
	--Left;
	MulticastStrike(Target, NewYaw);
}

void ARRRandomTigerSpirit::MulticastStrike_Implementation(FVector_NetQuantize Target, float NewYaw)
{
	if (!HasStarted() || Phase == EPhase::Shrink || Phase == EPhase::Gone)
	{
		return;
	}
	// The floor under the spot, and the warning there on every machine
	StrikeSpot = FindGround(Target);
	ShowTargetHolo(StrikeSpot, AreaRadius);
	// Placed so that the paws, PawReach ahead of it at the bottom of the leap, come down on the spot
	if (Phase == EPhase::Grow)
	{
		Place(FullScale);
	}
	YawFrom = Yaw;
	YawTo = NewYaw;
	LeapFrom = Feet;
	LeapTo = StrikeSpot - FRotator(0.f, NewYaw, 0.f).Vector() * PawReach * FullScale - FVector(0.f, 0.f, PawHeight * FullScale * PawSink);
	Phase = EPhase::Strike;
	PhaseTime = 0.f;
	bImpactDone = false;
	PlayBody(AttackAnimation, false, StrikeRate, StrikeFrom);
}

void ARRRandomTigerSpirit::OnEnding()
{
	if (Phase != EPhase::Shrink && Phase != EPhase::Gone)
	{
		Phase = EPhase::Shrink;
		PhaseTime = 0.f;
		PlayBody(RoarAnimation, false, 1.5f);
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->GiantShrink(Feet, 6.f);
		}
	}
}

void ARRRandomTigerSpirit::TickUltimate(float DeltaSeconds)
{
	PhaseTime += DeltaSeconds;
	// Glowing with the tiger's spirit, a slow pulse
	const float Pulse = 0.25f + 0.15f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.f);
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		Material->SetScalarParameterValue(GlowParameter, Phase == EPhase::Gone ? 0.f : Pulse);
	}

	switch (Phase)
	{
	case EPhase::Grow:
	{
		Place(FMath::Lerp(StartScale, FullScale, Ease(PhaseTime / FMath::Max(GrowTime, 0.01f))));
		if (PhaseTime >= GrowTime)
		{
			Phase = EPhase::Idle;
			PhaseTime = 0.f;
			PlayBody(IdleAnimation, true);
		}
		break;
	}
	case EPhase::Strike:
	{
		const float AnimTime = StrikeFrom + PhaseTime * StrikeRate;
		// Leaping to its place while the paws go up, turning first
		const float LeapEnd = (PawDownAt - StrikeFrom) / StrikeRate * 0.85f;
		Feet = FMath::Lerp(LeapFrom, LeapTo, Ease(PhaseTime / LeapEnd));
		Yaw = YawFrom + FMath::FindDeltaAngleDegrees(YawFrom, YawTo) * Ease(PhaseTime / (LeapEnd * 0.5f));
		Place(FullScale);
		if (!bImpactDone && AnimTime >= PawDownAt)
		{
			bImpactDone = true;
			Impact();
		}
		if (AnimTime >= StrikeTo)
		{
			// The leap carried the hips forward; stand there, where the idle has them
			Feet = LeapTo + FRotator(0.f, Yaw, 0.f).Vector() * HipsLeap * FullScale + FVector(0.f, 0.f, PawHeight * FullScale * PawSink);
			Place(FullScale);
			Phase = EPhase::Idle;
			PhaseTime = 0.f;
			PlayBody(IdleAnimation, true);
		}
		break;
	}
	case EPhase::Shrink:
	{
		Place(FMath::Lerp(FullScale, 0.01f, Ease(PhaseTime / FMath::Max(ShrinkTime, 0.01f))));
		if (PhaseTime >= ShrinkTime)
		{
			Phase = EPhase::Gone;
			Body->SetVisibility(false);
			if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
			{
				if (Brawler)
				{
					Effects->TeleportPoof(Brawler->GetActorLocation(), SpiritOrange);
				}
			}
			Done();
		}
		break;
	}
	default:
		break;
	}

	UpdateGhost();

	// A haze of the tiger's spirit round its feet
	if (Phase != EPhase::Gone)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->GiantAura(Feet, Body->GetRelativeScale3D().Z / 3.f, GiantHeight * Body->GetRelativeScale3D().Z / FullScale, SpiritOrange, DeltaSeconds);
		}
	}
}

void ARRRandomTigerSpirit::UpdateGhost()
{
	const APlayerController* Local = GetWorld()->GetFirstPlayerController();
	if (!Local || !Local->PlayerCameraManager || GhostMaterials.Num() != Materials.Num() || Phase == EPhase::Gone)
	{
		return;
	}
	// Inside the space the giant takes up, in its own frame: its body, and ahead of it as far as its paws reach (they come
	// down right on the brawlers it strikes); with a little give so it doesn't flicker
	const FVector Camera = GetActorTransform().InverseTransformPosition(Local->PlayerCameraManager->GetCameraLocation());
	const float Height = Body->GetRelativeScale3D().Z * ModelHeight;
	const float Give = GhostMargin * (bGhost ? 1.5f : 1.f);
	const FBox Space(FVector(-0.45f * Height, -0.4f * Height, -Height), FVector((PawReach / ModelHeight + 0.25f) * Height, 0.4f * Height, 1.15f * Height));
	const bool bWantGhost = Space.ExpandBy(Give).IsInside(Camera);
	if (bWantGhost != bGhost)
	{
		bGhost = bWantGhost;
		for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
		{
			Body->SetMaterial(Slot, bGhost ? GhostMaterials[Slot].Get() : Materials[Slot].Get());
		}
	}
}

void ARRRandomTigerSpirit::Impact()
{
	ReleaseTargetHolo();
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->TigerStrike(StrikeSpot, AreaRadius, SpiritOrange);
	}
	if (!HasAuthority() || !Brawler)
	{
		return;
	}
	ARRRandomWreckage::Wreck(this, StrikeSpot, StrikeSpot, AreaRadius * WreckShare, true);
	const TArray<ARRRandomCharacter*> Victims = FindEnemiesNear(StrikeSpot, AreaRadius, GiantHeight);
	for (ARRRandomCharacter* Victim : Victims)
	{
		const FVector Where = Victim->GetActorLocation();
		const float Distance = FVector::Dist2D(Where, StrikeSpot);
		const float Reach = AreaRadius + Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float HealthBefore = Victim->GetHealth();
		const FVector Out = Distance > 1.f ? ((Where - StrikeSpot) * FVector(1.f, 1.f, 0.f)).GetSafeNormal() : FRotator(0.f, Yaw, 0.f).Vector();
		const float Damage = StrikeDamage * FMath::Lerp(1.f, EdgeDamageShare, FMath::Clamp(Distance / Reach, 0.f, 1.f));
		FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Where, -Out);
		UGameplayStatics::ApplyPointDamage(Victim, Damage, Out, Hit, Brawler->GetController(), this, UDamageType::StaticClass());
		if (Victim->IsAlive())
		{
			Victim->LaunchCharacter(Out * StrikeKnock.X + FVector(0.f, 0.f, StrikeKnock.Y), true, true);
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: tiger spirit hits %s %.0f away: %.0f -> %.0f%s."), *Victim->GetName(), Distance, HealthBefore,
			Victim->GetHealth(), Victim->IsAlive() ? TEXT("") : TEXT(", knocked out"));
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the tiger spirit strikes at %s, hitting %d (%d strikes left)."), *StrikeSpot.ToCompactString(), Victims.Num(), Left);
}
