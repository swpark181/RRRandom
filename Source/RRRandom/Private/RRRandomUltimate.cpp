#include "RRRandomUltimate.h"
#include "RRRandomCharacter.h"
#include "RRRandomGameState.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ARRRandomUltimate::ARRRandomUltimate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Overview = CreateDefaultSubobject<UCameraComponent>(TEXT("Overview"));
	Overview->SetupAttachment(Root);
	Overview->SetUsingAbsoluteLocation(true);
	Overview->SetUsingAbsoluteRotation(true);
	Overview->SetFieldOfView(70.f);

	auto MakeHolo = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Holo = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Holo->SetupAttachment(Root);
		Holo->SetUsingAbsoluteLocation(true);
		Holo->SetUsingAbsoluteRotation(true);
		Holo->SetUsingAbsoluteScale(true);
		Holo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Holo->SetCastShadow(false);
		Holo->SetVisibility(false);
		return Holo;
	};
	CursorHolo = MakeHolo(TEXT("CursorHolo"));
	TargetHolo = MakeHolo(TEXT("TargetHolo"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		CursorHolo->SetStaticMesh(Cylinder.Object);
		TargetHolo->SetStaticMesh(Cylinder.Object);
	}
}

void ARRRandomUltimate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomUltimate, Brawler);
	DOREPLIFETIME(ARRRandomUltimate, Left);
	DOREPLIFETIME(ARRRandomUltimate, Face);
	DOREPLIFETIME(ARRRandomUltimate, bEnding);
}

void ARRRandomUltimate::Setup(ARRRandomCharacter* InBrawler, int32 InCount)
{
	Brawler = InBrawler;
	Face = FMath::Max(1, InCount);
	Left = Face;
}

void ARRRandomUltimate::BeginPlay()
{
	Super::BeginPlay();
	TryStart();
}

void ARRRandomUltimate::TryStart()
{
	if (bStarted || !Brawler)
	{
		return;
	}
	bStarted = true;
	StartFeet = Brawler->GetActorLocation() - FVector(0.f, 0.f, Brawler->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	GroundZ = StartFeet.Z;

	// The holograms (char-pipeline/scripts/ue_make_holo.py)
	if (UMaterialInterface* Holo = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/DiceFX/M_FXHolo.M_FXHolo")))
	{
		CursorHoloMaterial = UMaterialInstanceDynamic::Create(Holo, this);
		CursorHoloMaterial->SetVectorParameterValue(TEXT("Color"), CursorHoloColor);
		TargetHoloMaterial = UMaterialInstanceDynamic::Create(Holo, this);
		TargetHoloMaterial->SetVectorParameterValue(TEXT("Color"), TargetHoloColor);
		TargetHoloMaterial->SetScalarParameterValue(TEXT("Glow"), 2.6f);
		for (int32 Slot = 0; Slot < TargetHolo->GetNumMaterials(); ++Slot)
		{
			TargetHolo->SetMaterial(Slot, TargetHoloMaterial);
			CursorHolo->SetMaterial(Slot, CursorHoloMaterial);
		}
	}

	// The picking view: high over the arena, centered on everyone, turned the way its player looked (third person) or
	// screen up (top view)
	const float ViewYaw = Brawler->IsThirdPerson() && Brawler->GetController() ? Brawler->GetControlRotation().Yaw : 0.f;
	FVector Middle = FVector::ZeroVector;
	int32 Count = 0;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDecoy())
		{
			Middle += It->GetActorLocation();
			++Count;
		}
	}
	Middle = Count > 0 ? Middle / Count : StartFeet;
	Middle.Z = StartFeet.Z;
	const FRotator ViewTurn(-60.f, ViewYaw, 0.f);
	Overview->SetWorldLocationAndRotation(Middle - ViewTurn.Vector() * 7000.f, ViewTurn);

	OnStarted();
}

FString ARRRandomUltimate::GetStatusLine() const
{
	return WantsSpots() && IsReady()
		? FString::Printf(TEXT("%s  [CLICK ANYWHERE]  x%d LEFT"), *GetTitle(), Left)
		: FString::Printf(TEXT("%s  x%d LEFT"), *GetTitle(), Left);
}

bool ARRRandomUltimate::RequestSpot(const FVector& Target)
{
	if (!HasAuthority() || !WantsSpots() || Left <= 0 || bEnding)
	{
		return false;
	}
	// Busy (or still getting ready): the spot waits for the next turn, the latest click counting
	if (!IsReady())
	{
		QueuedTarget = Target;
		bHasQueued = true;
		return true;
	}
	bHasQueued = false;
	ReadyTime = 0.f;
	StartAction(Target);
	return true;
}

void ARRRandomUltimate::Finish()
{
	if (HasAuthority() && !bEnding)
	{
		bEnding = true;
		OnRep_Ending();
	}
}

void ARRRandomUltimate::OnRep_Ending()
{
	if (bEnding && bStarted)
	{
		CursorHolo->SetVisibility(false);
		OnEnding();
	}
}

void ARRRandomUltimate::Done()
{
	if (bDone)
	{
		return;
	}
	bDone = true;
	CursorHolo->SetVisibility(false);
	if (HasAuthority())
	{
		if (Brawler)
		{
			Brawler->EndUltimate(this);
		}
		SetLifeSpan(1.f);
	}
}

