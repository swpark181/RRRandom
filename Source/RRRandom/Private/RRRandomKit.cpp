#include "RRRandomKit.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomGameState.h"
#include "RRRandomGunComponent.h"
#include "RRRandomOre.h"
#include "RRRandomSpecies.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FRRKit MakeKits(int32 Species)
	{
		FRRKit Kit;
		switch (Species)
		{
		case RRSpecies::Fox:
			Kit.bBackstab = true;
			Kit.OwnDieChance = 0.2f;
			Kit.PassiveName = TEXT("BACKSTAB");
			Kit.PassiveLine = TEXT("+25% damage from behind and out of invisibility");
			Kit.DieName = TEXT("INVISIBLE DIE");
			Kit.DieLine = TEXT("invisible for 2s per pip, a faint shimmer to opponents");
			Kit.DieWord = TEXT("INVISIBLE");
			Kit.DieColor = FLinearColor::FromSRGBColor(FColor(189, 166, 240));
			break;
		case RRSpecies::Rabbit:
			Kit.MoveSpeedScale = 1.15f;
			Kit.OrePickupScale = 1.4f;
			Kit.JumpCount = 2;
			Kit.OwnDieChance = 0.2f;
			Kit.PassiveName = TEXT("HOP HOP");
			Kit.PassiveLine = TEXT("+15% speed, double jump, picks up ore from further");
			Kit.DieName = TEXT("SUPER HOP DIE");
			Kit.DieLine = TEXT("one super-high second jump per pip");
			Kit.DieWord = TEXT("HOP");
			Kit.DieColor = FLinearColor::FromSRGBColor(FColor(151, 201, 245));
			break;
		case RRSpecies::Squirrel:
			Kit.bCheekPouch = true;
			Kit.OwnDieChance = 0.2f;
			Kit.PassiveName = TEXT("CHEEK POUCH");
			Kit.PassiveLine = TEXT("knocked out, keeps its gun and a third of its ore");
			Kit.DieName = TEXT("ACORN SHIELD DIE");
			Kit.DieLine = TEXT("a shield of 10 per pip; in Meteor Rush, pulls ore in");
			Kit.DieWord = TEXT("SHIELD");
			Kit.DieColor = FLinearColor::FromSRGBColor(FColor(143, 221, 190));
			break;
		case RRSpecies::Tiger:
			Kit.HealthScale = 1.3f;
			Kit.MoveSpeedScale = 0.9f;
			Kit.KnockbackScale = 0.5f;
			Kit.GiantDieChance = 0.2f;
			Kit.PassiveName = TEXT("BEAST BODY");
			Kit.PassiveLine = TEXT("+30% health, pushed half as far, 10% slower");
			Kit.DieName = TEXT("GIANT DIE");
			Kit.DieLine = TEXT("only the tiger turns giant, twice as often");
			Kit.DieColor = FLinearColor(0.3f, 0.95f, 1.f);
			break;
		case RRSpecies::Dragon:
			Kit.bGlide = true;
			Kit.bOwnWings = true;
			Kit.FlightDieChance = 0.2f;
			Kit.PassiveName = TEXT("GLIDE");
			Kit.PassiveLine = TEXT("hold jump while falling to glide down");
			Kit.DieName = TEXT("FLIGHT DIE");
			Kit.DieLine = TEXT("only the dragon flies, twice as often");
			Kit.DieColor = FLinearColor(1.f, 0.5f, 0.85f);
			break;
		case RRSpecies::Rat:
		case RRSpecies::Cat:
			// The defense mode's villains roll nothing; the cat (a boss) is pushed about much less
			Kit.KnockbackScale = Species == RRSpecies::Cat ? 0.3f : 1.f;
			Kit.PassiveName = TEXT("VILLAIN");
			break;
		}
		return Kit;
	}

	// The red fox's is the fox's (RRSpecies::GetKitSpecies)
	const FRRKit Kits[RRSpecies::AllCount] = { MakeKits(0), MakeKits(1), MakeKits(2), MakeKits(3), MakeKits(4), MakeKits(RRSpecies::Fox),
		MakeKits(RRSpecies::Rat), MakeKits(RRSpecies::Cat) };

	// The invisible look (M_FXCloak): on the brawler's own screen and its teammates' a see-through body (outline plus some
	// fill), on its opponents' just the outline, fainter standing still than at full run; a hit flashes it up for
	// HitRevealTime
	constexpr float CloakSelfOpacity = 0.9f;
	constexpr float CloakSelfFill = 0.35f;
	constexpr float CloakAllyOpacity = 0.75f;
	constexpr float CloakAllyFill = 0.2f;
	constexpr float CloakEnemyStill = 0.3f;
	constexpr float CloakEnemyRunning = 0.55f;
	constexpr float CloakHitFlash = 0.9f;
	constexpr float HitRevealTime = 0.3f;
	constexpr float CloakFadeTime = 0.35f;
	// The ambush bonus waits this long for the shot that broke invisibility to land
	constexpr float AmbushWindow = 1.5f;
	const FLinearColor CloakColor(0.5f, 0.4f, 0.95f);
}

