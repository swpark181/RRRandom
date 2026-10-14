#include "RRRandomTigerSpirit.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomGameState.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
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
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	// The rig faces +Y; the actor faces +X
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	auto MakeHolo = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Holo = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Holo->SetupAttachment(RootComponent);
		Holo->SetUsingAbsoluteLocation(true);
		Holo->SetUsingAbsoluteRotation(true);
		Holo->SetUsingAbsoluteScale(true);
		Holo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Holo->SetCastShadow(false);
		Holo->SetVisibility(false);
		return Holo;
	};
	CursorHolo = MakeHolo(TEXT("CursorHolo"));
	TargetHolo = MakeHolo(TEXT("TargetHolo"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		CursorHolo->SetStaticMesh(Cylinder.Object);
		TargetHolo->SetStaticMesh(Cylinder.Object);
	}
	Overview = CreateDefaultSubobject<UCameraComponent>(TEXT("Overview"));
	Overview->SetupAttachment(Root);
	Overview->SetUsingAbsoluteLocation(true);
	Overview->SetUsingAbsoluteRotation(true);
	Overview->SetFieldOfView(70.f);

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Roar(TEXT("/Game/Characters/Tiger/Anims/A_Tiger_Roar.A_Tiger_Roar"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack(TEXT("/Game/Characters/Tiger/Anims/A_Tiger_JumpAttack.A_Tiger_JumpAttack"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Tiger/Anims/A_Tiger_Idle.A_Tiger_Idle"));
	RoarAnimation = Roar.Succeeded() ? Roar.Object : nullptr;
	AttackAnimation = Attack.Succeeded() ? Attack.Object : nullptr;
	IdleAnimation = Idle.Succeeded() ? Idle.Object : nullptr;
}

void ARRRandomTigerSpirit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomTigerSpirit, Tiger);
	DOREPLIFETIME(ARRRandomTigerSpirit, StrikesLeft);
	DOREPLIFETIME(ARRRandomTigerSpirit, bEnding);
}

void ARRRandomTigerSpirit::Setup(ARRRandomCharacter* InTiger, int32 Strikes)
{
	Tiger = InTiger;
	StrikesLeft = FMath::Max(1, Strikes);
}

void ARRRandomTigerSpirit::BeginPlay()
{
	Super::BeginPlay();
	FullScale = GiantHeight / ModelHeight;
	Body->SetVisibility(false);
	TryStart();
}

void ARRRandomTigerSpirit::TryStart()
{
	// On a joiner's machine the tiger may arrive a moment after its spirit
	if (bStarted || !Tiger)
	{
		return;
	}
	bStarted = true;
	Body->SetVisibility(true);
	PhaseTime = 0.f;
	// It grows out of the tiger where it stands, the tiger's body and look
	USkeletalMeshComponent* TigerBody = Tiger->GetMesh();
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
	Feet = Tiger->GetActorLocation() - FVector(0.f, 0.f, Tiger->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	GroundZ = Feet.Z;
	// The strike area's holograms (char-pipeline/scripts/ue_make_holo.py): the cursor's in the spirit's orange, the coming
	// strike's hotter and redder
	if (UMaterialInterface* Holo = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/DiceFX/M_FXHolo.M_FXHolo")))
	{
		CursorHoloMaterial = UMaterialInstanceDynamic::Create(Holo, this);
		CursorHoloMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.42f, 0.03f));
		TargetHoloMaterial = UMaterialInstanceDynamic::Create(Holo, this);
		TargetHoloMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.08f, 0.02f));
		TargetHoloMaterial->SetScalarParameterValue(TEXT("Glow"), 2.6f);
		for (int32 Slot = 0; Slot < TargetHolo->GetNumMaterials(); ++Slot)
		{
			TargetHolo->SetMaterial(Slot, TargetHoloMaterial);
			CursorHolo->SetMaterial(Slot, CursorHoloMaterial);
		}
	}
	Yaw = Tiger->GetActorRotation().Yaw;
	Place(StartScale);
	PlayBody(RoarAnimation, false);

	// Its player's view: high over the arena, turned the way that player looked (third person) or screen up (top view)
	const float ViewYaw = Tiger->IsThirdPerson() && Tiger->GetController() ? Tiger->GetControlRotation().Yaw : 0.f;
	// centered on everyone (the middle of the arena, about)
	FVector Middle = FVector::ZeroVector;
	int32 Count = 0;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		Middle += It->GetActorLocation();
		++Count;
	}
	Middle = Count > 0 ? Middle / Count : Feet;
	Middle.Z = Feet.Z;
	const FRotator ViewTurn(-60.f, ViewYaw, 0.f);
	Overview->SetWorldLocationAndRotation(Middle - ViewTurn.Vector() * 7000.f, ViewTurn);

	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->GiantBurst(Feet, 6.f, SpiritOrange);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s calls the tiger spirit for %d strikes."), *Tiger->GetName(), StrikesLeft);
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

