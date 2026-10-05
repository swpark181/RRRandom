#include "RRRandomPlayerController.h"
#include "RRRandomCharacter.h"
#include "RRRandomEventComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

ARRRandomPlayerController::ARRRandomPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ARRRandomPlayerController::CreateDefaultInputAssets()
{
	if (!ClickAction)
	{
		ClickAction = NewObject<UInputAction>(this, TEXT("IA_Click"));
		ClickAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!MoveAction)
	{
		MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
		MoveAction->ValueType = EInputActionValueType::Axis2D;
	}
	if (!RollAction)
	{
		RollAction = NewObject<UInputAction>(this, TEXT("IA_Roll"));
		RollAction->ValueType = EInputActionValueType::Boolean;
	}
	if (MappingContext)
	{
		return;
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	MappingContext->MapKey(ClickAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(RollAction, EKeys::SpaceBar);

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

	EnhancedInput->BindAction(ClickAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnClickStarted);
	EnhancedInput->BindAction(ClickAction, ETriggerEvent::Triggered, this, &ARRRandomPlayerController::OnClickTriggered);
	EnhancedInput->BindAction(ClickAction, ETriggerEvent::Completed, this, &ARRRandomPlayerController::OnClickReleased);
	EnhancedInput->BindAction(ClickAction, ETriggerEvent::Canceled, this, &ARRRandomPlayerController::OnClickReleased);
	EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARRRandomPlayerController::OnMove);
	EnhancedInput->BindAction(RollAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnRoll);
}

void ARRRandomPlayerController::OnClickStarted()
{
	bWalkingToDestination = false;
	FollowTime = 0.f;
}

void ARRRandomPlayerController::OnClickTriggered()
{
	FollowTime += GetWorld()->GetDeltaSeconds();

	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, true, Hit))
	{
		CachedDestination = Hit.Location;
	}

	if (APawn* ControlledPawn = GetPawn())
	{
		const FVector Direction = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal2D();
		ControlledPawn->AddMovementInput(Direction, 1.f);
	}
}

void ARRRandomPlayerController::OnClickReleased()
{
	bWalkingToDestination = FollowTime <= ShortPressThreshold;
	FollowTime = 0.f;
}

void ARRRandomPlayerController::OnMove(const FInputActionValue& Value)
{
	bWalkingToDestination = false;

	// The camera never rotates, so screen up is world +X and screen right is world +Y
	const FVector2D Input = Value.Get<FVector2D>();
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddMovementInput(FVector::ForwardVector, Input.Y);
		ControlledPawn->AddMovementInput(FVector::RightVector, Input.X);
	}
}

void ARRRandomPlayerController::OnRoll()
{
	if (const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->GetRandomEvents()->RollRandomEvent();
	}
}

void ARRRandomPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	APawn* ControlledPawn = GetPawn();
	if (!bWalkingToDestination || !ControlledPawn)
	{
		return;
	}

	const FVector ToDestination = (CachedDestination - ControlledPawn->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (ToDestination.Size() <= AcceptanceRadius)
	{
		bWalkingToDestination = false;
		return;
	}
	ControlledPawn->AddMovementInput(ToDestination.GetSafeNormal(), 1.f);
}
