#include "RRRandomPlayerController.h"
#include "RRRandomFogOfWar.h"
#include "RRRandomCannon.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomGameMode.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "RRRandomMeteor.h"
#include "RRRandomMovementComponent.h"
#include "RRRandomOre.h"
#include "RRRandomPlaneStation.h"
#include "RRRandomResultsDirector.h"
#include "RRRandomSessionSubsystem.h"
#include "RRRandomStarRobot.h"
#include "RRRandomUltimate.h"
#include "RRRandomVehicle.h"
#include "RRRandomWreckage.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
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
	// The clash test (Ctrl+8: the 8 key is gun 7, counting from 0): its middle this far ahead of me, the two robots this
	// far apart (within the sword's reach), both swinging this long after they're made (done and their crews in)
	constexpr int32 ClashTestGunKey = 7;
	constexpr float ClashTestAhead = 900.f;
	constexpr float ClashTestGap = 800.f;
	constexpr float ClashTestSwingDelay = 1.2f;
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
	if (!CheatStarCarsAction)
	{
		CheatStarCarsAction = NewObject<UInputAction>(this, TEXT("IA_CheatStarCars"));
		CheatStarCarsAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!CheatClashTestAction)
	{
		CheatClashTestAction = NewObject<UInputAction>(this, TEXT("IA_CheatClashTest"));
		CheatClashTestAction->ValueType = EInputActionValueType::Boolean;
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
	if (!SprintAction)
	{
		SprintAction = NewObject<UInputAction>(this, TEXT("IA_Sprint"));
		SprintAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!DescendAction)
	{
		DescendAction = NewObject<UInputAction>(this, TEXT("IA_Descend"));
		DescendAction->ValueType = EInputActionValueType::Boolean;
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
	MappingContext->MapKey(SprintAction, EKeys::LeftShift);
	MappingContext->MapKey(SprintAction, EKeys::RightShift);
	MappingContext->MapKey(DescendAction, EKeys::LeftControl);
	MappingContext->MapKey(DescendAction, EKeys::RightControl);
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
	// Ctrl + 0 (the 0 alone does nothing: OnCheatStarCarsKey checks for Ctrl)
	MappingContext->MapKey(CheatStarCarsAction, EKeys::Zero);
	// Ctrl + 8 (OnCheatClashTestKey checks for Ctrl)
	MappingContext->MapKey(CheatClashTestAction, EKeys::Eight);
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
	EnhancedInput->BindAction(CheatStarCarsAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnCheatStarCarsKey);
	EnhancedInput->BindAction(CheatClashTestAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnCheatClashTestKey);
	EnhancedInput->BindAction(CheatGiantAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRGiant);
	EnhancedInput->BindAction(CheatGodAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRGod);
	EnhancedInput->BindAction(CheatUltimateAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRUltimate);
	EnhancedInput->BindAction(CheatThirdPersonAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::RRThirdPerson);
	EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARRRandomPlayerController::OnLook);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnAimStart);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Triggered, this, &ARRRandomPlayerController::OnAimHeld);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &ARRRandomPlayerController::OnAimStop);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnSprintStart);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &ARRRandomPlayerController::OnSprintStop);
	EnhancedInput->BindAction(DescendAction, ETriggerEvent::Started, this, &ARRRandomPlayerController::OnDescendStart);
	EnhancedInput->BindAction(DescendAction, ETriggerEvent::Completed, this, &ARRRandomPlayerController::OnDescendStop);
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

	UpdateSprint();
	// Locked in a sword clash in the star robot: W A S D are the clash's keys (the robot can't walk meanwhile)
	if (ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn()); Mine && Mine->IsInRobot() && Mine->GetRobot()->IsClashing() && !IsGameplayInputBlocked())
	{
		const FKey ClashKeys[] = { EKeys::W, EKeys::A, EKeys::S, EKeys::D };
		for (uint8 Key = 0; Key < UE_ARRAY_COUNT(ClashKeys); ++Key)
		{
			if (WasInputKeyJustPressed(ClashKeys[Key]))
			{
				Mine->RobotClashKey(Key);
			}
		}
	}
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

	// RRStudioCam (not while my ultimate has the view over the arena)
	if (!bViewingSpirit)
	{
		UpdateStudioCam(DeltaTime);
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

void ARRRandomPlayerController::RRSprint(int32 bOn)
{
#if !UE_BUILD_SHIPPING
	bSprintHeld = bOn != 0;
#endif
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
	// Ctrl+8 is the sword clash test's (OnCheatClashTestKey), not a gun's
	if (Type == ClashTestGunKey && (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl)))
	{
		return;
	}
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

void ARRRandomPlayerController::RRTeamUltimate()
{
	ServerCheatTeamUltimate();
}

void ARRRandomPlayerController::ServerCheatTeamUltimate_Implementation()
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter)
	{
		return;
	}
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() == RandomCharacter->GetTeam() && It->IsAlive() && !It->IsDecoy())
		{
			It->GetDiceBuffs()->GiveUltimateDie();
			UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, %s gets an ultimate die (team)."), *It->GetName());
		}
	}