bool ARRRandomTigerSpirit::IsReady() const
{
	return Phase == EPhase::Idle && StrikesLeft > 0 && !bEnding;
}

bool ARRRandomTigerSpirit::RequestStrike(const FVector& Target)
{
	if (!HasAuthority() || StrikesLeft <= 0 || bEnding)
	{
		return false;
	}
	// Mid-strike (or still growing): the spot waits for the next turn, the latest click counting
	if (!IsReady())
	{
		QueuedTarget = Target;
		bHasQueued = true;
		return true;
	}
	bHasQueued = false;
	// Facing the spot from where it stands (straight on if the spot is right under it)
	const FVector Toward = (Target - Feet) * FVector(1.f, 1.f, 0.f);
	const float NewYaw = Toward.Size() > PawReach * FullScale * 0.3f ? Toward.Rotation().Yaw : Yaw;
	--StrikesLeft;
	ReadyTime = 0.f;
	MulticastStrike(Target, NewYaw);
	return true;
}

void ARRRandomTigerSpirit::MulticastStrike_Implementation(FVector_NetQuantize Target, float NewYaw)
{
	TryStart();
	if (!bStarted || Phase == EPhase::Shrink || Phase == EPhase::Gone)
	{
		return;
	}
	// The floor under the spot, and the warning there on every machine
	StrikeSpot = FindGround(Target);
	PlaceHolo(TargetHolo, StrikeSpot);
	TargetHolo->SetVisibility(true);
	TargetHoloTime = 0.f;
	ImpactHoloFade = 0.f;
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

void ARRRandomTigerSpirit::OnRep_Ending()
{
	if (bEnding && Phase != EPhase::Shrink && Phase != EPhase::Gone)
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

void ARRRandomTigerSpirit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TryStart();
	if (!bStarted)
	{
		return;
	}
	PhaseTime += DeltaSeconds;
	const bool bServer = HasAuthority();
	// Glowing with the tiger's spirit, a slow pulse
	const float Pulse = 0.25f + 0.15f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.f);
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		Material->SetScalarParameterValue(GlowParameter, Phase == EPhase::Gone ? 0.f : Pulse);
	}

	// The tiger gone, knocked out (it can't be, but), or the match over: straight to shrinking
	if (bServer && !bEnding && (!Tiger || !Tiger->IsAlive() || ARRRandomGameState::IsMatchOver(this)))
	{
		StrikesLeft = 0;
		if (Phase != EPhase::Strike)
		{
			bEnding = true;
			OnRep_Ending();
		}
	}

	switch (Phase)
	{
	case EPhase::Grow:
	{
		const float Alpha = Ease(PhaseTime / FMath::Max(GrowTime, 0.01f));
		Place(FMath::Lerp(StartScale, FullScale, Alpha));
		if (PhaseTime >= GrowTime)
		{
			Phase = EPhase::Idle;
			PhaseTime = 0.f;
			PlayBody(IdleAnimation, true);
		}
		break;
	}
	case EPhase::Idle:
		if (bServer)
		{
			if (StrikesLeft <= 0)
			{
				bEnding = true;
				OnRep_Ending();
				break;
			}
			// A spot clicked during the last strike goes now
			if (bHasQueued)
			{
				RequestStrike(QueuedTarget);
				break;
			}
			// A bot's tiger picks where the enemies are; a player who leaves it too long gets a strike straight ahead
			ReadyTime += DeltaSeconds;
			const bool bPlayer = Tiger && Tiger->IsPlayerControlled();
			if (ReadyTime >= (bPlayer ? PlayerDelay : BotDelay))
			{
				RequestStrike(bPlayer ? Feet + FRotator(0.f, Yaw, 0.f).Vector() * PawReach * FullScale * 1.5f : PickTarget());
			}
		}
		break;
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
			ReadyTime = 0.f;
			PlayBody(IdleAnimation, true);
		}
		break;
	}
	case EPhase::Shrink:
	{
		const float Alpha = Ease(PhaseTime / FMath::Max(ShrinkTime, 0.01f));
		Place(FMath::Lerp(FullScale, 0.01f, Alpha));
		if (PhaseTime >= ShrinkTime)
		{
			Phase = EPhase::Gone;
			Body->SetVisibility(false);
			if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
			{
				if (Tiger)
				{
					Effects->TeleportPoof(Tiger->GetActorLocation(), SpiritOrange);
				}
			}
			if (bServer)
			{
				if (Tiger)
				{
					Tiger->EndUltimate();
				}
				SetLifeSpan(1.f);
			}
		}
		break;
	}
	default:
		break;
	}

	UpdateGhost();

	// The coming strike's warning: pulsing until the paws land, then fading quickly
	if (TargetHoloTime >= 0.f && TargetHoloMaterial)
	{
		TargetHoloTime += DeltaSeconds;
		float Opacity = 0.75f + 0.25f * FMath::Sin(TargetHoloTime * 14.f);
		if (bImpactDone || Phase != EPhase::Strike)
		{
			ImpactHoloFade += DeltaSeconds;
			Opacity *= FMath::Max(0.f, 1.f - ImpactHoloFade / 0.35f);
			if (ImpactHoloFade >= 0.35f)
			{
				TargetHolo->SetVisibility(false);
				TargetHoloTime = -1.f;
			}
		}
		TargetHoloMaterial->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}
	if (Phase == EPhase::Gone || Phase == EPhase::Shrink)
	{
		CursorHolo->SetVisibility(false);
	}

	// A haze of the tiger's spirit round its feet
	if (Phase != EPhase::Gone)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->GiantAura(Feet, Body->GetRelativeScale3D().Z / 3.f, GiantHeight * Body->GetRelativeScale3D().Z / FullScale, SpiritOrange, DeltaSeconds);
		}
	}
}

