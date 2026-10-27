#include "RRRandomGunComponent.h"
#include "RRRandomCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "RRRandomSpecies.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Running (Shift, round 67): degrees the muzzle tips down at full run (ARRRandomCharacter::GetSprintShare). */
	constexpr float SprintMuzzleDown = 50.f;
	// The spit lasts this share of the pistol's time between shots for each of the gun's, within these
	constexpr float SpitShare = 0.14f;
	constexpr float SpitMinTime = 0.1f;
	constexpr float SpitMaxTime = 0.4f;

	/**
	 * Round 90: each shot the gun spits it out like a snake spitting out its prey, scaled about its grip (X along the gun):
	 * a quick gulp (shorter and fatter), a long stretch forward as it spits (longer and thinner), then a springy wobble back.
	 * Over Duration (a quick gun's shots each get a quicker one).
	 */
	FVector GetSpitScale(float Since, float Duration)
	{
		if (Since < 0.f || Since >= Duration || Duration <= 0.f)
		{
			return FVector::OneVector;
		}
		const float T = Since / Duration;
		if (T < 0.18f)
		{
			const float Gulp = FMath::Sin(T / 0.18f * HALF_PI);
			return FVector(1.f - 0.12f * Gulp, 1.f + 0.28f * Gulp, 1.f + 0.28f * Gulp);
		}
		if (T < 0.4f)
		{
			const float Spit = FMath::InterpEaseOut(0.f, 1.f, (T - 0.18f) / 0.22f, 2.f);
			const float Across = FMath::Lerp(1.28f, 0.82f, Spit);
			return FVector(FMath::Lerp(0.88f, 1.35f, Spit), Across, Across);
		}
		const float Back = (T - 0.4f) / 0.6f;
		const float Swing = FMath::Square(1.f - Back) * FMath::Cos(Back * 3.f * PI);
		return FVector(1.f + 0.35f * Swing, 1.f - 0.18f * Swing, 1.f - 0.18f * Swing);
	}

	/**
	 * A gun's model and how it is held (round 90, the grip fitting): the asset, its length in the hands at normal size, its
	 * muzzle tip on the 100-long mesh, and its grip: Grip, the point on the 100-long mesh that sits in the middle of the
	 * fist (on the pistol grip, about where the middle finger wraps it), and GripTurn, the gun turned in the fist from the
	 * standard hold (the fist's frame: muzzle ahead, top up), e.g. pitched for a grip that rakes back more or less.
	 * The fist itself is the animal friend's (FRRSpeciesLook::GunHandTurn / GunHandOffset): its right hand bone turned into
	 * that hold, measured on each rig, and moved for its hand's size and shape. So a gun's numbers are the same on everyone.
	 * Fitting a new gun: render it side on (blender_guns_r90.py: red ball = its origin, the grip), set Grip on the middle of
	 * the grip and GripTurn to 0, then check it in the game: console "RRGunGrip <gun> x y z pitch yaw roll" moves it live,
	 * "RRGunGrip" alone logs every gun's numbers to copy here; "rr.Gun.Debug 1" marks the palm and the grip.
	 */
	struct FRRGunLook
	{
		const TCHAR* Path;
		float Length;
		FVector Tip;
		FVector Grip;
		FRotator GripTurn;
	};
	// By ERRHitEffect, pistol to cannon. Round 90: new models (Midjourney sheet, Tripo, char-pipeline/scripts/blender_guns_r90.py:
	// grip at the origin, muzzle +X, 100 long): chubby pastel toys with eyes and a round mouth for a muzzle, about as big as
	// an animal friend's fist (the user's ask). Not const: RRGunGrip tunes them live.
	FRRGunLook GunLooks[] =
	{
		{ TEXT("/Game/Props/GunsR90/SM_Pistol.SM_Pistol"), 34.f, FVector(83.f, 0.f, 48.f), FVector(0.f, 0.f, 8.f), FRotator::ZeroRotator },
		{ TEXT("/Game/Props/GunsR90/SM_Smg.SM_Smg"), 36.f, FVector(84.f, -1.f, 47.f), FVector(0.f, 0.f, 8.f), FRotator::ZeroRotator },
		{ TEXT("/Game/Props/GunsR90/SM_Minigun.SM_Minigun"), 40.f, FVector(80.f, -3.f, 40.f), FVector(0.f, 0.f, 8.f), FRotator::ZeroRotator },
		{ TEXT("/Game/Props/GunsR90/SM_Rifle.SM_Rifle"), 42.f, FVector(82.f, 0.f, 38.f), FVector(0.f, 0.f, 8.f), FRotator::ZeroRotator },
		{ TEXT("/Game/Props/GunsR90/SM_Blaster.SM_Blaster"), 38.f, FVector(75.f, 0.f, 39.f), FVector(0.f, 0.f, 8.f), FRotator::ZeroRotator },
		{ TEXT("/Game/Props/GunsR90/SM_Cannon.SM_Cannon"), 44.f, FVector(74.f, -1.f, 33.f), FVector(0.f, 0.f, 8.f), FRotator::ZeroRotator },
	};
	const TCHAR* GunNames[] = { TEXT("PISTOL"), TEXT("SMG"), TEXT("MINIGUN"), TEXT("RIFLE"), TEXT("BLASTER"), TEXT("CANNON") };

	/** The species' hand fixes as set live with RRGunHand (over FRRSpeciesLook's), until the game ends. */
	TMap<int32, TPair<FVector, FRotator>> LiveHandFixes;

	TAutoConsoleVariable<int32> CVarGunDebug(TEXT("rr.Gun.Debug"), 0,
		TEXT("1: mark each held gun's palm (red), grip (yellow) and muzzle (cyan), and log the right hand's frame in the brawler's every second."));

	bool ParseNumbers(const TArray<FString>& Args, int32 First, FVector& Offset, FRotator& Turn)
	{
		if (Args.Num() < First + 6)
		{
			return false;
		}
		Offset = FVector(FCString::Atof(*Args[First]), FCString::Atof(*Args[First + 1]), FCString::Atof(*Args[First + 2]));
		Turn = FRotator(FCString::Atof(*Args[First + 3]), FCString::Atof(*Args[First + 4]), FCString::Atof(*Args[First + 5]));
		return true;
	}

	FAutoConsoleCommand GunGripCommand(TEXT("RRGunGrip"),
		TEXT("RRGunGrip <PISTOL|SMG|MINIGUN|RIFLE|BLASTER|CANNON> <grip x y z on the 100-long gun> <pitch yaw roll>: moves that gun in the fist. Alone: logs every gun's grip."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			for (int32 Gun = 0; Gun < UE_ARRAY_COUNT(GunLooks); ++Gun)
			{
				FVector Grip = FVector::ZeroVector;
				FRotator Turn = FRotator::ZeroRotator;
				if (Args.Num() > 0 && Args[0].Equals(GunNames[Gun], ESearchCase::IgnoreCase) && ParseNumbers(Args, 1, Grip, Turn))
				{
					GunLooks[Gun].Grip = Grip;
					GunLooks[Gun].GripTurn = Turn;
				}
				UE_LOG(LogTemp, Log, TEXT("RRGunGrip %s grip (%.1f, %.1f, %.1f) turn (%.1f, %.1f, %.1f)"), GunNames[Gun], GunLooks[Gun].Grip.X,
					GunLooks[Gun].Grip.Y, GunLooks[Gun].Grip.Z, GunLooks[Gun].GripTurn.Pitch, GunLooks[Gun].GripTurn.Yaw, GunLooks[Gun].GripTurn.Roll);
			}
		}));

	FAutoConsoleCommand GunHandCommand(TEXT("RRGunHand"),
		TEXT("RRGunHand <species> <offset forward right up, cm> <pitch yaw roll from the hand bone>: the species' fist for every gun, live."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			FVector Offset = FVector::ZeroVector;
			FRotator Turn = FRotator::ZeroRotator;
			const int32 Species = Args.Num() > 0 ? RRSpecies::FromName(Args[0]) : INDEX_NONE;
			if (Species != INDEX_NONE && ParseNumbers(Args, 1, Offset, Turn))
			{
				LiveHandFixes.Add(Species, TPair<FVector, FRotator>(Offset, Turn));
				UE_LOG(LogTemp, Log, TEXT("RRGunHand %s offset %s turn %s"), *Args[0], *Offset.ToString(), *Turn.ToString());
			}
		}));
}

