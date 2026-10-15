#include "RRRandomPlayerController.h"
#include "RRRandomFogOfWar.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomGameMode.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "RRRandomResultsDirector.h"
#include "RRRandomSessionSubsystem.h"
#include "RRRandomUltimate.h"
#include "RRRandomVehicle.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformApplicationMisc.h"
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
	// Third person: how far the crosshair looks for something to aim at
	constexpr float CrosshairRange = 20000.f;
	// Third person: the most one frame's mouse movement turns the camera (in mouse counts)
	constexpr float MaxLookDelta = 60.f;
	// Third person: how far (in pixels) the hidden cursor may drift from the middle of the view before it is put back
	constexpr int32 MouseRecenterSlack = 40;
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
	if (!RideAction)
	{
		RideAction = NewObject<UInputAction>(this, TEXT("IA_Ride"));
		RideAction->ValueType = EInputActionValueType::Boolean;
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
	if (!CheatGodAction)
	{
		CheatGodAction = NewObject<UInputAction>(this, TEXT("IA_CheatGod"));
		CheatGodAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!CheatUltimateAction)
	{
		CheatUltimateAction = NewObject<UInputAction>(this, TEXT("IA_CheatUltimate"));
		CheatUltimateAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!CheatThirdPersonAction)
	{
		CheatThirdPersonAction = NewObject<UInputAction>(this, TEXT("IA_CheatThirdPerson"));
		CheatThirdPersonAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!LookAction)
	{
		LookAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
		LookAction->ValueType = EInputActionValueType::Axis2D;
	}
	if (!FreeCursorAction)
	{
		FreeCursorAction = NewObject<UInputAction>(this, TEXT("IA_FreeCursor"));
		FreeCursorAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!AimAction)
	{
		AimAction = NewObject<UInputAction>(this, TEXT("IA_Aim"));
		AimAction->ValueType = EInputActionValueType::Boolean;
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
	MappingContext->MapKey(RideAction, EKeys::F);
	MappingContext->MapKey(HostAction, EKeys::F1);
	MappingContext->MapKey(JoinAction, EKeys::F2);
	MappingContext->MapKey(LeaveAction, EKeys::F3);
	MappingContext->MapKey(TitleAction, EKeys::Escape);
	MappingContext->MapKey(ConfirmAction, EKeys::Enter);
	MappingContext->MapKey(MapAction, EKeys::M);
	// Third person: the mouse looks, the right button aims down the sights
	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(AimAction, EKeys::RightMouseButton);
	// Third person: hold Left Alt to get the cursor back (to click the map, or leave the game window)
	MappingContext->MapKey(FreeCursorAction, EKeys::LeftAlt);
#if !UE_BUILD_SHIPPING
	MappingContext->MapKey(CheatGiantAction, EKeys::F7);
	// F9 is the engine's screenshot key; F10 is free
	MappingContext->MapKey(CheatGodAction, EKeys::F10);
	// U, and F11 (which the editor's viewport keeps for itself when playing in the editor)
	MappingContext->MapKey(CheatUltimateAction, EKeys::U);
	MappingContext->MapKey(CheatUltimateAction, EKeys::F11);
	MappingContext->MapKey(CheatThirdPersonAction, EKeys::F9);
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
			// The brawler picked on the title (the host's own was spawned as it already)
			if (!HasAuthority())
			{
				ServerChooseSpecies(static_cast<uint8>(Sessions->GetSpecies()));
			}
		}
		// Fog of war for this screen only (none in a third-person game: PlayerTick also takes it away if the game state
		// says so only after this)
		if (bFogOfWar && !ARRRandomGameState::IsThirdPersonMode(this))
		{
			FActorSpawnParameters FogParams;
			FogParams.Owner = this;
			FogOfWar = GetWorld()->SpawnActor<ARRRandomFogOfWar>(FogParams);
		}
	}
}