FVector ARRRandomTigerSpirit::FindGround(const FVector& Spot) const
{
	// Down onto the world's ground from a little above (not onto anyone, nor the top of something much taller)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TigerSpiritFloor), false, this);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	FCollisionObjectQueryParams Ground;
	Ground.AddObjectTypesToQuery(ECC_WorldStatic);
	const FVector From(Spot.X, Spot.Y, Spot.Z + 250.f);
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByObjectType(Hits, From, From - FVector(0.f, 0.f, 2500.f), Ground, Params);
	for (const FHitResult& Hit : Hits)
	{
		if (Hit.ImpactNormal.Z > 0.6f)
		{
			return Hit.ImpactPoint;
		}
	}
	return Spot;
}

void ARRRandomTigerSpirit::PlaceHolo(UStaticMeshComponent* Holo, const FVector& Spot) const
{
	// The cylinder is 100 across and 100 tall about its middle
	Holo->SetWorldLocationAndRotation(Spot + FVector(0.f, 0.f, HoloHeight * 0.5f + 3.f), FRotator::ZeroRotator);
	Holo->SetWorldScale3D(FVector(StrikeRadius / 50.f, StrikeRadius / 50.f, HoloHeight / 100.f));
}

void ARRRandomTigerSpirit::ShowCursorSpot(bool bShow, const FVector& Spot)
{
	const bool bVisible = bShow && bStarted && Phase != EPhase::Shrink && Phase != EPhase::Gone && StrikesLeft > 0;
	if (bVisible)
	{
		PlaceHolo(CursorHolo, Spot);
		if (CursorHoloMaterial)
		{
			CursorHoloMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.8f + 0.2f * FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f));
		}
	}
	if (CursorHolo->IsVisible() != bVisible)
	{
		CursorHolo->SetVisibility(bVisible);
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

FVector ARRRandomTigerSpirit::PickTarget() const
{
	// Every enemy is a candidate middle: the one with the most enemies within reach of a strike there
	TArray<FVector> Enemies;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && Tiger && It->GetTeam() != Tiger->GetTeam())
		{
			Enemies.Add(It->GetActorLocation() - FVector(0.f, 0.f, It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		}
	}
	FVector Best = Feet + FRotator(0.f, Yaw, 0.f).Vector() * PawReach * FullScale;
	int32 BestCount = 0;
	for (const FVector& Middle : Enemies)
	{
		int32 Count = 0;
		FVector Sum = FVector::ZeroVector;
		for (const FVector& Other : Enemies)
		{
			if (FVector::Dist2D(Middle, Other) < StrikeRadius * 1.6f)
			{
				++Count;
				Sum += Other;
			}
		}
		if (Count > BestCount)
		{
			BestCount = Count;
			Best = Sum / Count;
		}
	}
	return Best;
}

void ARRRandomTigerSpirit::Impact()
{
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->TigerStrike(StrikeSpot, StrikeRadius, SpiritOrange);
	}
	if (!HasAuthority() || !Tiger)
	{
		return;
	}
	int32 Hits = 0;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Victim = *It;
		if (!Victim->IsAlive() || Victim->GetTeam() == Tiger->GetTeam())
		{
			continue;
		}
		// Everyone of theirs inside the circle, flying over it or standing on something in it too: a column from a little
		// below the spot up past the giant's head
		const FVector Where = Victim->GetActorLocation();
		const float Distance = FVector::Dist2D(Where, StrikeSpot);
		const float Reach = StrikeRadius + Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (Distance > Reach || Where.Z < StrikeSpot.Z - StrikeRadius || Where.Z > StrikeSpot.Z + GiantHeight)
		{
			UE_LOG(LogTemp, Verbose, TEXT("RRRandom: tiger spirit misses %s, %.0f away (%+.0f up)."), *Victim->GetName(), Distance, Where.Z - StrikeSpot.Z);
			continue;
		}
		const float HealthBefore = Victim->GetHealth();
		const FVector Out = Distance > 1.f ? ((Where - StrikeSpot) * FVector(1.f, 1.f, 0.f)).GetSafeNormal() : FRotator(0.f, Yaw, 0.f).Vector();
		const float Damage = StrikeDamage * FMath::Lerp(1.f, EdgeDamageShare, FMath::Clamp(Distance / Reach, 0.f, 1.f));
		FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Where, -Out);
		UGameplayStatics::ApplyPointDamage(Victim, Damage, Out, Hit, Tiger->GetController(), this, UDamageType::StaticClass());
		if (Victim->IsAlive())
		{
			Victim->LaunchCharacter(Out * StrikeKnock.X + FVector(0.f, 0.f, StrikeKnock.Y), true, true);
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: tiger spirit hits %s %.0f away: %.0f -> %.0f%s."), *Victim->GetName(), Distance, HealthBefore,
			Victim->GetHealth(), Victim->IsAlive() ? TEXT("") : TEXT(", knocked out"));
		++Hits;
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the tiger spirit strikes at %s, hitting %d (%d strikes left)."), *StrikeSpot.ToCompactString(), Hits, StrikesLeft);
}
