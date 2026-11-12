#include "RRRandomGunComponent.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInstanceDynamic.h"
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
		// Round 97 (Midjourney concepts in Tripo: A-3 orchid, B-1 peach, C-7 sky blue; Smart Mesh; blender_guns_r90.py; tip and
		// grip as it reported): the sniper, the shotgun, the light sword (its
		// hilt at the origin, the blade along +X). Until a model is in, FallbackLooks stands in.
		{ TEXT("/Game/Props/Weapons97/SM_Sniper.SM_Sniper"), 54.f, FVector(83.f, -1.f, 7.f), FVector(-7.f, 0.f, 7.f), FRotator::ZeroRotator },
		{ TEXT("/Game/Props/Weapons97/SM_Shotgun.SM_Shotgun"), 44.f, FVector(90.f, -11.f, 18.f), FVector(-1.f, 0.f, 14.f), FRotator::ZeroRotator },
		{ TEXT("/Game/Props/Weapons97/SM_LightSword.SM_LightSword"), 72.f, FVector(82.f, -1.f, 0.f), FVector(-9.f, 0.f, 5.f), FRotator::ZeroRotator },
	};
	const TCHAR* GunNames[] = { TEXT("PISTOL"), TEXT("SMG"), TEXT("MINIGUN"), TEXT("RIFLE"), TEXT("BLASTER"), TEXT("CANNON"), TEXT("SNIPER"), TEXT("SHOTGUN"), TEXT("SWORD") };
	/** A look whose model isn't in yet shows another's: the sniper the rifle, the shotgun the blaster, the sword the pistol. */
	const int32 FallbackLooks[] = { 0, 1, 2, 3, 4, 5, 3, 4, 0 };
	/**
	 * The light sword's strokes (round 97): each blow of a combo this long (the third a quarter longer, as its body motion),
	 * along its own arc in the body's frame, yaw (right +) and pitch (up +) at the start and the end of the sweep:
	 * 1st right to left across the chest, 2nd back left to right a little higher, 3rd rising from low right up to high left.
	 */
	constexpr float SwordStrokeTime = 0.42f;
	struct FSwordArc
	{
		float YawFrom;
		float YawTo;
		float PitchFrom;
		float PitchTo;
	};
	const FSwordArc SwordArcs[] =
	{
		{ 80.f, -80.f, -8.f, -8.f },
		{ -80.f, 80.f, 6.f, 6.f },
		{ 45.f, -35.f, -50.f, 60.f },
	};
	/** Round 97: how much longer the blade grows at the middle of a stroke, and how long a parry's flick lasts. */
	constexpr float SwordStretch = 0.45f;
	constexpr float ParrySwingTime = 0.28f;
	/** The blade's glowing core (round 97): along this much of the 100-long sword mesh, this thick (at normal size). */
	constexpr float BladeStart = 12.f;
	constexpr float BladeEnd = 80.f;
	constexpr float BladeGlowWidth = 9.f;

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
		// (a model not made yet is left out quietly: round 97's until they are imported)
		if (Look.Path && FPackageName::DoesPackageExist(FString(Look.Path).Left(FString(Look.Path).Find(TEXT(".")))))
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
		UpdateBladeGlow(false, 1.f);
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
	int32 Kind = Brawler->GetWeapon().GetLookIndex();
	if (!(GunMeshes.IsValidIndex(Kind) && GunMeshes[Kind]) && Kind < UE_ARRAY_COUNT(FallbackLooks))
	{
		Kind = FallbackLooks[Kind];
	}
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
	// The LIGHT SWORD (round 97). The body motions (URRRandomBodyAnimInstance) move the arm; the blade itself is kept on a
	// clean arc round the hand, so it reads as one sweeping stroke whatever the motion's wrist does:
	// - each blow of the combo its own arc (SwordArcs): a short wind-up, a fast sweep, a follow-through, then back to the hold;
	// - a parry on guard meets the shot where it reaches the blade (ParryPoint) and follows through toward the aim (where the
	//   holder faces: it sends the shot there);
	// - on guard between parries, the blade held level along the raised forearm, across the chest, pointing left.
	// A glowing pink streak follows the tip through every stroke; the blade stretches out through it (SwordStretch).
	const bool bSword = Brawler->GetWeapon().bSword;
	const bool bTwoHanded = bSword && Brawler->HasDedicatedSwordMotion() && !Brawler->IsClimbing();
	bool bSwinging = false;
	float BladeStretch = 1.f;
	// 0 .. 1 into a stroke (a parry's too: the guard gives way to it)
	float StrokeWeight = 0.f;
	bool bParryStroke = false;
	if (bSword && !bTwoHanded)
	{
		const float SinceSwing = Brawler->GetTimeSinceShot();
		const float SinceParry = Brawler->GetTimeSinceDeflect();
		bParryStroke = Brawler->IsDeflecting() || SinceParry < SinceSwing;
		const float Since = bParryStroke ? SinceParry : SinceSwing;
		const int32 Step = FMath::Clamp(Brawler->GetComboStep(), 0, 2);
		const float StrokeTime = bParryStroke ? ParrySwingTime : Brawler->GetSwordAttackDuration();
		if (Since >= 0.f && Since < StrokeTime)
		{
			bSwinging = Since < StrokeTime;
			const float Share = FMath::Clamp(Since / StrokeTime, 0.f, 1.f);
			BladeStretch = 1.f + SwordStretch * FMath::Sin(PI * Share);
			const FQuat Facing = Actor.GetRotation();
			// The arc in the body's frame: yaw (right +) and pitch (up +), from the start to the end of the sweep
			float YawFrom, YawTo, PitchFrom, PitchTo;
			if (bParryStroke)
			{
				// Toward the shot (where it meets the blade), in the body's frame; through it, on round to the aim (ahead)
				FVector ToShot = Facing.UnrotateVector(Brawler->GetParryPoint() - HandLocation);
				if (ToShot.IsNearlyZero())
				{
					ToShot = FVector::ForwardVector;
				}
				const FRotator Shot = ToShot.Rotation();
				const float Way = FMath::Abs(Shot.Yaw) < 20.f ? -1.f : -FMath::Sign(Shot.Yaw);
				YawFrom = Shot.Yaw - Way * 50.f;
				YawTo = FMath::Abs(Shot.Yaw) < 20.f ? Shot.Yaw + Way * 45.f : Way * 25.f;
				PitchFrom = FMath::Clamp(Shot.Pitch, -30.f, 40.f) + 15.f;
				PitchTo = 0.f;
			}
			else
			{
				const FSwordArc& Arc = SwordArcs[Step];
				YawFrom = Arc.YawFrom;
				YawTo = Arc.YawTo;
				PitchFrom = Arc.PitchFrom;
				PitchTo = Arc.PitchTo;
			}
			// A short wind-up held at the start, the sweep fast through the middle, the follow-through easing out
			const float Sweep = FMath::SmoothStep(0.08f, 0.62f, Share);
			const FRotator Along(FMath::Lerp(PitchFrom, PitchTo, Sweep), FMath::Lerp(YawFrom, YawTo, Sweep), 0.f);
			const FVector Blade = Facing.RotateVector(Along.Vector());
			const FQuat Stroke = FRotationMatrix::MakeFromXZ(Blade, Facing.GetUpVector()).ToQuat();
			StrokeWeight = FMath::Min(1.f, Share / 0.08f) * FMath::Min(1.f, (1.f - Share) / 0.25f);
			HandRotation = FQuat::Slerp(HandRotation, Stroke, StrokeWeight);
			const FVector BladeTip = HandLocation + HandRotation.RotateVector(Tip * FVector(BladeStretch, 1.f, 1.f)) * Length / 100.f * Size;
			if (bSwinging && bSwordTipKnown && DeltaTime > 0.f)
			{
				if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
				{
					Effects->StarTail(LastSwordTip, BladeTip, 18.f * Size, 160.f * Size, FLinearColor(1.f, 0.45f, 0.85f), DeltaTime);
				}
			}
			LastSwordTip = BladeTip;
		}
	}
	// On guard (deflecting), like a laser-sword duellist: the blade held level along the raised forearm across the chest,
	// pointing left and a little ahead and up, jolting up at each shot it turns away; giving way to each parry's stroke. The
	// sword stays in the hand (the guard motion raises the arm)
	GuardBlend = FMath::FInterpConstantTo(GuardBlend, bSword && Brawler->IsDeflecting() ? 1.f : 0.f, DeltaTime, 7.f);
	if (bSword && !bTwoHanded && GuardBlend > 0.f)
	{
		const FQuat Facing = Actor.GetRotation();
		const float Jolt = FMath::Max(0.f, 1.f - Brawler->GetTimeSinceDeflect() / 0.18f);
		const float Breath = 0.03f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.4f);
		const FVector Blade = (-Facing.GetRightVector() * 0.95f + Facing.GetForwardVector() * 0.22f + Facing.GetUpVector() * (0.14f + Breath + 0.35f * Jolt)).GetSafeNormal();
		const FQuat Guard = FRotationMatrix::MakeFromXZ(Blade, Facing.GetUpVector()).ToQuat();
		const float Weight = FMath::InterpEaseInOut(0.f, 1.f, GuardBlend, 2.f) * (1.f - (bParryStroke ? StrokeWeight : 0.f));
		HandRotation = FQuat::Slerp(HandRotation, Guard, Weight);
	}
	if (bTwoHanded)
	{
		// Both palms are solved onto one hilt by the body layer. Follow those palms;
		// an independent cosmetic sword arc would break the two-handed grip.
		const FVector LeftPalm = Body->GetBoneIndex(TEXT("LeftHandIndex1")) != INDEX_NONE
			? (Body->GetBoneLocation(LeftHandBone) + Body->GetBoneLocation(TEXT("LeftHandIndex1"))) * 0.5f
			: Body->GetBoneLocation(LeftHandBone);
		const FVector Blade = (Palm - LeftPalm).GetSafeNormal();
		if (!Blade.IsNearlyZero())
		{
			HandRotation = FRotationMatrix::MakeFromXZ(Blade, Actor.GetRotation().GetUpVector()).ToQuat();
			// Right fist near the emitter, left fist toward the pommel. Re-anchor AFTER
			// choosing the rotation so the handle cannot orbit away from either hand.
			HandLocation = Palm - HandRotation.RotateVector(FVector(-3.75f, 0.f, 5.f)) * Length / 100.f * Size;
		}
		const bool bParryTrail = Brawler->GetTimeSinceDeflect() < ARRRandomCharacter::DedicatedParryDuration
			&& (Brawler->IsDeflecting() || Brawler->GetTimeSinceDeflect() < Brawler->GetTimeSinceShot());
		bSwinging = (Brawler->GetTimeSinceShot() < Brawler->GetSwordAttackDuration() && !Brawler->IsDeflecting()) || bParryTrail;
		const FVector BladeTip = HandLocation + HandRotation.RotateVector(Tip) * Length / 100.f * Size;
		const float TipSpeed = DeltaTime > 0.f ? FVector::Dist(LastSwordTip, BladeTip) / DeltaTime : 0.f;
		if (bSwinging && bSwordTipKnown && TipSpeed > 180.f * Size)
		{
			if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
			{
				const float SpeedShare = FMath::Clamp(TipSpeed / (900.f * Size), 0.f, 1.f);
				Effects->StarTail(LastSwordTip, BladeTip, FMath::Lerp(2.f, 7.f, SpeedShare) * Size,
					FMath::Lerp(30.f, 85.f, SpeedShare) * Size, FLinearColor(1.f, 0.45f, 0.85f), DeltaTime);
			}
		}
		LastSwordTip = BladeTip;
	}
	bSwordTipKnown = bSwinging;

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
	// (a sword swings, it doesn't spit)
	SetWorldScale3D(bSword ? BaseScale * FVector(BladeStretch, 1.f, 1.f) : BaseScale * GetSpitScale(Brawler->GetTimeSinceShot(), SpitTime));
	UpdateBladeGlow(bSword, BladeStretch);

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

