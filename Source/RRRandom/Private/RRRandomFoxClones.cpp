#include "RRRandomFoxClones.h"
#include "RRRandomAIController.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomGameMode.h"
#include "RRRandomGameState.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

ARRRandomFoxClones::ARRRandomFoxClones()
{
}

FString ARRRandomFoxClones::GetStatusLine() const
{
	return FString::Printf(TEXT("%s  x%d  %.0fs"), *GetTitle(), Left, FMath::Max(0.f, FMath::CeilToFloat(Duration - Age)));
}

void ARRRandomFoxClones::OnStarted()
{
	// Smoke and leaves on every machine, hiding which one is the fox
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->FoxPoof(Brawler->GetActorLocation(), 2.2f);
	}
	if (!HasAuthority())
	{
		return;
	}
	UWorld* World = GetWorld();
	const ARRRandomGameMode* GameMode = World->GetAuthGameMode<ARRRandomGameMode>();
	const float HalfHeight = Brawler->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Middle = Brawler->GetActorLocation();
	// The clones and the fox share a ring of places, the fox in a random one of them
	const int32 Places = Left + 1;
	const int32 FoxPlace = FMath::RandRange(0, Places - 1);
	const float Turn = FMath::FRandRange(0.f, 360.f);
	for (int32 Place = 0; Place < Places; ++Place)
	{
		const FVector Out = FRotator(0.f, Turn + 360.f * Place / Places, 0.f).Vector();
		// On the floor there (not on someone), the capsule's middle above it
		const FVector Spot = FindGround(Middle - FVector(0.f, 0.f, HalfHeight) + Out * RingRadius) + FVector(0.f, 0.f, HalfHeight + 2.f);
		if (Place == FoxPlace)
		{
			Brawler->TeleportTo(Spot, Brawler->GetActorRotation(), false, true);
			Brawler->LaunchCharacter(Out * ScatterSpeed * 0.6f + FVector(0.f, 0.f, 250.f), true, true);
			continue;
		}
		const FTransform Where(Brawler->GetActorRotation(), Spot);
		ARRRandomCharacter* Clone = World->SpawnActorDeferred<ARRRandomCharacter>(Brawler->GetClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Clone)
		{
			continue;
		}
		Clone->MakeDecoyOf(Brawler);
		Clone->FinishSpawning(Where);
		Clone->SpawnDefaultController();
		if (ARRRandomAIController* AI = Cast<ARRRandomAIController>(Clone->GetController()))
		{
			AI->SetArenaCenter(GameMode ? GameMode->GetArenaCenter() : Middle);
		}
		Clone->LaunchCharacter(Out * ScatterSpeed + FVector(0.f, 0.f, 250.f), true, true);
		Clones.Add(Clone);
	}
	Left = Clones.Num();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s splits into %d clones (place %d of %d)."), *Brawler->GetName(), Clones.Num(), FoxPlace, Places);
}

void ARRRandomFoxClones::TickUltimate(float DeltaSeconds)
{
	Age += DeltaSeconds;
	if (!HasAuthority())
	{
		return;
	}
	Clones.RemoveAll([](const ARRRandomCharacter* Clone) { return !IsValid(Clone) || !Clone->IsAlive(); });
	Left = Clones.Num();
	const bool bOver = Age >= Duration || !Brawler || !Brawler->IsAlive() || ARRRandomGameState::IsMatchOver(this);
	if (!IsEnding() && (bOver || Clones.Num() == 0))
	{
		VanishAll();
		Finish();
	}
}

void ARRRandomFoxClones::VanishAll()
{
	for (ARRRandomCharacter* Clone : Clones)
	{
		if (IsValid(Clone))
		{
			Clone->VanishDecoy();
		}
	}
	Clones.Reset();
	Left = 0;
}
