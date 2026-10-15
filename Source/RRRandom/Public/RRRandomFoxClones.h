#pragma once

#include "CoreMinimal.h"
#include "RRRandomUltimate.h"
#include "RRRandomFoxClones.generated.h"

class ARRRandomCharacter;

/**
 * The fox's ultimate, "Fox Clones" (여우 둔갑술): in a burst of smoke and leaves, as many clones as the die showed spring up
 * round the fox and scatter, the fox itself slipping into one of their places. The clones (ARRRandomCharacter::IsDecoy)
 * look like the fox down to its health and gun and fight like bots, but their shots do nothing and each vanishes in a puff
 * at the first hit. They last Duration seconds, or until the fox is knocked out or the match ends.
 */
UCLASS()
class RRRANDOM_API ARRRandomFoxClones : public ARRRandomUltimate
{
	GENERATED_BODY()

public:
	ARRRandomFoxClones();

	virtual FString GetTitle() const override { return TEXT("FOX CLONES"); }
	virtual FString GetStatusLine() const override;

	/** How long the clones last. */
	UPROPERTY(EditAnywhere, Category = "Fox Clones")
	float Duration = 12.f;

	/** How far round the fox the clones spring up, and how hard they scatter from there. */
	UPROPERTY(EditAnywhere, Category = "Fox Clones")
	float RingRadius = 170.f;
	UPROPERTY(EditAnywhere, Category = "Fox Clones")
	float ScatterSpeed = 700.f;

protected:
	virtual void OnStarted() override;
	virtual void TickUltimate(float DeltaSeconds) override;

private:
	/** Server: every clone left goes up in smoke. */
	void VanishAll();

	UPROPERTY()
	TArray<TObjectPtr<ARRRandomCharacter>> Clones;

	float Age = 0.f;
};