#endif
}

void ARRRandomPlayerController::RRMeteor()
{
	UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, asking for a meteor."));
	ServerCheatMeteor();
}

void ARRRandomPlayerController::RROre()
{
	ServerCheatOre();
}

void ARRRandomPlayerController::ServerCheatMeteor_Implementation()
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (!RandomCharacter || !State)
	{
		return;
	}
	// On the floor 700 ahead (under the brawler's feet if there is none there)
	const FVector Ahead = RandomCharacter->GetActorLocation() + RandomCharacter->GetActorForwardVector().GetSafeNormal2D() * 700.f;
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomCheatMeteor), false, RandomCharacter);
	const bool bFloor = GetWorld()->LineTraceSingleByObjectType(Floor, Ahead + FVector(0.f, 0.f, 300.f), Ahead - FVector(0.f, 0.f, 3000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	const FTransform Spot(bFloor ? Floor.ImpactPoint : RandomCharacter->GetActorLocation() - FVector(0.f, 0.f, RandomCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	if (ARRRandomMeteor* Meteor = GetWorld()->SpawnActorDeferred<ARRRandomMeteor>(ARRRandomMeteor::StaticClass(), Spot, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Meteor->Setup(State->GetServerWorldTimeSeconds() + ARRRandomMeteor::WarnTime, false);
		Meteor->FinishSpawning(Spot);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, a meteor comes down at %s."), *Spot.GetLocation().ToString());
	}
#endif
}

void ARRRandomPlayerController::RRWreck(float Radius, int32 bColumn)
{
	// Where the middle of the view first meets something solid (my own animal friend aside), at most 8000 away
	FVector ViewFrom;
	FRotator ViewTurn;
	GetPlayerViewPoint(ViewFrom, ViewTurn);
	const FVector ViewTo = ViewFrom + ViewTurn.Vector() * 8000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomCheatWreck), false, GetPawn());
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, ViewFrom, ViewTo, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	ServerCheatWreck(bHit ? FVector(Hit.ImpactPoint) : ViewTo, Radius, bColumn != 0);
}

void ARRRandomPlayerController::ServerCheatWreck_Implementation(FVector_NetQuantize Spot, float Radius, bool bColumn)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, wrecking round %s."), *Spot.ToCompactString());
	ARRRandomWreckage::Wreck(this, Spot, Spot, FMath::Clamp(Radius, 50.f, 3000.f), bColumn);
#endif
}

void ARRRandomPlayerController::RRStudioCam(float Yaw, float Distance, float Height)
{
	if (!StudioCam)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.ObjectFlags |= RF_Transient;
		StudioCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
	}
	const bool bWasOn = bStudioCam;
	bStudioCam = StudioCam != nullptr;
	StudioYaw = Yaw;
	StudioDistance = Distance;
	StudioHeight = Height;
	if (!bWasOn && GetPawn())
	{
		StudioFacing = GetPawn()->GetActorRotation().Yaw;
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: studio camera at %.0f degrees round from the front."), Yaw);
}

void ARRRandomPlayerController::RRStudioCamOff()
{
	bStudioCam = false;
	if (GetPawn())
	{
		SetViewTarget(GetPawn());
	}
}

void ARRRandomPlayerController::UpdateStudioCam(float DeltaTime)
{
	if (!bStudioCam || !StudioCam || !IsLocalController() || ResultsDirector)
	{
		return;
	}
	// Who it watches: my animal friend, or the robot I'm aboard (or combining)
	AActor* Subject = GetPawn();
	const ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(Subject);
	ARRRandomStarRobot* Robot = Mine ? Mine->GetRobot() : nullptr;
	const bool bRobot = Robot != nullptr;
	if (bRobot)
	{
		Subject = Robot;
	}
	if (!Subject)
	{
		return;
	}
	// Round from its front, turning with it smoothly (a quick turn isn't a jolt of the view)
	const float Real = DeltaTime / FMath::Max(UGameplayStatics::GetGlobalTimeDilation(this), 0.01f);
	StudioFacing = FRotator::NormalizeAxis(StudioFacing + FRotator::NormalizeAxis(Subject->GetActorRotation().Yaw - StudioFacing) * FMath::Min(1.f, Real * 3.f));
	const FVector Middle = Subject->GetActorLocation() + FVector(0.f, 0.f, bRobot ? 320.f : 0.f);
	const float Distance = StudioDistance > 0.f ? StudioDistance : (bRobot ? 2100.f : 620.f);
	const float Height = StudioHeight != 0.f ? StudioHeight : (bRobot ? 120.f : 40.f);
	const FVector Where = Middle + FRotator(0.f, StudioFacing + StudioYaw, 0.f).Vector() * Distance + FVector(0.f, 0.f, Height);
	StudioCam->SetActorLocationAndRotation(Where, (Middle - Where).Rotation());
	if (GetViewTarget() != StudioCam)
	{
		SetViewTarget(StudioCam);
	}
}

void ARRRandomPlayerController::RRSlomo(float Rate)
{
	ServerCheatSlomo(Rate);
}

void ARRRandomPlayerController::ServerCheatSlomo_Implementation(float Rate)
{
#if !UE_BUILD_SHIPPING
	UGameplayStatics::SetGlobalTimeDilation(this, FMath::Clamp(Rate, 0.05f, 4.f));
	UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, the game runs at %.2f speed."), Rate);
#endif
}

void ARRRandomPlayerController::RRRobotFire(float Distance)
{
	ServerCheatRobotFire(Distance, false);
}

void ARRRandomPlayerController::RRRobotMissiles(float Distance)
{
	ServerCheatRobotFire(Distance, true);
}

void ARRRandomPlayerController::ServerCheatRobotFire_Implementation(float Distance, bool bMissiles)
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn());
	ARRRandomStarRobot* Robot = Mine ? Mine->GetRobot() : nullptr;
	ARRRandomCharacter* Gunner = Robot ? Robot->GetCrew(ERRRobotSeat::Attack) : nullptr;
	if (!Gunner)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, no robot with someone in its ATTACK seat to fire."));
		return;
	}
	// Ahead on the floor, about an animal friend's chest high; at the rival robot if there is one
	const ARRRandomStarRobot* Rival = Robot->GetRival();
	const FVector Ahead = Rival ? Rival->GetActorLocation() + FVector(0.f, 0.f, 300.f)
		: Robot->GetActorLocation() + Robot->GetActorForwardVector().GetSafeNormal2D() * FMath::Max(Distance, 300.f) + FVector(0.f, 0.f, 60.f);
	if (bMissiles)
	{
		Robot->CrewAltFire(Gunner, Ahead);
	}
	else
	{
		Robot->CrewFire(Gunner, Ahead);
	}
