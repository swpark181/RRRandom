#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;

/**
 * A walking route over the ground for a capsule, without a navigation mesh (the levels are built at run time): A* over a
 * grid of cells round the start and the goal. A cell is open when the floor under it is found and a capsule fits there
 * (nothing world static or world dynamic in the way: cover, hills, props, the hidden walls at the shore and the rim), and
 * a step between cells is allowed when the floor rises or drops no more than a step's height. The route is then
 * straightened (a corner is skipped wherever the capsule can sweep straight past it). Server side, for the dance line.
 */
struct FRRGridPath
{
	/**
	 * Fills OutPoints with the route's corners, from just after Start to Goal (capsule middles), and returns true; false
	 * when there is no way there within the searched area (it reaches Margin beyond the two ends) or the goal is shut in.
	 */
	static bool Find(const UWorld* World, const FVector& Start, const FVector& Goal, float Radius, float HalfHeight,
		const TArray<const AActor*>& Ignore, TArray<FVector>& OutPoints, float CellSize = 100.f, float Margin = 1200.f);

	/** The length along a route from Start through its points. */
	static float Length(const FVector& Start, const TArray<FVector>& Points);
};
