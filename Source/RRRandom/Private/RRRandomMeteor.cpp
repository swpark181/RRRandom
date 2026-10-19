#include "RRRandomMeteor.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomFogOfWar.h"
#include "RRRandomGameState.h"
#include "RRRandomPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The meteor model is 100 tall, pivot at the bottom middle (char-pipeline/scripts/ue_import_meteor_rush.py); its glass
	// globe sits in the crust's ring with its middle this high up and this big round, as shares of the height
	constexpr float Height = 330.f;
	constexpr float GlobeMiddle = 0.47f;
	constexpr float GlobeRadius = 0.37f;
	// Turns the model's face (the open side of its globe) to the body's +X, which looks at the camera
	constexpr float CrustYaw = -90.f;
	constexpr int32 DiceCount = 9;
	constexpr float DieSize = 0.12f;
	// The fall: this long, from off to one side of the spot as this machine's camera sees it (this share of the way to the
	// edge of the view there, within these bounds) and high up (this many times as far), so its shooting-star tail
	// streaks in across the view rather than at the camera, from the narrow top-down camera too; the tail this long
	// behind it
	constexpr float FallTime = 1.3f;
	constexpr float FallSideShare = 0.75f;
	constexpr float FallSideMin = 700.f;
	constexpr float FallSideMax = 2000.f;
	constexpr float FallRise = 1.4f;
	constexpr float TailLength = 2600.f;
	// After landing, like a cartoon spinning top: squashed flat and bouncing back, spinning up (wobbling, dice whirling),
	// flinging a piece of ore out sideways every ThrowInterval from MixTime on, winding down, dizzy stars over it, and
	// after lingering a swell and a POOF
	constexpr float MixTime = 0.55f;
	constexpr float ThrowInterval = 0.38f;
	constexpr float WindDown = 0.6f;
	constexpr float DizzyTime = 2.4f;
	constexpr float LingerTime = 3.f;
	constexpr float ShrinkTime = 0.35f;
	// Spinning on the spot after landing, degrees per second at the fastest
	constexpr float LandSpin = 900.f;
	// How long a comic word stays up
	constexpr float ComicLife = 0.9f;

	int32 GetPieceCount(bool bGolden)
	{
		return bGolden ? 1 : 6;
	}
	/** Seconds after landing of a piece's throw. */
	float GetThrowTime(int32 Index)
	{
		return MixTime + ThrowInterval * Index;
	}
	/** Seconds after landing when the last piece has gone (and the spin starts winding down). */
	float GetThrowEnd(bool bGolden)
	{
		return GetThrowTime(GetPieceCount(bGolden) - 1) + 0.2f;
	}
	float GetSpinEnd(bool bGolden)
	{
		return GetThrowEnd(bGolden) + WindDown;
	}
	/** Seconds after landing when it pops away. */
	float GetVanishTime(bool bGolden)
	{
		return GetSpinEnd(bGolden) + LingerTime;
	}

	const TCHAR* const ThrowWords[] = { TEXT("POP!"), TEXT("PING!"), TEXT("BOING!"), TEXT("POP!"), TEXT("PLINK!"), TEXT("TA-DA!") };
	// The knock aside on landing
	constexpr float KnockSpeed = 850.f;
	constexpr float KnockLift = 520.f;

	const FLinearColor RingColor = FLinearColor::FromSRGBColor(FColor(242, 127, 166));
	const FLinearColor GoldColor = FLinearColor::FromSRGBColor(FColor(255, 200, 70));
	const FLinearColor DiceColors[] =
	{
		FLinearColor::FromSRGBColor(FColor(247, 150, 190)),
		FLinearColor::FromSRGBColor(FColor(130, 215, 180)),
		FLinearColor::FromSRGBColor(FColor(185, 160, 240)),
		FLinearColor::FromSRGBColor(FColor(250, 215, 120)),
		FLinearColor::FromSRGBColor(FColor(120, 185, 245)),
	};

	float ServerNow(const UWorld* World)
	{
		const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
		return State ? State->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.f);
	}

	/** Speeding up, but already on its way at the start (so its tail is long from the first moment it shows). */
	float EaseIn(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * (0.35f + 0.65f * T);
	}
}