#endif
}

void ARRRandomPlayerController::RRRivalRobot(float Distance)
{
	ServerCheatRivalRobot(Distance);
}

void ARRRandomPlayerController::ServerCheatRivalRobot_Implementation(float Distance)
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn());
	if (!Mine)
	{
		return;
	}
	// In front of me (or my robot), on the floor, facing back at me
	const AActor* From = Mine->GetRobot() ? static_cast<const AActor*>(Mine->GetRobot()) : Mine;
	const float Yaw = From->GetActorRotation().Yaw;
	FVector Where = From->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * Distance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRCheatRivalRobot), false, From);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Where + FVector(0.f, 0.f, 600.f), Where - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
	{
		Where = Hit.ImpactPoint;
	}
	// One of the other team's bots combines it
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() != Mine->GetTeam() && !It->IsPlayerControlled() && It->IsAlive() && !It->GetRobot())
		{
			if (!ARRRandomStarRobot::CheatAssemble(*It, Where, Yaw + 180.f))
			{
				UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, team %d's star robot can't be made (its cars in one already?)."), It->GetTeam());
			}
			return;
		}
	}
#endif
}

void ARRRandomPlayerController::RRClashCam(float Ahead, float Side, float Up, float LookRival, float FieldOfView)
{
	if (ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn()))
	{
		Mine->SetClashCameraTuning(Ahead, Side, Up, LookRival, FieldOfView);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: clash camera set: %.0f ahead, %.0f right, %.0f up, looking %.2f on to the rival, view %.0f."), Ahead, Side, Up, LookRival, FieldOfView);
	}
}