URRRandomGunComponent::URRRandomGunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the animation has posed the hands this frame
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rifle(TEXT("/Game/Props/Rifle/SM_Rifle.SM_Rifle"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FoamFinger(TEXT("/Game/Props/FoamFinger/SM_FoamFinger.SM_FoamFinger"));
	RifleMesh = Rifle.Succeeded() ? Rifle.Object : nullptr;
	FoamFingerMesh = FoamFinger.Succeeded() ? FoamFinger.Object : nullptr;
	for (const FRRGunLook& Look : GunLooks)
	{
		UStaticMesh* Mesh = nullptr;
		if (Look.Path)
		{
			ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Finder(Look.Path);
			Mesh = Finder.Get();
		}
		GunMeshes.Add(Mesh);
	}
	SetStaticMesh(RifleMesh);
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	// Placed in the world every frame from the hands
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void URRRandomGunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwner());
	const USkeletalMeshComponent* Body = Brawler ? Brawler->GetMesh() : nullptr;
	const bool bHands = Body && Body->GetBoneIndex(RightHandBone) != INDEX_NONE && Body->GetBoneIndex(RightPalmBone) != INDEX_NONE
		&& Body->GetBoneIndex(LeftHandBone) != INDEX_NONE;
	// Put away while knocked out and while dancing after a win; a defense villain has none
	const bool bShow = bHands && Brawler->IsAlive() && !Brawler->IsVillain() && !Brawler->IsCelebrating() && !Brawler->IsDespairing() && !Brawler->IsUsingUltimate()
		&& !Brawler->IsDriving();
	if (IsVisible() != bShow)
	{
		SetVisibility(bShow);
	}
	if (!bShow)
	{
		return;
	}

	const FVector RightHand = Body->GetBoneLocation(RightHandBone);
	const FVector Palm = (RightHand + Body->GetBoneLocation(RightPalmBone)) * 0.5f;
	const FVector Forward = (Body->GetBoneLocation(LeftHandBone) - RightHand).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return;
	}
	// The gun's own model (the rifle when its model is missing), or the foam finger
	const bool bFinger = Brawler->GetWeapon().bFoamFinger && FoamFingerMesh;
	const int32 Kind = static_cast<int32>(Brawler->GetWeapon().GetHitEffect());
	const bool bOwnLook = !bFinger && GunMeshes.IsValidIndex(Kind) && GunMeshes[Kind];
	UStaticMesh* WantedMesh = bFinger ? FoamFingerMesh.Get() : (bOwnLook ? GunMeshes[Kind].Get() : RifleMesh.Get());
	if (GetStaticMesh() != WantedMesh)
	{
		SetStaticMesh(WantedMesh);
		// The random events gave this component an instance of the rifle's material; the new mesh brings its own
		EmptyOverrideMaterials();
	}
	const float Length = bFinger ? FoamFingerLength : (bOwnLook ? GunLooks[Kind].Length : GunLength);
	const FVector Tip = bFinger ? FoamFingerTip : (bOwnLook ? GunLooks[Kind].Tip : MuzzleTip);

	// A giant's gun grows with it, and so does the push out of the body
	const float Size = Brawler->GetActorScale3D().Z;
	const FTransform Actor = Brawler->GetActorTransform();

	// The foam finger is worn on the right hand, pointing ahead and a little up, with a quick jab on every poke
	if (bFinger)
	{
		const float SincePoke = Brawler->GetTimeSinceShot();
		const float Jab = SincePoke >= 0.f && SincePoke < FoamFingerPokeTime ? FMath::Sin(PI * SincePoke / FMath::Max(FoamFingerPokeTime, KINDA_SMALL_NUMBER)) : 0.f;
		const FVector Pointing = Actor.GetRotation().RotateVector(FRotator(FMath::Lerp(FoamFingerLift, FoamFingerPokeLift, Jab), 0.f, 0.f).Vector());
		const FVector Location = Palm + (Actor.TransformVectorNoScale(FoamFingerHoldOffset) + Pointing * FoamFingerPokeDistance * Jab) * Size;
		SetWorldLocationAndRotation(Location, FRotationMatrix::MakeFromXZ(Pointing, FVector::UpVector).ToQuat());
		SetWorldScale3D(FVector(Length / 100.f * Size));
		AimBlend = 0.f;
		return;
	}

	// In the hands: muzzle toward the left hand, top toward the sky as far as that allows
	// (a gun pointed straight up keeps the last roll). Pushed out of the round body, except in the rifle stance,
	// where the hands hold it out front on their own and the gun should sit right in them.
	// The aiming pose (URRRandomBodyAnimInstance) points the hands where the brawler faces, so there the gun sits right in them
	const bool bAimSet = Brawler->GetAimAnimations().IsComplete();
	const float AimPose = Brawler->GetAimPoseWeight();
	const float Stance = FMath::Max(Brawler->GetFireStanceAlpha(), AimPose);
	FQuat HandRotation;
	FVector HandLocation;
	if (bOwnLook)
	{
		// Round 90: the fist-sized toy guns sit in the right fist like in a socket, following the hand bone's own turn
		// (idle, walk, run and aim alike): the species' fist (its hand bone turned and moved by GunHandTurn / GunHandOffset),
		// the gun turned in it by its GripTurn, and its Grip point on the middle of the fist
		const int32 Species = Brawler->GetSpecies();
		const TArray<FRRSpeciesLook>& Looks = FRRSpeciesLook::GetAll();
		FVector FixOffset = Looks.IsValidIndex(Species) ? Looks[Species].GunHandOffset : FVector::ZeroVector;
		FRotator FixTurn = Looks.IsValidIndex(Species) ? Looks[Species].GunHandTurn : FRotator::ZeroRotator;
		if (const TPair<FVector, FRotator>* Live = LiveHandFixes.Find(Species))
		{
			FixOffset = Live->Key;
			FixTurn = Live->Value;
		}
		const FQuat Fist = Body->GetBoneQuaternion(RightHandBone) * FixTurn.Quaternion();
		HandRotation = Fist * GunLooks[Kind].GripTurn.Quaternion();
		HandLocation = Palm + Fist.RotateVector(FixOffset) * Size - HandRotation.RotateVector(GunLooks[Kind].Grip) * Length / 100.f * Size;
	}
	else
	{
		const FVector Holding = Forward;
		HandRotation = FMath::Abs(Holding.Z) < 0.98f ? FRotationMatrix::MakeFromXZ(Holding, FVector::UpVector).ToQuat() : GetComponentQuat();
		// Running (Shift): the muzzle tipped down (the arms drop too, URRRandomBodyAnimInstance), about the gun's own side
		// axis so it stays in the hands; positive pitch about +Y tips the muzzle (+X) down
		if (const float Sprint = Brawler->GetSprintPoseShare(); Sprint > 0.f)
		{
			HandRotation = HandRotation * FQuat(FVector::RightVector, FMath::DegreesToRadians(SprintMuzzleDown * Sprint));
		}
		HandLocation = Palm + Actor.TransformVectorNoScale(HoldOffset) * Size * (1.f - Stance);
	}
	if (CVarGunDebug.GetValueOnGameThread() > 0)
	{
		DrawDebugPoint(GetWorld(), Palm, 10.f, FColor::Red);
		DrawDebugPoint(GetWorld(), Body->GetBoneLocation(LeftHandBone), 10.f, FColor::Blue);
		DrawDebugPoint(GetWorld(), HandLocation + HandRotation.RotateVector(Tip) * Length / 100.f * Size, 10.f, FColor::Cyan);
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - DebugLogTime >= 1.0)
		{
			DebugLogTime = Now;
			// The right hand bone's axes and the way to the left hand, in the brawler's frame (forward, right, up)
			const FQuat Hand = Actor.GetRotation().Inverse() * Body->GetBoneQuaternion(RightHandBone);
			UE_LOG(LogTemp, Log, TEXT("RRGunDebug %s speed %.0f aim %.2f hand X %s Y %s Z %s to left hand %s palm %s"), *Brawler->GetName(),
				Brawler->GetVelocity().Size2D(), AimPose, *Hand.GetAxisX().ToString(), *Hand.GetAxisY().ToString(), *Hand.GetAxisZ().ToString(),
				*Actor.GetRotation().UnrotateVector(Forward).ToString(), *Actor.InverseTransformPositionNoScale(Palm).ToString());
		}
	}

	// Without the aiming set, shooting on the move (no rifle stance, the walk keeps the hands low): aimed along the way
	// shots fly (the brawler faces its target to shoot) with the muzzle's tip on the spot shots appear
	// (ARRRandomCharacter::ShootAt: capsule center + MuzzleOffset turned with the brawler, not scaled)
	const FQuat AimRotation = Actor.GetRotation();
	const FVector AimLocation = Actor.GetLocation() + AimRotation.RotateVector(Brawler->GetMuzzleOffset())
		- AimRotation.RotateVector(Tip * Length / 100.f * Size);

	AimBlend = FMath::FInterpConstantTo(AimBlend, !bAimSet && Brawler->IsAiming() && Stance < 0.5f ? 1.f : 0.f, DeltaTime, AimBlendSpeed);
	const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, AimBlend, 2.f);
	SetWorldLocationAndRotation(FMath::Lerp(HandLocation, AimLocation, Alpha), FQuat::Slerp(HandRotation, AimRotation, Alpha));
	// Its size, with the spit of the latest shot on top (a quick gun's each shorter: about its time between shots)
	const FVector BaseScale(Length / 100.f * Size);
	const float SpitTime = FMath::Clamp(Brawler->GetWeapon().FireIntervalMultiplier * SpitShare, SpitMinTime, SpitMaxTime);
	SetWorldScale3D(BaseScale * GetSpitScale(Brawler->GetTimeSinceShot(), SpitTime));

	// Settled in the aiming pose: remember where this gun's muzzle is, from the brawler, for the shots
	// (ARRRandomCharacter::ShootAt), so they leave the barrel even when the first one comes before the pose does
	const int32 Look = bOwnLook ? Kind : INDEX_NONE;
	if (Look != MuzzleLook)
	{
		MuzzleLook = Look;
		bMuzzleKnown = false;
	}
	if (bAimSet && AimPose >= SettledAimPose)
	{
		// (where the muzzle is at rest: the spit doesn't move where shots leave)
		const FVector Muzzle = FTransform(GetComponentQuat(), GetComponentLocation(), BaseScale).TransformPosition(Tip);
		const FVector Offset = Actor.GetRotation().UnrotateVector(Muzzle - Actor.GetLocation()) / Size;
		AimMuzzle = bMuzzleKnown ? FMath::Lerp(AimMuzzle, Offset, FMath::Min(1.f, DeltaTime * MuzzleFollowSpeed)) : Offset;
		bMuzzleKnown = true;
	}
}

FVector URRRandomGunComponent::GetAimMuzzleOffset() const
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwner());
	if (!Brawler || !IsVisible() || !bMuzzleKnown || Brawler->GetWeapon().bFoamFinger)
	{
		return Brawler ? Brawler->GetMuzzleOffset() : FVector::ZeroVector;
	}
	return AimMuzzle * Brawler->GetActorScale3D().Z;
}
