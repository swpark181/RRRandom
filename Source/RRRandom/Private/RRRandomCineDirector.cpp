#include "RRRandomCineDirector.h"

#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "RRRandomCineBlocks.h"
#include "RRRandomEffects.h"
#include "RRRandomWreckage.h"

ARRRandomCineDirector::ARRRandomCineDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the sequence has moved everything this frame
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ARRRandomCineDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !Sequence)
	{
		return;
	}
	// Movie Render Queue plays the sequence on a sequence actor of its own: follow whichever actor plays this sequence and
	// has moved on from its start (ours otherwise)
	ULevelSequencePlayer* Player = Sequence->GetSequencePlayer();
	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		ULevelSequencePlayer* Other = It->GetSequencePlayer();
		if (*It != Sequence && Other && It->GetSequence() == Sequence->GetSequence() && Other->GetCurrentTime().AsSeconds() > 0.0)
		{
			Player = Other;
			break;
		}
	}
	if (!Player)
	{
		return;
	}
	const float Now = static_cast<float>(Player->GetCurrentTime().AsSeconds());
	if (!Player->IsPlaying() && Now <= 0.f)
	{
		return;
	}
	const bool bStep = LastTime >= 0.f && Now >= LastTime && Now - LastTime < 1.f;
	for (const FRRCineCue& Cue : Cues)
	{
		if (Cue.Kind == ERRCineEffect::DieSpin)
		{
			if (Now >= Cue.Time && Now <= Cue.Time + Cue.Duration)
			{
				if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
				{
					const float Progress = Cue.Duration > 0.f ? (Now - Cue.Time) / Cue.Duration : 1.f;
					Effects->UltimateDieSpin(DieSpot(Cue), Cue.Size, Cue.B, Progress, DeltaSeconds);
				}
			}
			continue;
		}
		if (bStep && Cue.Time > LastTime && Cue.Time <= Now)
		{
			Fire(Cue);
		}
	}
	LastTime = Now;
}

FVector ARRRandomCineDirector::DieSpot(const FRRCineCue& Cue) const
{
	return GoldDie ? GoldDie->GetActorLocation() : Cue.A;
}

void ARRRandomCineDirector::Fire(const FRRCineCue& Cue) const
{
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	switch (Cue.Kind)
	{
	case ERRCineEffect::FoxPoof:
		if (Effects)
		{
			Effects->FoxPoof(Cue.A, Cue.Size);
		}
		break;
	case ERRCineEffect::Wreck:
		ARRRandomWreckage::Wreck(this, Cue.A, Cue.B, Cue.Size, false, 0, true);
		break;
	case ERRCineEffect::Smash:
		for (TActorIterator<ARRRandomCineBlocks> It(GetWorld()); It; ++It)
		{
			It->SmashAt(Cue.A, Cue.Size, Cue.B);
		}
		if (Effects)
		{
			Effects->AcornPop(Cue.A, 2.5f);
		}
		break;
	case ERRCineEffect::DieBurst:
		if (Effects)
		{
			Effects->UltimateDieBurst(DieSpot(Cue), Cue.Size, Cue.B);
		}
		break;
	default:
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("RRCine: cue %d at %.2fs"), static_cast<int32>(Cue.Kind), Cue.Time);
}