void ARRRandomPlayerController::RRClashTest()
{
	ServerCheatClashTest();
}

void ARRRandomPlayerController::ServerCheatClashTest_Implementation()
{
#if !UE_BUILD_SHIPPING
	ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn());
	UWorld* World = GetWorld();
	if (!Mine || !Mine->IsAlive() || !World)
	{
		return;
	}
	// Any robot about comes apart first (its cars out again, everyone out)
	TArray<ARRRandomStarRobot*> Robots;
	for (TActorIterator<ARRRandomStarRobot> It(World); It; ++It)
	{
		Robots.Add(*It);
	}
	for (ARRRandomStarRobot* Robot : Robots)
	{
		Robot->CheatBreak();
	}
	// The floor ahead of me: the two robots either side of the middle, facing each other
	const float Yaw = Mine->GetActorRotation().Yaw;
	const FVector Forward = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector Middle = Mine->GetActorLocation() + Forward * ClashTestAhead;
	const auto Floor = [World, Mine](const FVector& Spot)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RRCheatClashTest), false, Mine);
		return World->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 600.f), Spot - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params) ? Hit.ImpactPoint : Spot;
	};
	// The other team's robot is combined by one of its players if it has any there, else by one of its bots
	ARRRandomCharacter* Theirs = nullptr;
	for (TActorIterator<ARRRandomCharacter> It(World); It; ++It)
	{
		if (It->GetTeam() != Mine->GetTeam() && It->IsAlive() && !It->GetRobot() && !It->IsDecoy() && (!Theirs || (It->IsPlayerControlled() && !Theirs->IsPlayerControlled())))
		{
			Theirs = *It;
		}
	}
	ARRRandomStarRobot* Ours = ARRRandomStarRobot::CheatAssemble(Mine, Floor(Middle - Forward * ClashTestGap * 0.5f), Yaw);
	ARRRandomStarRobot* Rival = Theirs ? ARRRandomStarRobot::CheatAssemble(Theirs, Floor(Middle + Forward * ClashTestGap * 0.5f), Yaw + 180.f) : nullptr;
	if (!Ours || !Rival)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, the clash test needs both teams' star cars free and someone on the other team."));
		return;
	}
	// Once both are done and their crews in, both swing at once
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Ours = TWeakObjectPtr<ARRRandomStarRobot>(Ours), Rival = TWeakObjectPtr<ARRRandomStarRobot>(Rival)]()
	{
		if (Ours.IsValid() && Rival.IsValid())
		{
			Ours->CheatSwingAt(Rival.Get());
			Rival->CheatSwingAt(Ours.Get());
			UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, both star robots swing at once (clash test)."));
		}
	}), ClashTestSwingDelay, false);
#endif
}

void ARRRandomPlayerController::RRClashPress()
{
#if !UE_BUILD_SHIPPING
	ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn());
	const ARRRandomStarRobot* Robot = Mine && Mine->IsInRobot() ? Mine->GetRobot() : nullptr;
	if (Robot && Robot->IsClashing())
	{
		const int32 Seat = Mine->GetRobotSeat() == ERRRobotSeat::Move ? 0 : Mine->GetRobotSeat() == ERRRobotSeat::Attack ? 1 : 2;
		Mine->RobotClashKey(Robot->GetClashKey(Seat, Robot->GetClashDone(Seat)));
	}
#endif
}