ARRRandomMeteor::ARRRandomMeteor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CrustMesh(TEXT("/Game/MeteorRush/SM_MRMeteor.SM_MRMeteor"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DieMesh(TEXT("/Game/Props/Dice/SM_Dice.SM_Dice"));

	auto MakeMesh = [this](const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetStaticMesh(Mesh);
		return Part;
	};
	Ring = MakeMesh(TEXT("Ring"), Root, Plane.Object);
	Closing = MakeMesh(TEXT("Closing"), Root, Plane.Object);
	Ring->SetCastShadow(false);
	Closing->SetCastShadow(false);

	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetUsingAbsoluteLocation(true);
	Body->SetUsingAbsoluteRotation(true);
	Body->SetUsingAbsoluteScale(true);
	Crust = MakeMesh(TEXT("Crust"), Body, CrustMesh.Object);
	Crust->SetRelativeScale3D(FVector(Height / 100.f));
	Crust->SetRelativeRotation(FRotator(0.f, CrustYaw, 0.f));
	Globe = MakeMesh(TEXT("Globe"), Body, Sphere.Object);
	Globe->SetRelativeLocation(FVector(0.f, 0.f, Height * GlobeMiddle));
	// The engine sphere is 100 across
	Globe->SetRelativeScale3D(FVector(Height * GlobeRadius / 50.f));
	Globe->SetCastShadow(false);
	for (int32 Index = 0; Index < DiceCount; ++Index)
	{
		UStaticMeshComponent* Die = MakeMesh(*FString::Printf(TEXT("Die%d"), Index), Body, DieMesh.Object);
		Die->SetCastShadow(false);
		Dice.Add(Die);
	}
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Body);
	Light->SetRelativeLocation(FVector(0.f, 0.f, Height * GlobeMiddle));
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(900.f);
}

void ARRRandomMeteor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomMeteor, ImpactTime);
	DOREPLIFETIME(ARRRandomMeteor, bGolden);
}

void ARRRandomMeteor::Setup(float InImpactTime, bool bInGolden)
{
	ImpactTime = InImpactTime;
	bGolden = bInGolden;
	// Thrown from the back: three shards, two lumps, and the big crystal last (the golden meteor: its crystal alone)
	Contents = bGolden ? TArray<ERROreKind>{ ERROreKind::Golden }
		: TArray<ERROreKind>{ ERROreKind::Big, ERROreKind::Ore, ERROreKind::Shard, ERROreKind::Ore, ERROreKind::Shard, ERROreKind::Shard };
}

float ARRRandomMeteor::GetTimeToImpact() const
{
	return ImpactTime - ServerNow(GetWorld());
}

bool ARRRandomMeteor::IsActive() const
{
	return GetTimeToImpact() > 0.f || Contents.Num() > 0;
}

bool ARRRandomMeteor::IsBursting() const
{
	const float T = -GetTimeToImpact();
	return T >= 0.f && T <= GetSpinEnd(bGolden);
}

float ARRRandomMeteor::GetHeight()
{
	return Height;
}