const FRRKit& RRKit::Get(int32 Species)
{
	return Kits[FMath::Clamp(Species, 0, RRSpecies::AllCount - 1)];
}

URRRandomKitComponent::URRRandomKitComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void URRRandomKitComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URRRandomKitComponent, CloakRemaining);
	DOREPLIFETIME(URRRandomKitComponent, SuperHops);
	DOREPLIFETIME(URRRandomKitComponent, Shield);
	DOREPLIFETIME(URRRandomKitComponent, ShieldMax);
	DOREPLIFETIME(URRRandomKitComponent, ShieldRemaining);
	DOREPLIFETIME(URRRandomKitComponent, MagnetRemaining);
}

void URRRandomKitComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Cloak = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/DiceFX/M_FXCloak.M_FXCloak")))
	{
		CloakMaterial = UMaterialInstanceDynamic::Create(Cloak, this);
		CloakMaterial->SetVectorParameterValue(TEXT("Color"), CloakColor);
	}
}

ARRRandomCharacter* URRRandomKitComponent::GetBrawler() const
{
	return Cast<ARRRandomCharacter>(GetOwner());
}

const FRRKit& URRRandomKitComponent::GetKit() const
{
	const ARRRandomCharacter* Brawler = GetBrawler();
	return RRKit::Get(Brawler ? Brawler->GetSpecies() : RRSpecies::Fox);
}

void URRRandomKitComponent::ApplyOwnDie(int32 Face)
{
	ARRRandomCharacter* Brawler = GetBrawler();
	if (!Brawler || !Brawler->IsAlive())
	{
		return;
	}
	switch (RRSpecies::GetKitSpecies(Brawler->GetSpecies()))
	{
	case RRSpecies::Fox:
		CloakRemaining = FMath::Max(CloakRemaining, Face * RRKit::CloakSecondsPerPip);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) turns invisible for %.0f s."), *Brawler->GetName(), Brawler->GetTeam(), CloakRemaining);
		break;
	case RRSpecies::Rabbit:
		SuperHops = FMath::Min(SuperHops + Face, RRKit::MaxSuperHops);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) has %d super hops."), *Brawler->GetName(), Brawler->GetTeam(), SuperHops);
		break;
	case RRSpecies::Squirrel:
		// A new shield tops up the old one rather than stacking
		Shield = FMath::Max(Shield, Face * RRKit::ShieldPerPip);
		ShieldMax = Shield;
		ShieldRemaining = RRKit::ShieldSeconds;
		if (ARRRandomGameState::IsMeteorRush(this))
		{
			MagnetRemaining = FMath::Max(MagnetRemaining, Face * RRKit::MagnetSecondsPerPip);
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) gets a %.0f shield, ore magnet %.0f s."), *Brawler->GetName(), Brawler->GetTeam(), Shield, MagnetRemaining);
		break;
	default:
		break;
	}
}

void URRRandomKitComponent::ClearPowers()
{
	CloakRemaining = 0.f;
	SuperHops = 0;
	Shield = 0.f;
	ShieldMax = 0.f;
	ShieldRemaining = 0.f;
	MagnetRemaining = 0.f;
	AmbushUntil = -1.f;
}

bool URRRandomKitComponent::IsHiddenFrom(int32 Team) const
{
	const ARRRandomCharacter* Brawler = GetBrawler();
	if (!IsCloaked() || !Brawler || Brawler->GetTeam() == Team || GetWorld()->GetTimeSeconds() < RevealUntil)
	{
		return false;
	}
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() == Team && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Brawler->GetActorLocation()) < CloakNoticeRange)
		{
			return false;
		}
	}
	return true;
}

void URRRandomKitComponent::OnShot()
{
	if (IsCloaked())
	{
		CloakRemaining = 0.f;
		AmbushUntil = GetWorld()->GetTimeSeconds() + AmbushWindow;
	}
}

bool URRRandomKitComponent::ConsumeAmbush()
{
	if (GetWorld()->GetTimeSeconds() < AmbushUntil)
	{
		AmbushUntil = -1.f;
		return true;
	}
	return false;
}

void URRRandomKitComponent::OnHit()
{
	RevealUntil = GetWorld()->GetTimeSeconds() + HitRevealTime;
}