void ARRRandomPlayerController::RRRival(const FString& What)
{
	ServerCheatRival(What);
}

void ARRRandomPlayerController::ServerCheatRival_Implementation(const FString& What)
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn());
	const AActor* Me = Mine && Mine->GetRobot() ? static_cast<const AActor*>(Mine->GetRobot()) : Mine;
	ARRRandomStarRobot* Rival = nullptr;
	for (TActorIterator<ARRRandomStarRobot> It(GetWorld()); It && Mine; ++It)
	{
		Rival = It->GetTeam() != Mine->GetTeam() && It->IsAssembled() ? *It : Rival;
	}
	if (!Rival || !Me)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, no rival robot (RRRivalRobot makes one)."));
		return;
	}
	const FVector At = Me->GetActorLocation() + FVector(0.f, 0.f, 300.f);
	if (What == TEXT("fire"))
	{
		Rival->CrewFire(Rival->GetCrew(ERRRobotSeat::Attack), At);
	}
	else if (What == TEXT("missiles"))
	{
		Rival->CrewAltFire(Rival->GetCrew(ERRRobotSeat::Attack), At);
	}
	else if (What == TEXT("shield"))
	{
		Rival->CheatShield(3.f);
	}
	else if (What == TEXT("hurt"))
	{
		Rival->CheatHurt(40.f);
	}
#endif
}

void ARRRandomPlayerController::RRRobotShield(float Seconds)
{
	ServerCheatRobotShield(Seconds);
}

void ARRRandomPlayerController::ServerCheatRobotShield_Implementation(float Seconds)
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* Mine = Cast<ARRRandomCharacter>(GetPawn());
	if (ARRRandomStarRobot* Robot = Mine ? Mine->GetRobot() : nullptr)
	{
		Robot->CheatShield(Seconds);
	}
#endif
}

void ARRRandomPlayerController::RRStar()
{
	ServerCheatStar();
}

void ARRRandomPlayerController::RRHurt(float Amount)
{
	ServerCheatHurt(Amount);
}

void ARRRandomPlayerController::RRRoll()
{
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->RollDice();
	}
}

void ARRRandomPlayerController::RRRide()
{
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->ToggleRide();
	}
}

void ARRRandomPlayerController::RRShoot()
{
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter)
	{
		return;
	}
	FVector Target = RandomCharacter->GetActorLocation() + RandomCharacter->GetActorForwardVector() * 1500.f;
	float Nearest = UE_BIG_NUMBER;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist(It->GetActorLocation(), RandomCharacter->GetActorLocation());
		if (It->GetTeam() != RandomCharacter->GetTeam() && It->IsAlive() && Distance < Nearest)
		{
			Nearest = Distance;
			Target = It->GetActorLocation();
		}
	}
	RandomCharacter->FireAt(Target);
}

void ARRRandomPlayerController::OnCheatStarCarsKey()
{
	if (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl))
	{
		RRStarCars();
	}
}

void ARRRandomPlayerController::OnCheatClashTestKey()
{
	if (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl))
	{
		RRClashTest();
	}
}

void ARRRandomPlayerController::RRStarCars()
{
	ServerCheatStarCars();
}

void ARRRandomPlayerController::ServerCheatStarCars_Implementation()
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter || !RandomCharacter->IsAlive())
	{
		return;
	}
	// My team's cars: my team's robot already about (combining or done) comes apart first, everyone out
	if (ARRRandomStarRobot* Robot = ARRRandomStarRobot::Get(this, RandomCharacter->GetTeam()))
	{
		Robot->CheatBreak();
	}
	// Ahead of me, left, middle and right, facing me
	const FVector Feet = RandomCharacter->GetActorLocation() - FVector(0.f, 0.f, RandomCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FRotator Facing(0.f, RandomCharacter->GetActorRotation().Yaw, 0.f);
	for (TActorIterator<ARRRandomVehicle> It(GetWorld()); It; ++It)
	{
		const int32 Part = It->GetStarPart();
		if (Part < 0 || It->IsCombined() || It->GetStarTeam() != RandomCharacter->GetTeam())
		{
			continue;
		}
		// Whoever rides it gets put off where they are (the one behind first)
		for (ARRRandomCharacter* Rider : { It->GetPassenger(), It->GetRider() })
		{
			if (Rider)
			{
				Rider->LeaveVehicle();
			}
		}
		const float Side = Part == 0 ? 0.f : Part == 1 ? -1.f : 1.f;
		FVector Spot = Feet + Facing.RotateVector(FVector(Part == 0 ? 650.f : 480.f, Side * 420.f, 0.f));
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RRCheatStarCars), false, *It);
		Params.AddIgnoredActor(RandomCharacter);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 400.f), Spot - FVector(0.f, 0.f, 1000.f), ECC_Visibility, Params))
		{
			Spot = Hit.ImpactPoint;
		}
		It->SetActorLocationAndRotation(Spot, FRotator(0.f, Facing.Yaw + 180.f - Side * 30.f, 0.f));
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, the star cars park in front of %s."), *RandomCharacter->GetName());
#endif
}

