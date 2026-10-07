#include "RRRandomPlayerController.h"
#include "RRRandomCharacter.h"
#include "RRRandomSessionSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	constexpr float AimTraceDistance = 10000.f;
	// Surfaces flatter than this count as somewhere a brawler could stand
	constexpr float MinStandableNormalZ = 0.7f;
	// The cursor counts as on a brawler this far outside its capsule radius
	constexpr float CursorPickSlack = 30.f;
}

ARRRandomPlayerController::ARRRandomPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void ARRRandomPlayerController::CreateDefaultInputAssets()
{
	if (!FireAction)
	{
		FireAction = NewObject<UInputAction>(this, TEXT("IA_Fire"));
		FireAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!MoveAction)
	{
		MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
		MoveAction->ValueType = EInputActionValueType::Axis2D;
	}
	if (!JumpAction)
	{
		JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
		JumpAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!DiceAction)
	{
		DiceAction = NewObject<UInputAction>(this, TEXT("IA_RollDice"));
		DiceAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!ReloadAction)
	{
		ReloadAction = NewObject<UInputAction>(this, TEXT("IA_Reload"));
		ReloadAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!HostAction)
	{
		HostAction = NewObject<UInputAction>(this, TEXT("IA_Host"));
		HostAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!JoinAction)
	{
		JoinAction = NewObject<UInputAction>(this, TEXT("IA_Join"));
		JoinAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!TitleAction)
	{
		TitleAction = NewObject<UInputAction>(this, TEXT("IA_Title"));
		TitleAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!LeaveAction)
	{
		LeaveAction = NewObject<UInputAction>(this, TEXT("IA_Leave"));
		LeaveAction->ValueType = EInputActionValueType::Boolean;
	}
	if (MappingContext)
	{
		return;
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	MappingContext->MapKey(FireAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(DiceAction, EKeys::E);
	MappingContext->MapKey(ReloadAction, EKeys::R);
	MappingContext->MapKey(HostAction, EKeys::F1);
	MappingContext->MapKey(JoinAction, EKeys::F2);
	MappingContext->MapKey(LeaveAction, EKeys::F3);
	MappingContext->MapKey(TitleAction, EKeys::Escape);

	// Move value: X = right, Y = forward. Keys report on X, so W/S are swizzled onto Y.
	MappingContext->MapKey(MoveAction, EKeys::D);
	MappingContext->MapKey(MoveAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	MappingContext->MapKey(MoveAction, EKeys::W).Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext));
	FEnhancedActionKeyMapping& Back = MappingContext->MapKey(MoveAction, EKeys::S);
	Back.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext));
	Back.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
}

void ARRRandomPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CreateDefaultInputAssets();
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	// Online status for the HUD, and -RRHost / -RRJoin on the command line once the first arena is up
	if (IsLocalController())
	{
		if (URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>())
		{
			Sessions->OnArenaReady();
		}
	}
}

void ARRRandomPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	CreateDefaultInputAssets();
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("RRRandom needs EnhancedInputComponent as the default input component (Config/DefaultInput.ini)."));
		return;
	}

	// Triggered fires every frame while held; the character's fire cooldown sets the rate
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Triggered, this, &ARRRandomPlayerController::OnFire);
	EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARRRandomPlayerController::OnMove);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnJump);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ARRRandomPlayerController::OnStopJumping);
	EnhancedInput->BindAction(DiceAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnRollDice);
	EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnReload);
	EnhancedInput->BindAction(HostAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRHost);
	EnhancedInput->BindAction(JoinAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRJoin);
	EnhancedInput->BindAction(LeaveAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRLeave);
	EnhancedInput->BindAction(TitleAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRTitle);
}

void ARRRandomPlayerController::RRHost()
{
	if (URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>())
	{
		Sessions->HostGame();
	}
}

void ARRRandomPlayerController::RRJoin()
{
	if (URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>())
	{
		Sessions->JoinGame();
	}
}

void ARRRandomPlayerController::RRLeave()
{
	if (URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>())
	{
		Sessions->PlaySolo();
	}
}

void ARRRandomPlayerController::RRTitle()
{
	if (URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>())
	{
		Sessions->ReturnToTitle();
	}
}

void ARRRandomPlayerController::OnFire()
{
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	FVector CursorOrigin;
	FVector CursorDirection;
	if (RandomCharacter && DeprojectMousePositionToWorld(CursorOrigin, CursorDirection))
	{
		RandomCharacter->FireAt(GetAimTarget(CursorOrigin, CursorDirection));
	}
}

FVector ARRRandomPlayerController::GetAimTarget(const FVector& CursorOrigin, const FVector& CursorDirection) const
{
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter)
	{
		return CursorOrigin;
	}

	// Cursor on (or right next to) a brawler: aim straight at it, whatever level it stands on, the closest to the camera first
	const ARRRandomCharacter* Pointed = nullptr;
	float PointedDepth = UE_BIG_NUMBER;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == RandomCharacter || !It->IsAlive())
		{
			continue;
		}
		const FVector Center = It->GetActorLocation();
		const float Depth = FVector::DotProduct(Center - CursorOrigin, CursorDirection);
		if (Depth > 0.f && Depth < PointedDepth
			&& FMath::PointDistToLine(Center, CursorDirection, CursorOrigin) < It->GetCapsuleComponent()->GetScaledCapsuleRadius() + CursorPickSlack)
		{
			Pointed = *It;
			PointedDepth = Depth;
		}
	}
	if (Pointed)
	{
		return Pointed->GetActorLocation();
	}

	// Aim at where a brawler standing on the ground or cover under the cursor would be, so from on top of cover
	// shots angle down at the floor below; off flat surfaces, aim at the character's own height
	const FVector Location = RandomCharacter->GetActorLocation();
	FVector Target = FMath::RayPlaneIntersection(CursorOrigin, CursorDirection, FPlane(Location, FVector::UpVector));
	// Visibility, not an object type: the level's floor is movable (WorldDynamic) while cover is WorldStatic.
	// Brawlers are skipped so the ray reaches the surface they stand on.
	FHitResult Surface;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomAim), false);
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	if (GetWorld()->LineTraceSingleByChannel(Surface, CursorOrigin, CursorOrigin + CursorDirection * AimTraceDistance, ECC_Visibility, Params)
		&& Surface.ImpactNormal.Z > MinStandableNormalZ)
	{
		Target = Surface.ImpactPoint + FVector(0.f, 0.f, RandomCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
	return Target;
}

void ARRRandomPlayerController::OnMove(const FInputActionValue& Value)
{
	// The camera never rotates, so screen up is world +X and screen right is world +Y
	const FVector2D Input = Value.Get<FVector2D>();
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddMovementInput(FVector::ForwardVector, Input.Y);
		ControlledPawn->AddMovementInput(FVector::RightVector, Input.X);
	}
}

void ARRRandomPlayerController::OnJump()
{
	// The character climbs instead when facing cover it can reach the top of
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->Jump();
	}
}

void ARRRandomPlayerController::OnStopJumping()
{
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->StopJumping();
	}
}

void ARRRandomPlayerController::OnRollDice()
{
	// The character asks the server when this is a client
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->RollDice();
	}
}

void ARRRandomPlayerController::OnReload()
{
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->StartReload();
	}
}