bool URRRandomKitComponent::SpendSuperHop()
{
	if (SuperHops <= 0)
	{
		return false;
	}
	--SuperHops;
	return true;
}

float URRRandomKitComponent::AbsorbDamage(float Damage)
{
	if (Shield <= 0.f || Damage <= 0.f)
	{
		return Damage;
	}
	const float Taken = FMath::Min(Shield, Damage);
	Shield -= Taken;
	return Damage - Taken;
}

void URRRandomKitComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner()->HasAuthority())
	{
		CloakRemaining = FMath::Max(0.f, CloakRemaining - DeltaTime);
		if (Shield > 0.f)
		{
			ShieldRemaining -= DeltaTime;
			if (ShieldRemaining <= 0.f)
			{
				Shield = 0.f;
				ShieldRemaining = 0.f;
			}
		}
		if (MagnetRemaining > 0.f)
		{
			MagnetRemaining = FMath::Max(0.f, MagnetRemaining - DeltaTime);
			UpdateMagnet();
		}
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateCloakLook(DeltaTime);
		UpdateShieldLook(DeltaTime);
	}
}

void URRRandomKitComponent::UpdateMagnet()
{
	const ARRRandomCharacter* Brawler = GetBrawler();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Brawler || !Brawler->IsAlive() || Now < NextMagnetScan || ARRRandomGameState::IsPlayLocked(this))
	{
		return;
	}
	NextMagnetScan = Now + 0.15f;
	const FVector Feet = Brawler->GetActorLocation() - FVector(0.f, 0.f, Brawler->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	for (TActorIterator<ARRRandomOre> It(GetWorld()); It; ++It)
	{
		const FVector Offset = It->GetActorLocation() - Feet;
		if (It->IsResting() && Offset.Size2D() < MagnetRadius && Offset.Size2D() > 60.f && FMath::Abs(Offset.Z) < 300.f)
		{
			It->PullToward(Brawler);
		}
	}
}

void URRRandomKitComponent::UpdateCloakLook(float DeltaTime)
{
	ARRRandomCharacter* Brawler = GetBrawler();
	if (!Brawler)
	{
		return;
	}
	// Who is looking: this brawler's own player, a teammate, or an opponent (no brawler of one's own counts as one)
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	const ARRRandomCharacter* ViewerBrawler = Viewer ? Cast<ARRRandomCharacter>(Viewer->GetPawn()) : nullptr;
	const bool bSelf = Brawler->IsLocallyControlled() && Brawler->IsPlayerControlled();
	const bool bAlly = !bSelf && ViewerBrawler && ViewerBrawler->GetTeam() == Brawler->GetTeam();
	const bool bWanted = IsCloaked() && Brawler->IsAlive() && !ARRRandomGameState::IsMatchOver(this);
	const bool bForEnemy = bWanted && !bSelf && !bAlly;
	if (bWanted != bCloakShown || (bWanted && bForEnemy != bCloakForEnemy))
	{
		// A puff where it vanishes and where it comes back
		if (bWanted != bCloakShown)
		{
			if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
			{
				Effects->FoxPoof(Brawler->GetActorLocation(), 0.45f);
			}
		}
		SetCloakLook(bWanted, bForEnemy);
	}
	if (!bCloakShown || !CloakMaterial)
	{
		return;
	}
	CloakFadeIn = FMath::Min(1.f, CloakFadeIn + DeltaTime / CloakFadeTime);
	float Opacity = bSelf ? CloakSelfOpacity : bAlly ? CloakAllyOpacity
		: FMath::Lerp(CloakEnemyStill, CloakEnemyRunning, FMath::Clamp(Brawler->GetVelocity().Size2D() / 400.f, 0.f, 1.f));
	// Hit: it flickers into view (the newest damage number is how recent)
	float NewestHit = UE_BIG_NUMBER;
	for (const FRRDamagePopup& Popup : Brawler->GetDamagePopups())
	{
		NewestHit = FMath::Min(NewestHit, Popup.Age);
	}
	if (NewestHit < HitRevealTime)
	{
		Opacity = FMath::Max(Opacity, CloakHitFlash * (1.f - NewestHit / HitRevealTime));
	}
	float Fill = bSelf ? CloakSelfFill : bAlly ? CloakAllyFill : 0.f;
	// Fading out of view: from fully there to the shimmer
	Opacity = FMath::Lerp(1.f, Opacity, CloakFadeIn);
	Fill = FMath::Lerp(1.f, Fill, CloakFadeIn);
	CloakMaterial->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	CloakMaterial->SetScalarParameterValue(TEXT("Fill"), Fill);
	CloakMaterial->SetScalarParameterValue(TEXT("Glow"), bForEnemy ? 0.8f : 1.f);
}