void ARRRandomPlayerController::RROwnDie(int32 Face, int32 bEveryone)
{
	ServerCheatOwnDie(Face, bEveryone != 0);
}

void ARRRandomPlayerController::ServerCheatOwnDie_Implementation(int32 Face, bool bEveryone)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, own die %d%s."), Face, bEveryone ? TEXT(" for everyone") : TEXT(""));
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if ((bEveryone || *It == GetPawn()) && It->IsAlive() && !It->IsDecoy())
		{
			It->CheatOwnDie(FMath::Clamp(Face, 1, 6));
		}
	}
#endif
}

void ARRRandomPlayerController::ServerCheatHurt_Implementation(float Amount)
{
#if !UE_BUILD_SHIPPING
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()); RandomCharacter && RandomCharacter->IsAlive())
	{
		RandomCharacter->TakeDamage(FMath::Max(0.f, Amount), FDamageEvent(), nullptr, nullptr);
	}
#endif
}

void ARRRandomPlayerController::ServerCheatStar_Implementation()
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter)
	{
		return;
	}
	const FVector Ahead = RandomCharacter->GetActorLocation() + RandomCharacter->GetActorForwardVector().GetSafeNormal2D() * 500.f;
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomCheatStar), false, RandomCharacter);
	if (!GetWorld()->LineTraceSingleByObjectType(Floor, Ahead + FVector(0.f, 0.f, 300.f), Ahead - FVector(0.f, 0.f, 3000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return;
	}
	const FTransform From(Floor.ImpactPoint + FVector(-900.f, 300.f, 2800.f));
	if (ARRRandomOre* Star = GetWorld()->SpawnActorDeferred<ARRRandomOre>(ARRRandomOre::StaticClass(), From, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Star->SetupFallingStar(FMath::RandBool() ? ERROreKind::Ore : ERROreKind::Big, Floor.ImpactPoint);
		Star->FinishSpawning(From);
	}
#endif
}

void ARRRandomPlayerController::ServerCheatOre_Implementation()
{
#if !UE_BUILD_SHIPPING
	const ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter)
	{
		return;
	}
	// In a line straight ahead, so walking forward picks them up one after another
	const FVector Forward = RandomCharacter->GetActorForwardVector().GetSafeNormal2D();
	const ERROreKind Kinds[] = { ERROreKind::Shard, ERROreKind::Ore, ERROreKind::Big, ERROreKind::Golden };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Kinds); ++Index)
	{
		const FTransform Spot(RandomCharacter->GetActorLocation() + Forward * (260.f + Index * 170.f));
		if (ARRRandomOre* Piece = GetWorld()->SpawnActorDeferred<ARRRandomOre>(ARRRandomOre::StaticClass(), Spot, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Piece->Setup(Kinds[Index], FVector(0.f, 0.f, 200.f));
			Piece->FinishSpawning(Spot);
		}
	}
#endif
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

bool ARRRandomPlayerController::GetFireTarget(FVector& OutTarget) const
{
	if (bThirdPerson)
	{
		OutTarget = GetCrosshairTarget();
		return true;
	}
	FVector CursorOrigin;
	FVector CursorDirection;
	if (!DeprojectMousePositionToWorld(CursorOrigin, CursorDirection))
	{
		return false;
	}
	OutTarget = GetAimTarget(CursorOrigin, CursorDirection);
	return true;
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

void ARRRandomPlayerController::UpdateSprint()
{
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	URRRandomMovementComponent* Movement = RandomCharacter ? Cast<URRRandomMovementComponent>(RandomCharacter->GetCharacterMovement()) : nullptr;
	if (!Movement || !IsLocalController())
	{
		return;
	}
	// Running wins over the fire button (no shots while running, ARRRandomCharacter::ShootAt); in the sights it walks
	Movement->bWantsToSprint = bSprintHeld && !IsGameplayInputBlocked() && !RandomCharacter->IsAimingDownSights();
}

void ARRRandomPlayerController::OnAimStart()
{
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()); RandomCharacter && bThirdPerson && !RandomCharacter->IsInRobot())
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

void ARRRandomPlayerController::OnAimHeld()
{
	// Aimed like the left button: the crosshair over the shoulder, else the cursor
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter || RandomCharacter->GetRobotSeat() != ERRRobotSeat::Attack || IsGameplayInputBlocked())
	{
		return;
	}
	if (bThirdPerson)
	{
		if (!bShowMouseCursor && IsViewportFocused())
		{
			RandomCharacter->RobotAltFireAt(GetCrosshairTarget());
		}
		return;
	}
	FVector CursorOrigin;
	FVector CursorDirection;
	if (DeprojectMousePositionToWorld(CursorOrigin, CursorDirection))
	{
		RandomCharacter->RobotAltFireAt(GetAimTarget(CursorOrigin, CursorDirection));
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

void ARRRandomPlayerController::OnDescendStart()
{
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->SetPlaneDescend(!IsGameplayInputBlocked());
	}
}

void ARRRandomPlayerController::OnDescendStop()
{
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->SetPlaneDescend(false);
	}
}