void ARRRandomMeteor::BeginPlay()
{
	Super::BeginPlay();

	// The marks on the floor (char-pipeline/scripts/ue_make_meteor_ring.py): a band at the edge of a flat plane
	if (UMaterialInterface* Marker = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRRing.M_MRRing")))
	{
		const FLinearColor Color = bGolden ? GoldColor : RingColor;
		RingMaterial = UMaterialInstanceDynamic::Create(Marker, this);
		RingMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		RingMaterial->SetScalarParameterValue(TEXT("Glow"), 2.4f);
		RingMaterial->SetScalarParameterValue(TEXT("Thickness"), 0.07f);
		RingMaterial->SetScalarParameterValue(TEXT("Fill"), 0.14f);
		ClosingMaterial = UMaterialInstanceDynamic::Create(Marker, this);
		ClosingMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		ClosingMaterial->SetScalarParameterValue(TEXT("Glow"), 1.8f);
		ClosingMaterial->SetScalarParameterValue(TEXT("Fill"), 0.f);
		for (int32 Slot = 0; Slot < Ring->GetNumMaterials(); ++Slot)
		{
			Ring->SetMaterial(Slot, RingMaterial);
			Closing->SetMaterial(Slot, ClosingMaterial);
		}
	}
	if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRGlass.M_MRGlass")))
	{
		UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Glass, this);
		Paint->SetVectorParameterValue(TEXT("Tint"), bGolden ? FLinearColor(1.f, 0.85f, 0.45f) : FLinearColor(0.85f, 0.8f, 1.f));
		Paint->SetScalarParameterValue(TEXT("Opacity"), 0.12f);
		Paint->SetScalarParameterValue(TEXT("EdgeOpacity"), 0.6f);
		Paint->SetScalarParameterValue(TEXT("Glow"), 0.25f);
		Globe->SetMaterial(0, Paint);
	}
	if (bGolden)
	{
		if (UMaterialInterface* CrustMaterial = Crust->GetMaterial(0))
		{
			UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(CrustMaterial, this);
			Paint->SetScalarParameterValue(TEXT("Glow"), 3.5f);
			Crust->SetMaterial(0, Paint);
		}
	}

	// The dice in the globe: rainbow (all gold for the golden meteor), white pips, each tumbling round its own axis
	UMaterialInterface* DieBody = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRDie.M_MRDie"));
	FRandomStream Stream(GetUniqueID());
	for (int32 Index = 0; Index < Dice.Num(); ++Index)
	{
		UStaticMeshComponent* Die = Dice[Index];
		if (!Die->GetStaticMesh())
		{
			continue;
		}
		const float Side = FMath::Max(1.f, Die->GetStaticMesh()->GetBounds().BoxExtent.GetMax() * 2.f);
		Die->SetRelativeScale3D(FVector(DieSize * Height / Side));
		const TArray<FName> Slots = Die->GetMaterialSlotNames();
		for (int32 Slot = 0; Slot < Die->GetNumMaterials(); ++Slot)
		{
			const bool bPip = Slots.IsValidIndex(Slot) && Slots[Slot].ToString().Contains(TEXT("Pip"));
			UMaterialInterface* Base = bPip ? Die->GetMaterial(Slot) : DieBody;
			if (!Base)
			{
				continue;
			}
			UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Base, this);
			Paint->SetVectorParameterValue(bPip ? TEXT("PipColor") : TEXT("Color"), bPip ? FLinearColor::White : (bGolden ? GoldColor : DiceColors[Index % UE_ARRAY_COUNT(DiceColors)]));
			Die->SetMaterial(Slot, Paint);
		}
		DiceAxes.Add(FVector(Stream.FRandRange(-1.f, 1.f), Stream.FRandRange(-1.f, 1.f), Stream.FRandRange(-0.4f, 0.4f)).GetSafeNormal());
		DicePhases.Add(Stream.FRandRange(0.f, 2.f * PI));
	}
	Light->SetLightColor(bGolden ? GoldColor : RingColor);
	UpdateLook(0.f);
}

void ARRRandomMeteor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The match is over: no more meteors (nobody gets knocked out of the dance line)
	if (HasAuthority() && ARRRandomGameState::IsMatchOver(this))
	{
		Destroy();
		return;
	}
	UpdateLook(DeltaSeconds);
	if (HasAuthority())
	{
		UpdateServer();
	}
}