void URRRandomGunComponent::UpdateBladeGlow(bool bShow, float Stretch)
{
	// Round 97: the light sword's glowing core along its blade, like a laser sword's (a see-through glowing rod over the
	// toy blade, stretching with it; pulsing a little brighter while on guard)
	if (!bShow)
	{
		if (BladeGlow)
		{
			BladeGlow->SetVisibility(false);
		}
		return;
	}
	if (!BladeGlow && GetOwner())
	{
		BladeGlow = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("BladeGlow"));
		BladeGlow->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		BladeGlow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BladeGlow->SetCanEverAffectNavigation(false);
		BladeGlow->SetCastShadow(false);
		BladeGlow->SetUsingAbsoluteLocation(true);
		BladeGlow->SetUsingAbsoluteRotation(true);
		BladeGlow->SetUsingAbsoluteScale(true);
		BladeGlow->SetupAttachment(this);
		BladeGlow->RegisterComponent();
		if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRGlass.M_MRGlass")))
		{
			BladeGlowMaterial = UMaterialInstanceDynamic::Create(Glass, this);
			BladeGlowMaterial->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.45f, 0.9f, 1.f));
			BladeGlowMaterial->SetScalarParameterValue(TEXT("EdgeOpacity"), 0.2f);
			BladeGlowMaterial->SetScalarParameterValue(TEXT("Glow"), 6.f);
			BladeGlow->SetMaterial(0, BladeGlowMaterial);
		}
	}
	if (!BladeGlow)
	{
		return;
	}
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwner());
	const FTransform& Gun = GetComponentTransform();
	const FVector From = Gun.TransformPosition(FVector(BladeStart, 0.f, 0.f));
	const FVector To = Gun.TransformPosition(FVector(BladeEnd, 0.f, 0.f));
	const float Long = FVector::Dist(From, To);
	if (Long < 1.f)
	{
		BladeGlow->SetVisibility(false);
		return;
	}
	const float Size = Brawler ? Brawler->GetActorScale3D().Z : 1.f;
	const float Width = BladeGlowWidth * Size * (1.f + 0.25f * (Stretch - 1.f));
	// (the engine cylinder is 100 across and 100 tall, round its middle, along its Z)
	BladeGlow->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(To - From).ToQuat());
	BladeGlow->SetWorldScale3D(FVector(Width / 100.f, Width / 100.f, Long / 100.f));
	BladeGlow->SetOwnerNoSee(bOwnerNoSee);
	BladeGlow->SetVisibility(IsVisible());
	if (BladeGlowMaterial)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		const bool bGuard = Brawler && Brawler->IsDeflecting();
		BladeGlowMaterial->SetScalarParameterValue(TEXT("Opacity"), (bGuard ? 0.55f : 0.4f) + 0.08f * FMath::Sin(Now * (bGuard ? 18.f : 6.f)) + 0.15f * (Stretch - 1.f));
	}
}
