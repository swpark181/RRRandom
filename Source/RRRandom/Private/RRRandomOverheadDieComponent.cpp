#include "RRRandomOverheadDieComponent.h"
#include "RRRandomEffects.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Seconds the die takes to pop up to full size when thrown
	constexpr float PopTime = 0.15f;
	// Landing bounce: how much bigger the die gets for a moment, and for how long
	constexpr float LandPunch = 0.18f;
	constexpr float LandPunchTime = 0.18f;
	// How quickly the die follows the HUD's indicators when chips come and go
	constexpr float StackFollowSpeed = 12.f;

	// Each face's outward direction in SM_Dice's own space, and a direction along it that should point up on screen
	// (so the six's two columns stand upright). Opposite faces add up to 7. Checked on the imported mesh by counting
	// the dents on each side (blender_dice_pips.py carves them; Unreal's import flips Blender's Y).
	struct FFaceAxes
	{
		FVector Normal;
		FVector Up;
	};
	const FFaceAxes FaceAxes[6] = {
		{ FVector(0.f, 0.f, 1.f), FVector(0.f, 1.f, 0.f) },  // 1
		{ FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, 1.f) },  // 2
		{ FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f) },  // 3
		{ FVector(-1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f) }, // 4
		{ FVector(0.f, -1.f, 0.f), FVector(0.f, 0.f, 1.f) }, // 5
		{ FVector(0.f, 0.f, -1.f), FVector(0.f, 1.f, 0.f) }, // 6
	};

	// Overshoots a little before settling at 1, for a springy pop
	float EaseOutBack(float Alpha)
	{
		constexpr float Overshoot = 1.70158f;
		const float T = FMath::Clamp(Alpha, 0.f, 1.f) - 1.f;
		return 1.f + (Overshoot + 1.f) * T * T * T + Overshoot * T * T;
	}
}

URRRandomOverheadDieComponent::URRRandomOverheadDieComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiceMesh(TEXT("/Game/Props/Dice/SM_Dice.SM_Dice"));
	if (DiceMesh.Succeeded())
	{
		SetStaticMesh(DiceMesh.Object);
	}
	// The ultimate die's body (char-pipeline/scripts/ue_make_gold_die.py)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gold(TEXT("/Game/Props/Dice/M_DiceGold.M_DiceGold"));
	GoldMaterial = Gold.Succeeded() ? Gold.Object : nullptr;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	// A shadow far below the floating die would only confuse
	CastShadow = false;
	bReceivesDecals = false;
	// Placed in the world every frame; a giant's bigger body doesn't grow it
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	SetVisibleFlag(false);
}

void URRRandomOverheadDieComponent::SetStackTop(const FVector& WorldLocation)
{
	if (const AActor* Owner = GetOwner())
	{
		TargetStackOffset = WorldLocation - Owner->GetActorLocation();
		bHasStackTop = true;
	}
}

FQuat URRRandomOverheadDieComponent::FaceToward(int32 Face, const FQuat& Camera)
{
	const FFaceAxes& Axes = FaceAxes[FMath::Clamp(Face, 1, 6) - 1];
	const FQuat Local = FRotationMatrix::MakeFromXZ(Axes.Normal, Axes.Up).ToQuat();
	const FQuat Wanted = FRotationMatrix::MakeFromXZ(-Camera.GetForwardVector(), Camera.GetUpVector()).ToQuat();
	// Takes the face's normal to the camera and its up to the screen's up
	return Wanted * Local.Inverse();
}

void URRRandomOverheadDieComponent::SetGold(bool bInGold)
{
	if (bGold == bInGold || !GetStaticMesh())
	{
		return;
	}
	bGold = bInGold;
	for (int32 Slot = 0; Slot < GetNumMaterials(); ++Slot)
	{
		UMaterialInterface* Plain = GetStaticMesh()->GetMaterial(Slot);
		const bool bPips = GetMaterialSlotNames().IsValidIndex(Slot) && GetMaterialSlotNames()[Slot].ToString().Contains(TEXT("Pip"));
		if (!bGold)
		{
			SetMaterial(Slot, Plain);
		}
		else if (bPips)
		{
			if (!GoldPips)
			{
				GoldPips = UMaterialInstanceDynamic::Create(Plain, this);
				GoldPips->SetVectorParameterValue(TEXT("PipColor"), FLinearColor(0.45f, 0.02f, 0.01f));
			}
			SetMaterial(Slot, GoldPips);
		}
		else if (GoldMaterial)
		{
			if (!GoldBody)
			{
				GoldBody = UMaterialInstanceDynamic::Create(GoldMaterial, this);
			}
			SetMaterial(Slot, GoldBody);
		}
	}
}

void URRRandomOverheadDieComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwner());
	const URRRandomDiceBuffComponent* Dice = Brawler ? Brawler->GetDiceBuffs() : nullptr;
	// Turned toward this machine's own view, so each player reads the face head-on
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const float Age = Dice ? Dice->GetTimeSinceLastRoll() : UE_BIG_NUMBER;
	const float SpinTime = Dice ? Dice->GetSpinTime() : 0.f;
	if (!Dice || !Camera || !Brawler->IsAlive() || Age >= SpinTime + HoldTime + ExitTime)
	{
		if (IsVisible())
		{
			SetVisibility(false);
		}
		bSnapStack = true;
		return;
	}

	if (Dice->GetRollCount() != ShownRoll)
	{
		ShownRoll = Dice->GetRollCount();
		// Mostly sideways, so it tumbles over rather than spinning like a top
		SpinAxis = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.4f, 0.4f)).GetSafeNormal();
		if (SpinAxis.IsNearlyZero())
		{
			SpinAxis = FVector::ForwardVector;
		}
		bSnapStack = true;
	}

	const bool bUltimate = Dice->GetLastRoll().bUltimate;
	SetGold(bUltimate);
	const float Size = bUltimate ? UltimateScale : 1.f;
	const FQuat View = Camera->GetCameraRotation().Quaternion();
	const FVector ScreenUp = View.GetUpVector();

	// Sit on the HUD's indicators (or a fixed height over the head without a HUD), following them smoothly
	if (!bHasStackTop)
	{
		TargetStackOffset = FVector(0.f, 0.f, Brawler->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + DefaultLift);
	}
	StackOffset = bSnapStack ? TargetStackOffset : FMath::VInterpTo(StackOffset, TargetStackOffset, DeltaTime, StackFollowSpeed);
	bSnapStack = false;
	const float HalfSize = 50.f * DieScale * Size;
	FVector Location = Brawler->GetActorLocation() + StackOffset + ScreenUp * (HalfSize + StackGap);

	FQuat Rotation = FaceToward(Dice->GetLastRoll().Face, View);
	float Scale = 1.f;
	if (Age < SpinTime)
	{
		// Pops up, tumbles along an arc fast at first and slows to a stop on the rolled face
		const float Alpha = Age / SpinTime;
		Location += ScreenUp * HopHeight * (bUltimate ? UltimateHop : 1.f) * FMath::Sin(PI * Alpha);
		const float TurnsLeft = (bUltimate ? UltimateTurns : SpinTurns) * FMath::Pow(1.f - Alpha, 3.f);
		Rotation = FQuat(SpinAxis, TurnsLeft * 2.f * PI) * Rotation;
		Scale = EaseOutBack(Age / PopTime);
	}
	else
	{
		// A quick bounce on landing, then shrink away after HoldTime
		const float Landed = Age - SpinTime;
		Scale = 1.f + LandPunch * FMath::Sin(PI * FMath::Clamp(Landed / LandPunchTime, 0.f, 1.f));
		if (Landed > HoldTime)
		{
			Scale *= 1.f - FMath::InterpEaseIn(0.f, 1.f, FMath::Clamp((Landed - HoldTime) / ExitTime, 0.f, 1.f), 2.f);
		}
	}

	SetWorldLocationAndRotation(Location, Rotation);
	SetWorldScale3D(FVector(DieScale * Size * FMath::Max(Scale, KINDA_SMALL_NUMBER)));
	// The gold glows brighter as it spins down, and flares on landing
	if (bUltimate && GoldBody)
	{
		const float Pulse = 0.25f + 0.35f * FMath::Abs(FMath::Sin(Age * 9.f)) + (Age >= SpinTime ? 1.5f * FMath::Max(0.f, 1.f - (Age - SpinTime) / 0.4f) : 0.f);
		GoldBody->SetScalarParameterValue(TEXT("Glow"), Pulse);
	}

	// Glitter while it tumbles, a pop of light when it lands
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		const FVector Feet = Brawler->GetActorLocation() - FVector(0.f, 0.f, Brawler->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (Age < SpinTime)
		{
			bUltimate ? Effects->UltimateDieSpin(Location, HalfSize * Scale, Feet, Age / SpinTime, DeltaTime)
				: Effects->DiceSpinSparkle(Location, HalfSize * Scale, DeltaTime);
			bLandBurstShown = false;
		}
		else if (!bLandBurstShown)
		{
			bLandBurstShown = true;
			bUltimate ? Effects->UltimateDieBurst(Location, HalfSize, Feet) : Effects->DiceLandBurst(Location, HalfSize);
		}
	}
	if (!IsVisible())
	{
		SetVisibility(true);
	}
}