void ARRRandomMeteor::UpdateLook(float DeltaSeconds)
{
	const float T = -GetTimeToImpact();
	const FVector Target = GetActorLocation();
	const float Radius = ImpactRadius;

	// The warning on the floor: a glowing ring round the spot, pulsing faster as it nears, and a thinner one closing in
	// on it from twice as far (the engine plane is 100 across)
	const bool bWarning = T < 0.f;
	Ring->SetVisibility(bWarning);
	Closing->SetVisibility(bWarning);
	if (bWarning)
	{
		const float Left = FMath::Clamp(-T / WarnTime, 0.f, 1.f);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * (8.f + 10.f * (1.f - Left)));
		Ring->SetWorldLocationAndRotation(Target + FVector(0.f, 0.f, 7.f), FRotator::ZeroRotator);
		Ring->SetWorldScale3D(FVector(Radius / 50.f, Radius / 50.f, 1.f));
		const float Closer = Radius * (1.f + Left);
		Closing->SetWorldLocationAndRotation(Target + FVector(0.f, 0.f, 6.f), FRotator::ZeroRotator);
		Closing->SetWorldScale3D(FVector(Closer / 50.f, Closer / 50.f, 1.f));
		if (RingMaterial)
		{
			RingMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.6f + 0.4f * Pulse);
			// The same band width on the floor however big the closing ring is
			ClosingMaterial->SetScalarParameterValue(TEXT("Thickness"), 18.f / Closer);
			ClosingMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.45f + 0.45f * (1.f - Left));
		}
	}

	// The meteor: out of sight until it falls, streaking down the last FallTime, then sitting where it landed
	const bool bFalling = T >= -FallTime && T < 0.f;
	// Where it falls from, picked on each machine for its own camera (kept once it starts falling), so it streaks in
	// across that player's view
	if (T < -FallTime || !bFallFromSet)
	{
		FVector Away = FVector::ZeroVector;
		// How far off: most of the way to the edge of the view at the spot (the top-down camera's view is narrow)
		float Side = FallSideMax;
		const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
		const ARRRandomPlayerController* Player = Cast<ARRRandomPlayerController>(Viewer);
		const ARRRandomFogOfWar* Fog = Player ? Player->GetFogOfWar() : nullptr;
		int32 ViewWidth = 0;
		int32 ViewHeight = 0;
		if (Viewer && Viewer->PlayerCameraManager)
		{
			const APlayerCameraManager* Camera = Viewer->PlayerCameraManager;
			const FVector Look = Target - Camera->GetCameraLocation();
			Away = Look.SizeSquared2D() > 1.f ? Look.GetSafeNormal2D() : Camera->GetCameraRotation().Vector().GetSafeNormal2D();
			Side = Look.Size() * FMath::Tan(FMath::DegreesToRadians(Camera->GetFOVAngle() * 0.5f)) * FallSideShare;
			Viewer->GetViewportSize(ViewWidth, ViewHeight);
		}
		if (Away.IsNearlyZero())
		{
			Away = FVector(-1.f, 0.f, 0.f);
		}
		Side = FMath::Clamp(Side, FallSideMin, FallSideMax);
		// Of a few ways in (mostly from the side, the first ones favoured), the one whose path stays most on this
		// player's screen and over ground they can see (the fog darkens whatever flies over the rest)
		auto Score = [&](const FVector& From)
		{
			float Total = 0.f;
			for (int32 Sample = 0; Sample < 4; ++Sample)
			{
				const FVector Point = Target + From * (1.f - Sample / 4.f);
				FVector2D Screen;
				if (ViewWidth > 0 && Viewer->ProjectWorldLocationToScreen(Point, Screen) && Screen.X > ViewWidth * 0.04f && Screen.X < ViewWidth * 0.96f
					&& Screen.Y > ViewHeight * 0.04f && Screen.Y < ViewHeight * 0.96f)
				{
					Total += 1.f;
				}
				Total += Fog ? Fog->GetSightAt(Point) : 0.f;
			}
			return Total;
		};
		const float Sign = GetUniqueID() % 2 == 0 ? 1.f : -1.f;
		const float Turns[] = { 70.f, -70.f, 110.f, -110.f, 40.f, -40.f, 140.f, -140.f };
		float Best = -1.f;
		for (const float Turn : Turns)
		{
			const FVector From = Away.RotateAngleAxis(Turn * Sign, FVector::UpVector) * Side + FVector(0.f, 0.f, Side * FallRise);
			// (a clear winner only)
			const float Points = Score(From);
			if (Points > Best + 0.25f)
			{
				Best = Points;
				FallFrom = From;
			}
		}
		bFallFromSet = true;
	}
	const FVector Start = Target + FallFrom;
	const int32 Pieces = GetPieceCount(bGolden);
	const float SpinEnd = GetSpinEnd(bGolden);
	const float Vanish = GetVanishTime(bGolden);
	FVector Where = Target;
	// Squash and stretch: XY and Z scale (1 is its own shape)
	float Wide = 1.f;
	float Tall = 1.f;
	if (bFalling)
	{
		Where = FMath::Lerp(Start, Target, EaseIn((T + FallTime) / FallTime));
		// Stretched along the fall
		Tall = 1.15f;
		Wide = 0.9f;
	}
	else if (T >= 0.f)
	{
		// Landing: squashed flat, then springing back with a wobble and a bounce
		const float Splat = 0.42f * FMath::Exp(-T * 6.f) * FMath::Cos(T * 21.f);
		Tall -= Splat;
		Wide += Splat * 0.6f;
		const float Bounce = T > 0.1f && T < 0.45f ? FMath::Sin((T - 0.1f) / 0.35f * PI) : 0.f;
		Where = Target - FVector(0.f, 0.f, 12.f) + FVector(0.f, 0.f, 90.f * Bounce);
		// Each fling: a quick stretch up, like a hiccup
		for (int32 Index = 0; Index < Pieces; ++Index)
		{
			const float Since = T - GetThrowTime(Index);
			if (Since >= 0.f && Since < 0.2f)
			{
				const float Hic = FMath::Sin(Since / 0.2f * PI) * 0.16f;
				Tall += Hic;
				Wide -= Hic * 0.5f;
			}
		}
		// Going: a swell, then shrinking to nothing in a POOF
		if (T > Vanish - 0.25f)
		{
			const float Swell = T < Vanish ? FMath::Sin((T - (Vanish - 0.25f)) / 0.25f * PI * 0.5f) * 0.18f : 0.18f;
			const float Gone = FMath::Clamp((T - Vanish) / ShrinkTime, 0.f, 1.f);
			const float Size = (1.f + Swell) * (1.f - Gone * Gone);
			Wide *= Size;
			Tall *= Size;
		}
	}
	const bool bShown = T >= -FallTime && Tall > 0.01f;
	Body->SetVisibility(bShown, true);
	Light->SetVisibility(bShown);
	if (!bShown)
	{
		return;
	}
	// Turned to this machine's camera (eased), tipped back toward it a little, so the globe's open side is always seen;
	// tumbling while it falls
	if (const APlayerController* Viewer = GetWorld()->GetFirstPlayerController(); Viewer && Viewer->PlayerCameraManager)
	{
		const FVector ToCamera = Viewer->PlayerCameraManager->GetCameraLocation() - Where;
		const FRotator Wanted(FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ToCamera.Z, ToCamera.Size2D())) * 0.45f, 0.f, 30.f), ToCamera.Rotation().Yaw, 0.f);
		Facing = DeltaSeconds > 0.f && bFacingSet ? FMath::RInterpTo(Facing, Wanted, DeltaSeconds, 3.f) : Wanted;
		bFacingSet = true;
	}
	// Tumbling down; after landing spinning up on the spot like a top (wobbling as it goes), winding down to face the
	// camera again
	const float ThrowEnd = GetThrowEnd(bGolden);
	float SpinRate = 0.f;
	if (bFalling)
	{
		SpinRate = 300.f;
	}
	else if (T >= 0.f && T < SpinEnd)
	{
		SpinRate = LandSpin * FMath::Clamp(T / 0.35f, 0.15f, 1.f) * (T > ThrowEnd ? 1.f - (T - ThrowEnd) / WindDown : 1.f);
	}
	if (SpinRate > 0.f)
	{
		Spin += DeltaSeconds * SpinRate;
	}
	else
	{
		Spin = FMath::FInterpTo(FMath::UnwindDegrees(Spin), 0.f, DeltaSeconds, 3.f);
	}
	const float Wobble = T >= 0.f && T < SpinEnd + 0.6f ? 9.f * FMath::Clamp(1.f - (T - SpinEnd) / 0.6f, 0.f, 1.f) : 0.f;
	const float WobbleAt = GetWorld()->GetTimeSeconds() * 9.f;
	Body->SetWorldLocationAndRotation(Where, Facing + FRotator(Wobble * FMath::Sin(WobbleAt), Spin, Wobble * FMath::Cos(WobbleAt)));
	Body->SetWorldScale3D(FVector(Wide, Wide, Tall));
	bSpinning = T >= 0.f && T < SpinEnd;
	DizzyAlpha = T >= SpinEnd - 0.2f && T < SpinEnd + DizzyTime ? FMath::Clamp(FMath::Min((T - SpinEnd + 0.2f) / 0.3f, (SpinEnd + DizzyTime - T) / 0.4f), 0.f, 1.f) : 0.f;
	TopLocation = Where + FVector(0.f, 0.f, Height * Tall);
	SpinSpeedShare = SpinRate / LandSpin;
	// Falling: a long shooting-star tail back along its path (from where it was last frame, on the same clock)
	if (bFalling && DeltaSeconds > 0.f)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			const FVector Lift(0.f, 0.f, Height * GlobeMiddle);
			const FVector Before = FMath::Lerp(Start, Target, EaseIn((T - DeltaSeconds + FallTime) / FallTime));
			Effects->StarTail(Before + Lift, Where + Lift, Height * 0.45f, TailLength, bGolden ? GoldColor : RingColor, DeltaSeconds);
		}
	}

	// Landing, once on each machine: a shockwave, dust, stars and dice
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	const FLinearColor Color = bGolden ? GoldColor : RingColor;
	const float Now = GetWorld()->GetTimeSeconds();
	auto AddWord = [&](const TCHAR* Text, const FVector& At, const FLinearColor& WordColor, float Size)
	{
		FRRComicPop& Pop = ComicPops.AddDefaulted_GetRef();
		Pop.Text = Text;
		Pop.Where = At;
		Pop.Start = Now;
		Pop.Color = WordColor;
		Pop.Size = Size;
		Pop.Tilt = FMath::FRandRange(-14.f, 14.f);
	};
	ComicPops.RemoveAll([Now](const FRRComicPop& Pop) { return Now - Pop.Start > ComicLife; });
	if (T >= 0.f && !bLandingShown)
	{
		bLandingShown = true;
		if (Effects)
		{
			Effects->TigerStrike(Target, Radius, Color);
			Effects->StarImpact(Target, -FallFrom, 2.6f, Color);
			// Dust puffs all round its foot (small ones: a big puff whites out a close camera)
			for (int32 Puff = 0; Puff < 5; ++Puff)
			{
				Effects->FoxPoof(Target + FVector(Height * 0.55f, 0.f, 20.f).RotateAngleAxis(Puff * 72.f + 20.f, FVector::UpVector), 0.7f);
			}
		}
		AddWord(bGolden ? TEXT("KA-BOOM!") : TEXT("BOOM!"), Target + FVector(0.f, 0.f, Height * 1.25f), Color, 1.4f);
	}
	// While it spins: sparks whirling round the globe; dust kicked up round its foot
	const FVector GlobeAt = Where + FVector(0.f, 0.f, Height * GlobeMiddle * Tall);
	if (Effects && bSpinning && DeltaSeconds > 0.f)
	{
		Effects->UltimateDieSpin(GlobeAt, Height * GlobeRadius * 1.2f, Target, FMath::Clamp(SpinSpeedShare, 0.f, 1.f) * 0.8f, DeltaSeconds);
	}
	// Each fling (on every machine, from the clock): a pop at its side and a comic word
	while (ShownThrows < Pieces && T >= GetThrowTime(ShownThrows))
	{
		const bool bLast = ShownThrows == Pieces - 1;
		const FVector Side = FVector(Height * 0.5f, 0.f, 0.f).RotateAngleAxis(FMath::FRandRange(0.f, 360.f), FVector::UpVector);
		if (Effects)
		{
			Effects->DiceLandBurst(GlobeAt + Side * 0.6f, 30.f);
		}
		AddWord(bGolden ? TEXT("TA-DA!") : ThrowWords[ShownThrows % UE_ARRAY_COUNT(ThrowWords)], GlobeAt + Side + FVector(0.f, 0.f, Height * 0.45f),
			bLast ? GoldColor : DiceColors[ShownThrows % UE_ARRAY_COUNT(DiceColors)], bLast ? 1.15f : 0.85f);
		++ShownThrows;
	}
	// Going: a puff of smoke and POOF
	if (T >= Vanish && !bBurstShown)
	{
		bBurstShown = true;
		if (Effects)
		{
			for (int32 Puff = 0; Puff < 3; ++Puff)
			{
				Effects->FoxPoof(Where + FVector(Height * 0.2f, 0.f, Height * (0.25f + 0.2f * Puff)).RotateAngleAxis(Puff * 120.f, FVector::UpVector), 0.6f);
			}
		}
		AddWord(TEXT("POOF!"), Where + FVector(0.f, 0.f, Height * 0.9f), FLinearColor::FromSRGBColor(FColor(189, 166, 240)), 1.f);
	}

	// The dice: drifting in the globe, whirling like a lottery draw while it spins
	const float Whirl = bSpinning ? 1.f : 0.f;
	const float Time = Now;
	const FVector Middle(0.f, 0.f, Height * GlobeMiddle);
	for (int32 Index = 0; Index < Dice.Num() && Index < DiceAxes.Num(); ++Index)
	{
		const float Speed = FMath::Lerp(0.8f, 5.5f, Whirl);
		const float Angle = DicePhases[Index] + Time * Speed * (Index % 2 == 0 ? 1.f : -1.f);
		const FVector Axis = DiceAxes[Index];
		const FVector Side = FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal();
		const FVector Orbit = Side.RotateAngleAxisRad(Angle, Axis) * Height * GlobeRadius * (0.35f + 0.25f * ((Index * 37) % 10) / 10.f);
		// At rest they settle toward the bottom of the globe
		const FVector Settle = FVector(0.f, 0.f, -Height * GlobeRadius * 0.35f * (1.f - Whirl));
		Dice[Index]->SetRelativeLocation(Middle + Orbit * FMath::Lerp(0.8f, 1.f, Whirl) + Settle);
		Dice[Index]->AddLocalRotation(FRotator(140.f, 90.f, 60.f) * DeltaSeconds * Speed);
	}
	Light->SetIntensity((bGolden ? 16000.f : 9000.f) * (T >= 0.f && T < 0.3f ? 3.f : 1.f) * (0.85f + 0.15f * FMath::Sin(Time * 6.f)));
}

