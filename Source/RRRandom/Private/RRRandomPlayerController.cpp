#include "RRRandomPlayerController.h"
#include "RRRandomFogOfWar.h"
#include "RRRandomCharacter.h"
#include "RRRandomGameMode.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "RRRandomResultsDirector.h"
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
	if (!MapAction)
	{
		MapAction = NewObject<UInputAction>(this, TEXT("IA_Map"));
		MapAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!ConfirmAction)
	{
		ConfirmAction = NewObject<UInputAction>(this, TEXT("IA_Confirm"));
		ConfirmAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!CheatWinAction)
	{
		CheatWinAction = NewObject<UInputAction>(this, TEXT("IA_CheatWin"));
		CheatWinAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!CheatGiantAction)
	{
		CheatGiantAction = NewObject<UInputAction>(this, TEXT("IA_CheatGiant"));
		CheatGiantAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!CheatFlyAction)
	{
		CheatFlyAction = NewObject<UInputAction>(this, TEXT("IA_CheatFly"));
		CheatFlyAction->ValueType = EInputActionValueType::Boolean;
	}
	if (CheatGunActions.IsEmpty())
	{
		for (int32 Type = 0; Type < FRRWeapon::GetTypeCount(); ++Type)
		{
			UInputAction* Action = NewObject<UInputAction>(this, *FString::Printf(TEXT("IA_CheatGun%d"), Type + 1));
			Action->ValueType = EInputActionValueType::Boolean;
			CheatGunActions.Add(Action);
		}
	}
	// Esc, Enter and clicks still work in the pause menu of a stopped solo game
	for (UInputAction* Action : { FireAction.Get(), TitleAction.Get(), ConfirmAction.Get() })
	{
		Action->bTriggerWhenPaused = true;
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
	MappingContext->MapKey(ConfirmAction, EKeys::Enter);
	MappingContext->MapKey(MapAction, EKeys::M);
#if !UE_BUILD_SHIPPING
	MappingContext->MapKey(CheatGiantAction, EKeys::F7);
	MappingContext->MapKey(CheatFlyAction, EKeys::F8);
	MappingContext->MapKey(CheatWinAction, EKeys::F6);
	const FKey GunKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 Type = 0; Type < FMath::Min(CheatGunActions.Num(), static_cast<int32>(UE_ARRAY_COUNT(GunKeys))); ++Type)
	{
		MappingContext->MapKey(CheatGunActions[Type], GunKeys[Type]);
	}
#endif

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
		// Fog of war for this screen only
		if (bFogOfWar)
		{
			FActorSpawnParameters FogParams;
			FogParams.Owner = this;
			FogOfWar = GetWorld()->SpawnActor<ARRRandomFogOfWar>(FogParams);
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
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnClick);
	EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnConfirm);
	EnhancedInput->BindAction(MapAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRMap);
	EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARRRandomPlayerController::OnMove);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnJump);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ARRRandomPlayerController::OnStopJumping);
	EnhancedInput->BindAction(DiceAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnRollDice);
	EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnReload);
	EnhancedInput->BindAction(HostAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRHost);
	EnhancedInput->BindAction(JoinAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRJoin);
	EnhancedInput->BindAction(LeaveAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRLeave);
	EnhancedInput->BindAction(TitleAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnEscape);
	EnhancedInput->BindAction(CheatWinAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRWin);
	EnhancedInput->BindAction(CheatGiantAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRGiant);
	EnhancedInput->BindAction(CheatFlyAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRFly);
	for (int32 Type = 0; Type < CheatGunActions.Num(); ++Type)
	{
		EnhancedInput->BindAction(CheatGunActions[Type], ETriggerEvent::Started, this, &ARRRandomPlayerController::OnCheatGun, Type);
	}
}

void ARRRandomPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bPauseMenuOpen && ARRRandomGameState::IsMatchOver(this))
	{
		ClosePauseMenu();
	}

	// The match just ended: blend this screen's view to the MVP and the fireworks
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (IsLocalController() && !ResultsDirector && State && State->IsMatchOver() && State->GetMVP())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		ResultsDirector = GetWorld()->SpawnActor<ARRRandomResultsDirector>(SpawnParams);
		if (ResultsDirector)
		{
			ResultsDirector->Setup(State->GetMVP());
			SetViewTargetWithBlend(ResultsDirector, ResultsBlendTime, VTBlend_EaseInOut, 2.f);
		}
	}
}

void ARRRandomPlayerController::RRNewGame()
{
	if (!ARRRandomGameState::IsMatchOver(this))
	{
		return;
	}
	// The game mode exists only on the host (or in a solo game)
	if (ARRRandomGameMode* GameMode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>())
	{
		GameMode->RestartMatch();
	}
	else
	{
		ClientMessage(TEXT("Only the host can start a new game."));
	}
}

void ARRRandomPlayerController::OnClick()
{
	const ARRRandomHUD* Hud = Cast<ARRRandomHUD>(GetHUD());
	float MouseX;
	float MouseY;
	if (!Hud || !GetMousePosition(MouseX, MouseY))
	{
		return;
	}
	// The map takes clicks on it (and any click while it fills the screen)
	if (!bPauseMenuOpen && !ARRRandomGameState::IsMatchOver(this))
	{
		if (ARRRandomHUD* MapHud = Cast<ARRRandomHUD>(GetHUD()))
		{
			MapHud->HandleMapClick(FVector2D(MouseX, MouseY));
		}
		return;
	}
	switch (Hud->GetResultsButtonAt(FVector2D(MouseX, MouseY)))
	{
	case ERRResultsButton::Resume:
		ClosePauseMenu();
		break;
	case ERRResultsButton::NewGame:
		RRNewGame();
		break;
	case ERRResultsButton::Title:
		RRTitle();
		break;
	default:
		break;
	}
}

