#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RRRandomTitlePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/** A button on the title menu. */
enum class ERRTitleChoice : uint8
{
	Single,
	Network,
	Host,
	Join,
	Back,
	Quit,
};

/**
 * Title menu: SINGLE PLAY or NETWORK (then HOST GAME / JOIN GAME), and QUIT.
 * W/S or the arrow keys pick, Enter or Space presses, Esc goes back (or cancels a host/join in progress); the mouse works too.
 * Starting a game goes through URRRandomSessionSubsystem, which opens the arena or joins a host.
 */
UCLASS()
class RRRANDOM_API ARRRandomTitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARRRandomTitlePlayerController();

	/** The buttons on the page being shown, top to bottom. */
	TArray<ERRTitleChoice> GetChoices() const;

	int32 GetSelected() const { return Selected; }
	void SetSelected(int32 Index);

	bool IsOnNetworkPage() const { return bNetworkPage; }

	/** True while a host or join is under way; the buttons wait for it. */
	bool IsBusy() const;

	void Choose(ERRTitleChoice Choice);

	static FString GetLabel(ERRTitleChoice Choice);
	static FString GetDescription(ERRTitleChoice Choice);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void CreateInputAssets();

	void OnUp();
	void OnDown();
	void OnConfirm();
	void OnBack();
	void OnClick();

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> UpAction;

	UPROPERTY()
	TObjectPtr<UInputAction> DownAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY()
	TObjectPtr<UInputAction> BackAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ClickAction;

	bool bNetworkPage = false;
	int32 Selected = 0;
};