void ARRRandomPlayerController::ServerChooseSpecies_Implementation(uint8 Species)
{
	if (Species >= RRSpecies::Count)
	{
		return;
	}
	ChosenSpecies = Species;
	if (ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetPawn()))
	{
		Brawler->SetSpecies(Species);
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
	EnhancedInput->BindAction(RideAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnRide);
	EnhancedInput->BindAction(HostAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRHost);
	EnhancedInput->BindAction(JoinAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRJoin);
	EnhancedInput->BindAction(LeaveAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRLeave);
	EnhancedInput->BindAction(TitleAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnEscape);
	EnhancedInput->BindAction(CheatWinAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRWin);
	EnhancedInput->BindAction(CheatGiantAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRGiant);
	EnhancedInput->BindAction(CheatGodAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRGod);
	EnhancedInput->BindAction(CheatUltimateAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRUltimate);
	EnhancedInput->BindAction(CheatThirdPersonAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRThirdPerson);
	EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARRRandomPlayerController::OnLook);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnAimStart);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &ARRRandomPlayerController::OnAimStop);
	EnhancedInput->BindAction(FreeCursorAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnFreeCursorStart);
	EnhancedInput->BindAction(FreeCursorAction, ETriggerEvent::Completed, this, &ARRRandomPlayerController::OnFreeCursorStop);
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
	// A third-person game: this screen goes over the shoulder as soon as it has a brawler (and again for a new one), with
	// no fog of war at all
	if (IsLocalController() && ARRRandomGameState::IsThirdPersonMode(this))
	{
		const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
		if (RandomCharacter && (!bThirdPerson || !RandomCharacter->IsThirdPerson()))
		{
			SetThirdPersonView(true);
		}
		if (FogOfWar)
		{
			FogOfWar->Destroy();
			FogOfWar = nullptr;
		}
	}
	// Third person hides the cursor for the mouse look. It comes back for the pause menu and the results screen (their
	// buttons), while Left Alt is held, and whenever the game's window isn't the one in use (another window clicked,
	// Alt+Tab): the mouse is then the player's again; clicking back into the game takes it over again
	if (bThirdPerson && IsLocalController())
	{
		const ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn());
		const bool bWantCursor = bPauseMenuOpen || ARRRandomGameState::IsMatchOver(this) || bCursorFreed || !IsViewportFocused()
			|| (Mine && Mine->GetUltimate() && Mine->GetUltimate()->KeepsOverview());
		if (bShowMouseCursor != bWantCursor)
		{
			bShowMouseCursor = bWantCursor;
			if (bWantCursor)
			{
				FInputModeGameAndUI Mode;
				Mode.SetHideCursorDuringCapture(false);
				SetInputMode(Mode);
			}
			else
			{
				SetInputMode(FInputModeGameOnly());
			}
			LockMouse(!bWantCursor);
		}
		// While the game has it, the hidden cursor stays in the middle of the view: held inside the window, and put back in
		// the middle whenever it has wandered (where the system doesn't keep it still by itself)
		if (!bWantCursor)
		{
			LockMouse(true);
			CenterMouse();
		}
	}

	// My ultimate wants spots: the view goes up over the arena to pick them, and back down after
	if (IsLocalController() && !ResultsDirector)
	{
		const ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn());
		ARRRandomUltimate* Ultimate = Mine ? Mine->GetUltimate() : nullptr;
		ARRRandomUltimate* Picking = Ultimate && Ultimate->KeepsOverview() ? Ultimate : nullptr;
		if (Picking && GetViewTarget() != Picking)
		{
			SetViewTargetWithBlend(Picking, 0.8f, VTBlend_EaseInOut, 2.f);
			bViewingSpirit = true;
		}
		// The area's hologram under the cursor
		if (Ultimate)
		{
			FVector Spot;
			const bool bSpot = Ultimate->IsPicking() && GetUltimateSpot(Spot);
			Ultimate->ShowCursorSpot(bSpot, Spot);
		}
		if (!Picking && bViewingSpirit && GetPawn())
		{
			SetViewTargetWithBlend(GetPawn(), 0.8f, VTBlend_EaseInOut, 2.f);
			bViewingSpirit = false;
		}
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
			// The show moves its camera in from this view itself (a view blend would swing up to the sky on the way)
			ResultsDirector->Setup(State->GetMVP(), PlayerCameraManager ? PlayerCameraManager->GetCameraCacheView() : FMinimalViewInfo(), ResultsBlendTime);
			SetViewTarget(ResultsDirector);
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
		// (in third person with the cursor hidden the button only shoots; with it out, Alt held, it clicks the map)
		if (bThirdPerson && !bShowMouseCursor)
		{
			return;
		}
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

void ARRRandomPlayerController::RRGod()
{
	ServerCheatGod();
}

void ARRRandomPlayerController::RRUltimate()
{
	ServerCheatUltimate();
}