void ARRRandomPlayerController::RRMap()
{
	if (ARRRandomHUD* Hud = Cast<ARRRandomHUD>(GetHUD()))
	{
		Hud->ToggleMap();
	}
}

void ARRRandomPlayerController::OnEscape()
{
	// Esc closes the full-screen map first
	ARRRandomHUD* Hud = Cast<ARRRandomHUD>(GetHUD());
	if (Hud && Hud->IsMapExpanded())
	{
		Hud->ToggleMap();
		return;
	}
	if (ARRRandomGameState::IsMatchOver(this))
	{
		RRTitle();
	}
	else
	{
		RRPause();
	}
}

void ARRRandomPlayerController::OnConfirm()
{
	if (bPauseMenuOpen)
	{
		RRTitle();
	}
	else
	{
		RRNewGame();
	}
}

void ARRRandomPlayerController::RRPause()
{
	if (bPauseMenuOpen)
	{
		ClosePauseMenu();
		return;
	}
	if (ARRRandomGameState::IsMatchOver(this))
	{
		return;
	}
	bPauseMenuOpen = true;
	// Only a solo game stops; pausing an online one would freeze everybody else too
	if (GetNetMode() == NM_Standalone)
	{
		SetPause(true);
	}
}

void ARRRandomPlayerController::ClosePauseMenu()
{
	bPauseMenuOpen = false;
	if (IsPaused())
	{
		SetPause(false);
	}
}

bool ARRRandomPlayerController::IsGameplayInputBlocked() const
{
	return bPauseMenuOpen || ARRRandomGameState::IsPlayLocked(this);
}

void ARRRandomPlayerController::RRWin()
{
	ServerCheatWin();
}

void ARRRandomPlayerController::ServerCheatWin_Implementation()
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	ARRRandomGameMode* GameMode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>();
	if (RandomCharacter && GameMode)
	{
		GameMode->CheatWin(RandomCharacter->GetTeam());
	}
#endif
}

void ARRRandomPlayerController::RRGiant()
{
	ServerCheatGiant();
}

void ARRRandomPlayerController::RRFly()
{
	ServerCheatFly();
}

void ARRRandomPlayerController::RRGun(const FString& Gun)
{
	// A number (1 = pistol) or a name, any case
	int32 Type = INDEX_NONE;
	if (Gun.IsNumeric())
	{
		Type = FCString::Atoi(*Gun) - 1;
	}
	else
	{
		for (int32 Index = 0; Index < FRRWeapon::GetTypeCount(); ++Index)
		{
			if (FRRWeapon::GetTypeName(Index).Equals(Gun.TrimStartAndEnd(), ESearchCase::IgnoreCase))
			{
				Type = Index;
			}
		}
	}
	if (Type < 0 || Type >= FRRWeapon::GetTypeCount())
	{
		TArray<FString> Names;
		for (int32 Index = 0; Index < FRRWeapon::GetTypeCount(); ++Index)
		{
			Names.Add(FString::Printf(TEXT("%d %s"), Index + 1, *FRRWeapon::GetTypeName(Index)));
		}
		ClientMessage(FString::Printf(TEXT("RRGun <name or number>: %s"), *FString::Join(Names, TEXT(", "))));
		return;
	}
	ServerCheatGun(Type);
}

void ARRRandomPlayerController::OnCheatGun(int32 Type)
{
	ServerCheatGun(Type);
}

void ARRRandomPlayerController::ServerCheatGiant_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->StartGiant(CheatSeconds);
	}
#endif
}

void ARRRandomPlayerController::ServerCheatFly_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->StartFlight(CheatSeconds);
	}
#endif
}

void ARRRandomPlayerController::ServerCheatGun_Implementation(int32 Type)
{
#if !UE_BUILD_SHIPPING
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (RandomCharacter && RandomCharacter->IsAlive() && Type >= 0 && Type < FRRWeapon::GetTypeCount())
	{
		RandomCharacter->EquipWeapon(FRRWeapon::Make(Type, CheatGunQuality, RandomCharacter->GetDefaultMagazineSize()));
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) cheats a %s."), *RandomCharacter->GetName(), RandomCharacter->GetTeam(), *FRRWeapon::GetTypeName(Type));
	}
#endif
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
	// In the pause menu and after the match the left button only clicks buttons, and on the map it only clicks the map
	// (OnClick)
	float MouseX;
	float MouseY;
	const ARRRandomHUD* MapHud = Cast<ARRRandomHUD>(GetHUD());
	if (IsGameplayInputBlocked() || (MapHud && GetMousePosition(MouseX, MouseY) && MapHud->IsPointOnMap(FVector2D(MouseX, MouseY))))
	{
		return;
	}
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
	if (IsGameplayInputBlocked())
	{
		return;
	}
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddMovementInput(FVector::ForwardVector, Input.Y);
		ControlledPawn->AddMovementInput(FVector::RightVector, Input.X);
	}
}

void ARRRandomPlayerController::OnJump()
{
	if (IsGameplayInputBlocked())
	{
		return;
	}
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
	if (IsGameplayInputBlocked())
	{
		return;
	}
	// The character asks the server when this is a client
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->RollDice();
	}
}

void ARRRandomPlayerController::OnReload()
{
	if (IsGameplayInputBlocked())
	{
		return;
	}
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->StartReload();
	}
}