void ARRRandomMeteor::UpdateServer()
{
	const float T = -GetTimeToImpact();
	if (T < 0.f)
	{
		return;
	}
	const FVector Target = GetActorLocation();
	if (!bHasLanded)
	{
		bHasLanded = true;
		NextThrowTime = MixTime;
		// Everyone close is knocked aside, away from the middle (no harm done)
		for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
		{
			ARRRandomCharacter* Brawler = *It;
			const FVector Offset = Brawler->GetActorLocation() - Target;
			if (!Brawler->IsAlive() || Brawler->IsDecoy() || Brawler->GetVehicle() || Offset.Size2D() > ImpactRadius || FMath::Abs(Offset.Z) > 400.f)
			{
				continue;
			}
			const FVector Away = Offset.Size2D() > 10.f ? Offset.GetSafeNormal2D() : FVector(1.f, 0.f, 0.f).RotateAngleAxis(FMath::FRandRange(0.f, 360.f), FVector::UpVector);
			Brawler->LaunchCharacter(Away * KnockSpeed * (1.f - 0.4f * Offset.Size2D() / ImpactRadius) + FVector(0.f, 0.f, KnockLift), true, true);
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: a %smeteor lands at %s."), bGolden ? TEXT("golden ") : TEXT(""), *Target.ToString());
	}
	while (Contents.Num() > 0 && T >= NextThrowTime)
	{
		ThrowNext();
		NextThrowTime += ThrowInterval;
	}
	// (kept until its POOF word is done)
	if (Contents.Num() == 0 && T > GetVanishTime(bGolden) + ComicLife + 0.1f)
	{
		Destroy();
	}
}

void ARRRandomMeteor::ThrowNext()
{
	const ERROreKind Kind = Contents.Pop();
	const bool bCrystal = RROre::IsCrystal(Kind);
	// Flung off the spinning globe's side like water off a sprinkler: round it a piece at a time in the way it spins
	// (each a little further round than the last, starting anywhere), out and along the spin, to land just outside the
	// warning ring (the crystal closer)
	if (Contents.Num() == 5 || bGolden)
	{
		ThrowAngle = FMath::FRandRange(0.f, 360.f);
	}
	ThrowAngle += 360.f / 6.f + FMath::FRandRange(-12.f, 12.f);
	const FVector Out = FVector(1.f, 0.f, 0.f).RotateAngleAxis(ThrowAngle, FVector::UpVector);
	const FVector Along = FVector(-Out.Y, Out.X, 0.f);
	const FVector Throw = Out * (bCrystal ? FMath::FRandRange(130.f, 200.f) : FMath::FRandRange(230.f, 380.f)) + Along * FMath::FRandRange(120.f, 200.f)
		+ FVector(0.f, 0.f, FMath::FRandRange(650.f, 820.f));
	const FTransform Spawn(GetActorLocation() + Out * Height * GlobeRadius + FVector(0.f, 0.f, Height * GlobeMiddle));
	if (ARRRandomOre* Piece = GetWorld()->SpawnActorDeferred<ARRRandomOre>(ARRRandomOre::StaticClass(), Spawn, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Piece->Setup(Kind, Throw);
		Piece->FinishSpawning(Spawn);
	}
}