void ARRRandomPlayerController::RRCannon(int32 Index)
{
	ServerCheatCannon(Index);
}

void ARRRandomPlayerController::ServerCheatCannon_Implementation(int32 Index)
{
#if !UE_BUILD_SHIPPING
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter || !RandomCharacter->IsAlive() || RandomCharacter->IsRidingCannon())
	{
		return;
	}
	for (TActorIterator<ARRRandomCannon> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: cannon %d at %s, pad at %s."), It->GetIndex(), *It->GetActorLocation().ToString(), *It->GetPadLocation().ToString());
		if (It->GetIndex() != Index)
		{
			continue;
		}
		// Off whatever it rides, just behind the cannon (away from its pad), facing it
		RandomCharacter->LeaveVehicle();
		const FVector Back = -It->GetActorForwardVector();
		const FVector Spot = It->GetActorLocation() + Back * 200.f + FVector(0.f, 0.f, RandomCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f);
		RandomCharacter->TeleportTo(Spot, FRotator(0.f, It->GetActorRotation().Yaw, 0.f));
		ClientMessage(FString::Printf(TEXT("Cheat: cannon %d (press F)"), Index));
	}
#endif
}

void ARRRandomPlayerController::RRPlane(int32 Face)
{
	ServerCheatPlane(FMath::Clamp(Face, 1, 6));
}

void ARRRandomPlayerController::ServerCheatPlane_Implementation(int32 Face)
{
#if !UE_BUILD_SHIPPING
	ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn());
	if (!RandomCharacter)
	{
		return;
	}
	// Off whatever it rides first
	RandomCharacter->LeaveVehicle();
	if (const ARRRandomVehicle* Plane = ARRRandomPlaneStation::SpawnPlaneFor(RandomCharacter, Face, nullptr))
	{
		ClientMessage(FString::Printf(TEXT("Cheat: %s"), Plane->GetHandling().Name));
	}
#endif
}

void ARRRandomPlayerController::RRDefer(const FString& Command)
{
	const FString Line = Command.Replace(TEXT("+"), TEXT(" "));
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Line]()
	{
		ConsoleCommand(Line);
	}));
}

void ARRRandomPlayerController::RRPlaneDown(int32 bHold)
{
	if (ARRRandomCharacter* RandomCharacter = Cast<ARRRandomCharacter>(GetPawn()))
	{
		RandomCharacter->SetPlaneDescend(bHold != 0);
	}
}