void ARRRandomUltimate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TryStart();
	if (!bStarted || bDone)
	{
		return;
	}
	// A joiner learns of the ending after it started here
	if (bEnding && !HasAuthority() && CursorHolo->IsVisible())
	{
		CursorHolo->SetVisibility(false);
	}
	TickUltimate(DeltaSeconds);

	if (HasAuthority() && !bEnding)
	{
		// The brawler gone or knocked out, or the match over: wind down
		if (!Brawler || !Brawler->IsAlive() || ARRRandomGameState::IsMatchOver(this))
		{
			Left = 0;
		}
		if (WantsSpots() && !IsBusy())
		{
			if (Left <= 0)
			{
				Finish();
			}
			else if (bHasQueued)
			{
				RequestSpot(QueuedTarget);
			}
			else
			{
				ReadyTime += DeltaSeconds;
				const bool bPlayer = Brawler && Brawler->IsPlayerControlled();
				if (ReadyTime >= (bPlayer ? PlayerDelay : BotDelay))
				{
					RequestSpot(bPlayer ? GetDefaultSpot() : PickTarget());
				}
			}
		}
	}

	// The warning: pulsing until released, then fading quickly
	if (TargetHoloTime >= 0.f && TargetHoloMaterial)
	{
		TargetHoloTime += DeltaSeconds;
		float Opacity = (0.75f + 0.25f * FMath::Sin(TargetHoloTime * 14.f)) * TargetHoloStrength;
		if (TargetHoloFade >= 0.f)
		{
			TargetHoloFade += DeltaSeconds;
			Opacity *= FMath::Max(0.f, 1.f - TargetHoloFade / 0.35f);
			if (TargetHoloFade >= 0.35f)
			{
				TargetHolo->SetVisibility(false);
				TargetHoloTime = -1.f;
			}
		}
		TargetHoloMaterial->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}
}

void ARRRandomUltimate::ShowTargetHolo(const FVector& Spot, float Radius, float Strength)
{
	TargetHoloStrength = Strength;
	PlaceHolo(TargetHolo, Spot, Radius);
	TargetHolo->SetVisibility(true);
	TargetHoloTime = 0.f;
	TargetHoloFade = -1.f;
}

void ARRRandomUltimate::ReleaseTargetHolo()
{
	if (TargetHoloTime >= 0.f && TargetHoloFade < 0.f)
	{
		TargetHoloFade = 0.f;
	}
}

FVector ARRRandomUltimate::GetDefaultSpot() const
{
	const float Yaw = Brawler ? Brawler->GetActorRotation().Yaw : 0.f;
	return FindGround(StartFeet + FRotator(0.f, Yaw, 0.f).Vector() * AreaRadius * 2.f);
}

FVector ARRRandomUltimate::PickTarget() const
{
	// Every enemy is a candidate middle: the one with the most enemies within reach of a hit there
	TArray<FVector> Enemies;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && Brawler && It->GetTeam() != Brawler->GetTeam())
		{
			Enemies.Add(It->GetActorLocation() - FVector(0.f, 0.f, It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		}
	}
	FVector Best = GetDefaultSpot();
	int32 BestCount = 0;
	for (const FVector& Middle : Enemies)
	{
		int32 Count = 0;
		FVector Sum = FVector::ZeroVector;
		for (const FVector& Other : Enemies)
		{
			if (FVector::Dist2D(Middle, Other) < AreaRadius * 1.6f)
			{
				++Count;
				Sum += Other;
			}
		}
		if (Count > BestCount)
		{
			BestCount = Count;
			Best = Sum / Count;
		}
	}
	return Best;
}

TArray<ARRRandomCharacter*> ARRRandomUltimate::FindEnemiesNear(const FVector& Spot, float Radius, float Height) const
{
	TArray<ARRRandomCharacter*> Found;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Victim = *It;
		if (!Brawler || !Victim->IsAlive() || Victim->GetTeam() == Brawler->GetTeam())
		{
			continue;
		}
		// A column from a little below the spot up to Height: on the floor, flying over it, or standing on something
		const FVector Where = Victim->GetActorLocation();
		const float Reach = Radius + Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (FVector::Dist2D(Where, Spot) <= Reach && Where.Z >= Spot.Z - Radius && Where.Z <= Spot.Z + Height)
		{
			Found.Add(Victim);
		}
	}
	return Found;
}

FVector ARRRandomUltimate::FindGround(const FVector& Spot) const
{
	// Down onto the world's ground from a little above (not onto anyone, nor the top of something much taller)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UltimateFloor), false, this);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	FCollisionObjectQueryParams Ground;
	Ground.AddObjectTypesToQuery(ECC_WorldStatic);
	const FVector From(Spot.X, Spot.Y, Spot.Z + 250.f);
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByObjectType(Hits, From, From - FVector(0.f, 0.f, 2500.f), Ground, Params);
	for (const FHitResult& Hit : Hits)
	{
		if (Hit.ImpactNormal.Z > 0.6f)
		{
			return Hit.ImpactPoint;
		}
	}
	return Spot;
}

void ARRRandomUltimate::PlaceHolo(UStaticMeshComponent* Holo, const FVector& Spot, float Radius) const
{
	// The cylinder is 100 across and 100 tall about its middle
	Holo->SetWorldLocationAndRotation(Spot + FVector(0.f, 0.f, HoloHeight * 0.5f + 3.f), FRotator::ZeroRotator);
	Holo->SetWorldScale3D(FVector(Radius / 50.f, Radius / 50.f, HoloHeight / 100.f));
}

void ARRRandomUltimate::ShowCursorSpot(bool bShow, const FVector& Spot)
{
	const bool bVisible = bShow && IsPicking() && !bDone;
	if (bVisible)
	{
		PlaceHolo(CursorHolo, Spot, AreaRadius);
		if (CursorHoloMaterial)
		{
			CursorHoloMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.8f + 0.2f * FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f));
		}
	}
	if (CursorHolo->IsVisible() != bVisible)
	{
		CursorHolo->SetVisibility(bVisible);
	}
}