void URRRandomKitComponent::SetCloakLook(bool bOn, bool bForEnemy)
{
	ARRRandomCharacter* Brawler = GetBrawler();
	USkeletalMeshComponent* Body = Brawler->GetMesh();
	UStaticMeshComponent* Gun = Brawler->GetGun();
	if (bOn && CloakMaterial)
	{
		if (!bCloakShown)
		{
			SavedBodyMaterials.Reset();
			for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
			{
				SavedBodyMaterials.Add(Body->GetMaterial(Slot));
				Body->SetMaterial(Slot, CloakMaterial);
			}
			SavedGunMaterials.Reset();
			for (int32 Slot = 0; Gun && Slot < Gun->GetNumMaterials(); ++Slot)
			{
				SavedGunMaterials.Add(Gun->GetMaterial(Slot));
				Gun->SetMaterial(Slot, CloakMaterial);
			}
			CloakFadeIn = 0.f;
		}
		// Opponents get no shadow and no toon outline to give it away
		Body->SetCastShadow(!bForEnemy);
		Body->SetRenderCustomDepth(!bForEnemy);
		if (Gun)
		{
			Gun->SetCastShadow(!bForEnemy);
			Gun->SetRenderCustomDepth(!bForEnemy);
		}
		bCloakShown = true;
		bCloakForEnemy = bForEnemy;
		return;
	}
	if (bCloakShown)
	{
		for (int32 Slot = 0; Slot < SavedBodyMaterials.Num() && Slot < Body->GetNumMaterials(); ++Slot)
		{
			Body->SetMaterial(Slot, SavedBodyMaterials[Slot]);
		}
		for (int32 Slot = 0; Gun && Slot < SavedGunMaterials.Num() && Slot < Gun->GetNumMaterials(); ++Slot)
		{
			Gun->SetMaterial(Slot, SavedGunMaterials[Slot]);
		}
		Body->SetCastShadow(true);
		Body->SetRenderCustomDepth(true);
		if (Gun)
		{
			Gun->SetCastShadow(true);
			Gun->SetRenderCustomDepth(true);
		}
	}
	SavedBodyMaterials.Reset();
	SavedGunMaterials.Reset();
	bCloakShown = false;
	bCloakForEnemy = false;
}

void URRRandomKitComponent::UpdateShieldLook(float DeltaTime)
{
	ARRRandomCharacter* Brawler = GetBrawler();
	const bool bShow = Brawler && Brawler->IsAlive() && Shield > 0.f;
	// A hit on the shield flashes it
	if (Shield < LastShield)
	{
		BubbleFlash = 1.f;
	}
	LastShield = Shield;
	BubbleFlash = FMath::Max(0.f, BubbleFlash - DeltaTime * 4.f);
	if (bShow && !Bubble)
	{
		// The engine sphere (100 across) in the Meteor Rush glass, round the capsule
		Bubble = NewObject<UStaticMeshComponent>(Brawler, TEXT("ShieldBubble"));
		Bubble->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Bubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bubble->SetCanEverAffectNavigation(false);
		Bubble->SetCastShadow(false);
		Bubble->SetupAttachment(Brawler->GetCapsuleComponent());
		Bubble->RegisterComponent();
		if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRGlass.M_MRGlass")))
		{
			BubbleMaterial = UMaterialInstanceDynamic::Create(Glass, this);
			BubbleMaterial->SetVectorParameterValue(TEXT("Tint"), GetKit().DieColor);
			BubbleMaterial->SetScalarParameterValue(TEXT("EdgeOpacity"), 0.85f);
			Bubble->SetMaterial(0, BubbleMaterial);
		}
	}
	if (!Bubble)
	{
		return;
	}
	Bubble->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}
	const UCapsuleComponent* Capsule = Brawler->GetCapsuleComponent();
	const float Radius = Capsule->GetUnscaledCapsuleRadius() * 1.9f;
	const float Tall = Capsule->GetUnscaledCapsuleHalfHeight() * 1.25f;
	const float Pulse = 1.f + 0.03f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.f);
	Bubble->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, Tall / 50.f) * Pulse);
	if (BubbleMaterial)
	{
		const float Share = ShieldMax > 0.f ? Shield / ShieldMax : 0.f;
		const bool bEnding = ShieldRemaining < 3.f && FMath::Frac(GetWorld()->GetTimeSeconds() * 3.f) < 0.5f;
		BubbleMaterial->SetScalarParameterValue(TEXT("Opacity"), (0.08f + 0.1f * Share) * (bEnding ? 0.4f : 1.f) + 0.3f * BubbleFlash);
		BubbleMaterial->SetScalarParameterValue(TEXT("Glow"), 0.6f + 1.f * BubbleFlash);
	}
}