void ARRRandomPlayerController::ServerCheatUltimate_Implementation()
{
#if !UE_BUILD_SHIPPING
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter || !RandomCharacter->IsAlive())
	{
		return;
	}
	RandomCharacter->GetDiceBuffs()->GiveUltimateDie();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, %s gets an ultimate die."), *RandomCharacter->GetName());
#endif
}

void ARRRandomPlayerController::ServerUltimateStrike_Implementation(FVector_NetQuantize Spot)
{
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (RandomCharacter && RandomCharacter->GetUltimate())
	{
		RandomCharacter->GetUltimate()->RequestSpot(Spot);
	}
}

bool ARRRandomPlayerController::GetUltimateSpot(FVector& OutSpot) const
{
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	FVector Origin;
	FVector Direction;
	if (!RandomCharacter || !RandomCharacter->GetUltimate() || !DeprojectMousePositionToWorld(Origin, Direction))
	{
		return false;
	}
	// Where the cursor's line meets the floor's height (anywhere, past the edge too: the strike's spot is a place on the
	// floor, not someone, nor the top of something tall in the way), then down onto the ground there
	const ARRRandomUltimate* Spirit = RandomCharacter->GetUltimate();
	if (Direction.Z >= -0.01f)
	{
		return false;
	}
	const FVector OnFloor = Origin + Direction * ((Spirit->GetGroundZ() - Origin.Z) / Direction.Z);
	OutSpot = Spirit->FindGround(OnFloor);
	return true;
}

void ARRRandomPlayerController::ServerCheatGod_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->SetInvincible(!RandomCharacter->IsInvincible());
		UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, %s is %s."), *RandomCharacter->GetName(), RandomCharacter->IsInvincible() ? TEXT("invincible") : TEXT("mortal again"));
	}
#endif
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
	if (IsGameplayInputBlocked() || (!bThirdPerson && MapHud && GetMousePosition(MouseX, MouseY) && MapHud->IsPointOnMap(FVector2D(MouseX, MouseY))))
	{
		return;
	}
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	// My ultimate wants spots: the button picks one (once per hit, however long it's held; a click while the last one is
	// under way waits for its turn)
	const ARRRandomUltimate* Ultimate = RandomCharacter ? RandomCharacter->GetUltimate() : nullptr;
	if (Ultimate && Ultimate->IsPicking())
	{
		FVector Spot;
		if (GetWorld()->GetTimeSeconds() - LastStrikeRequestTime > 0.3f && GetUltimateSpot(Spot))
		{
			LastStrikeRequestTime = GetWorld()->GetTimeSeconds();
			ServerUltimateStrike(Spot);
		}
		return;
	}
	if (RandomCharacter && bThirdPerson)
	{
		// Not with the cursor out (Alt held, or the click that takes the mouse back into the game)
		if (!bShowMouseCursor && IsViewportFocused())
		{
			RandomCharacter->FireAt(GetCrosshairTarget());
		}
		return;
	}
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
		// In third person the camera turns: forward is where it looks, on the ground
		const FRotator Heading(0.f, bThirdPerson ? GetControlRotation().Yaw : 0.f, 0.f);
		ControlledPawn->AddMovementInput(Heading.RotateVector(FVector::ForwardVector), Input.Y);
		ControlledPawn->AddMovementInput(Heading.RotateVector(FVector::RightVector), Input.X);
	}
}

void ARRRandomPlayerController::RRThirdPerson()
{
#if !UE_BUILD_SHIPPING
	// A third-person game stays in third person
	if (ARRRandomGameState::IsThirdPersonMode(this) || !Cast<ARRRandomCharacter>(GetPawn()))
	{
		return;
	}
	SetThirdPersonView(!bThirdPerson);
	ClientMessage(bThirdPerson ? TEXT("Cheat: third person (right button: aim)") : TEXT("Cheat: top-down view"));
#endif
}

void ARRRandomPlayerController::SetThirdPersonView(bool bOn)
{
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter)
	{
		return;
	}
	if (!bOn)
	{
		LockMouse(false);
	}
	bThirdPerson = bOn;
	if (bThirdPerson)
	{
		// Start looking the way the brawler faces, a little down
		SetControlRotation(FRotator(-10.f, RandomCharacter->GetActorRotation().Yaw, 0.f));
	}
	RandomCharacter->SetThirdPerson(bThirdPerson);
	// The mouse turns the camera instead of pointing at the ground
	bShowMouseCursor = !bThirdPerson;
	if (bThirdPerson)
	{
		SetInputMode(FInputModeGameOnly());
	}
	else
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -70.f;
		PlayerCameraManager->ViewPitchMax = 50.f;
	}
	if (FogOfWar)
	{
		FogOfWar->SetShadingVisible(!bThirdPerson);
	}
}

