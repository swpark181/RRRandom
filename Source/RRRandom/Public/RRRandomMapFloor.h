#pragma once

#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;

namespace RRMapFloor
{
	/**
	 * The level's floor under a map actor (Dice Town, Dice Land, the studio), which the map stands on and hides: a trace
	 * down from SearchUp over it to SearchDown under it, or, on a level just loaded in the editor (no physics yet to trace
	 * against: OnConstruction's preview), the highest top among the level's other solid things under it in that span, by
	 * their bounds. Null when there is none; OutZ is the floor's top.
	 */
	UPrimitiveComponent* Find(const AActor* MapActor, float SearchUp, float SearchDown, float& OutZ);
}