void ARRRandomPlayerController::OnLook(const FInputActionValue& Value)
{
	// Only while the game has the mouse: not with the cursor out (menus, Alt, another window in use), so moving the mouse
	// for anything else never turns the camera
	if (!bThirdPerson || IsGameplayInputBlocked() || bShowMouseCursor || !IsViewportFocused())
	{
		return;
	}
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	const float Sensitivity = LookSensitivity * (RandomCharacter && RandomCharacter->IsAimingDownSights() ? SightsSensitivity : 1.f);
	// A jump of the pointer (taking the mouse back, a window switch) is not a turn
	const FVector2D Delta = Value.Get<FVector2D>().ClampAxes(-MaxLookDelta, MaxLookDelta);
	AddYawInput(Delta.X * Sensitivity);
	// Mouse up looks up
	AddPitchInput(-Delta.Y * Sensitivity);
}

void ARRRandomPlayerController::OnAimStart()
{
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()); RandomCharacter && bThirdPerson)
	{
		RandomCharacter->SetAimingDownSights(true);
	}
}

void ARRRandomPlayerController::OnAimStop()
{
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->SetAimingDownSights(false);
	}
}

FVector ARRRandomPlayerController::GetCrosshairTarget() const
{
	// From the camera through the middle of the screen, past the brawler itself, to the first thing in the way
	FVector Eye;
	FRotator Look;
	GetPlayerViewPoint(Eye, Look);
	const FVector Direction = Look.Vector();
	const APawn* ControlledPawn = GetPawn();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomCrosshair), false, ControlledPawn);
	if (ControlledPawn && ControlledPawn->GetAttachParentActor())
	{
		Params.AddIgnoredActor(ControlledPawn->GetAttachParentActor());
	}
	const FVector Start = Eye + Direction * (ControlledPawn ? FVector::Dist(Eye, ControlledPawn->GetActorLocation()) : 0.f);
	const FVector End = Eye + Direction * CrosshairRange;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}
	return End;
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

void ARRRandomPlayerController::OnRide()
{
	if (IsGameplayInputBlocked())
	{
		return;
	}
	// The character asks the server when this is a client
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->ToggleRide();
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

bool ARRRandomPlayerController::IsViewportFocused() const
{
	const UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	return Viewport && Viewport->HasFocus() && FPlatformApplicationMisc::IsThisApplicationForeground();
}

void ARRRandomPlayerController::OnFreeCursorStart()
{
	bCursorFreed = true;
}

void ARRRandomPlayerController::OnFreeCursorStop()
{
	bCursorFreed = false;
}

void ARRRandomPlayerController::LockMouse(bool bLock)
{
	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (!Viewport)
	{
		return;
	}
	ViewportClient->SetMouseLockMode(bLock ? EMouseLockMode::LockAlways : EMouseLockMode::DoNotLock);
	ViewportClient->SetMouseCaptureMode(bLock ? EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown : EMouseCaptureMode::CaptureDuringMouseDown);
	ViewportClient->SetHideCursorDuringCapture(bLock);
	if (bLock && !Viewport->HasMouseCapture())
	{
		Viewport->CaptureMouse(true);
	}
	else if (!bLock && Viewport->HasMouseCapture())
	{
		Viewport->CaptureMouse(false);
	}
	Viewport->LockMouseToViewport(bLock);
}

void ARRRandomPlayerController::CenterMouse()
{
	const UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (!Viewport)
	{
		return;
	}
	const FIntPoint Size = Viewport->GetSizeXY();
	const FIntPoint Middle(Size.X / 2, Size.Y / 2);
	const FIntPoint Mouse(Viewport->GetMouseX(), Viewport->GetMouseY());
	// Only once it has drifted a bit, so a system that keeps the cursor still isn't nudged every frame
	if (FMath::Abs(Mouse.X - Middle.X) > MouseRecenterSlack || FMath::Abs(Mouse.Y - Middle.Y) > MouseRecenterSlack)
	{
		// (the camera turns by the raw mouse movement, so the jump back doesn't turn it)
		Viewport->SetMouse(Middle.X, Middle.Y);
	}
}
