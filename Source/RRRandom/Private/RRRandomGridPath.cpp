#include "RRRandomGridPath.h"
#include "Algo/Reverse.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"

namespace
{
	// The floor between neighbors may rise or drop this much (a little over the character's step) and still be walked
	constexpr float MaxStep = 50.f;
	// Look for the floor this far above and below the start's height
	constexpr float FloorSearch = 600.f;
	// How far down to look for the ground under a start in the air
	constexpr float FloorDrop = 3000.f;
	// The most cells searched, so a hopeless search can't stall the frame
	constexpr int32 MaxCells = 40000;

	struct FCell
	{
		/** 0 unknown, 1 open, 2 shut. */
		uint8 State = 0;
		float FloorZ = 0.f;
	};
}

bool FRRGridPath::Find(const UWorld* World, const FVector& Start, const FVector& Goal, float Radius, float HalfHeight,
	const TArray<const AActor*>& Ignore, TArray<FVector>& OutPoints, float CellSize, float Margin)
{
	OutPoints.Reset();
	if (!World)
	{
		return false;
	}
	const FVector2D Min(FMath::Min(Start.X, Goal.X) - Margin, FMath::Min(Start.Y, Goal.Y) - Margin);
	const int32 Columns = FMath::CeilToInt((FMath::Max(Start.X, Goal.X) + Margin - Min.X) / CellSize) + 1;
	const int32 Rows = FMath::CeilToInt((FMath::Max(Start.Y, Goal.Y) + Margin - Min.Y) / CellSize) + 1;
	if (Columns * Rows > MaxCells)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomGridPath), false);
	for (const AActor* Actor : Ignore)
	{
		Params.AddIgnoredActor(Actor);
	}
	FCollisionObjectQueryParams Solid;
	Solid.AddObjectTypesToQuery(ECC_WorldStatic);
	Solid.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, HalfHeight - 4.f);
	const float FeetBelow = HalfHeight;
	// The ground under the start (it may be in the air, flying or falling), which the search is measured from
	FHitResult Ground;
	const float StartFloor = World->LineTraceSingleByChannel(Ground, Start, Start - FVector(0.f, 0.f, FloorDrop), ECC_Visibility, Params)
		? float(Ground.ImpactPoint.Z) : float(Start.Z - FeetBelow);

	TArray<FCell> Cells;
	Cells.SetNum(Columns * Rows);
	auto CellAt = [&](const FVector& Where)
	{
		return FIntPoint(FMath::Clamp(FMath::RoundToInt((Where.X - Min.X) / CellSize), 0, Columns - 1), FMath::Clamp(FMath::RoundToInt((Where.Y - Min.Y) / CellSize), 0, Rows - 1));
	};
	auto Middle = [&](const FIntPoint& Cell)
	{
		return FVector(Min.X + Cell.X * CellSize, Min.Y + Cell.Y * CellSize, Cells[Cell.Y * Columns + Cell.X].FloorZ + FeetBelow);
	};
	// Whether a capsule can stand in the cell: the floor under it (the highest walkable surface near the start's height),
	// and room above it
	auto Probe = [&](const FIntPoint& Cell) -> bool
	{
		FCell& Info = Cells[Cell.Y * Columns + Cell.X];
		if (Info.State == 0)
		{
			const FVector Top(Min.X + Cell.X * CellSize, Min.Y + Cell.Y * CellSize, StartFloor + FeetBelow + FloorSearch);
			FHitResult Floor;
			if (!World->LineTraceSingleByChannel(Floor, Top, Top - FVector(0.f, 0.f, FloorSearch * 2.f), ECC_Visibility, Params) || Floor.ImpactNormal.Z < 0.7f)
			{
				Info.State = 2;
			}
			else
			{
				Info.FloorZ = Floor.ImpactPoint.Z;
				const FVector Body(Top.X, Top.Y, Info.FloorZ + FeetBelow + 2.f);
				Info.State = World->OverlapAnyTestByObjectType(Body, FQuat::Identity, Solid, Capsule, Params) ? 2 : 1;
			}
		}
		return Info.State == 1;
	};

	const FIntPoint From = CellAt(Start);
	const FIntPoint To = CellAt(Goal);
	// The brawler is where it is (coming down to the ground under it first), whatever the grid says about that cell
	Cells[From.Y * Columns + From.X] = { 1, StartFloor };
	if (!Probe(To))
	{
		return false;
	}

	// A* with eight neighbors; the cost is the distance plus a little for climbing
	TArray<float> Cost;
	Cost.Init(UE_BIG_NUMBER, Columns * Rows);
	TArray<int32> Came;
	Came.Init(INDEX_NONE, Columns * Rows);
	TArray<TPair<float, int32>> Open;
	auto Guess = [&](const FIntPoint& Cell) { return FVector2D::Distance(FVector2D(Cell), FVector2D(To)) * CellSize; };
	const int32 FromIndex = From.Y * Columns + From.X;
	const int32 ToIndex = To.Y * Columns + To.X;
	Cost[FromIndex] = 0.f;
	Open.HeapPush(TPair<float, int32>(Guess(From), FromIndex), [](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	const FIntPoint Steps[] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1} };
	bool bFound = false;
	while (Open.Num() > 0)
	{
		TPair<float, int32> Top;
		Open.HeapPop(Top, [](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; }, EAllowShrinking::No);
		const int32 Index = Top.Value;
		if (Index == ToIndex)
		{
			bFound = true;
			break;
		}
		const FIntPoint Cell(Index % Columns, Index / Columns);
		if (Top.Key > Cost[Index] + Guess(Cell) + 1.f)
		{
			continue;
		}
		for (const FIntPoint& Step : Steps)
		{
			const FIntPoint Next = Cell + Step;
			if (Next.X < 0 || Next.Y < 0 || Next.X >= Columns || Next.Y >= Rows || !Probe(Next))
			{
				continue;
			}
			// Diagonals only past two open sides, so the capsule doesn't cut a corner of a wall
			if (Step.X != 0 && Step.Y != 0 && (!Probe(Cell + FIntPoint(Step.X, 0)) || !Probe(Cell + FIntPoint(0, Step.Y))))
			{
				continue;
			}
			const int32 NextIndex = Next.Y * Columns + Next.X;
			const float Rise = Cells[NextIndex].FloorZ - Cells[Index].FloorZ;
			if (FMath::Abs(Rise) > MaxStep)
			{
				continue;
			}
			const float NewCost = Cost[Index] + (Step.X != 0 && Step.Y != 0 ? 1.4142f : 1.f) * CellSize + FMath::Abs(Rise);
			if (NewCost < Cost[NextIndex])
			{
				Cost[NextIndex] = NewCost;
				Came[NextIndex] = Index;
				Open.HeapPush(TPair<float, int32>(NewCost + Guess(Next), NextIndex), [](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
			}
		}
	}
	if (!bFound)
	{
		return false;
	}

	// The cells back from the goal, then the route straightened: from each corner, on to the furthest one the capsule
	// can sweep to in a straight line
	TArray<FVector> Raw;
	for (int32 Index = ToIndex; Index != FromIndex && Index != INDEX_NONE; Index = Came[Index])
	{
		Raw.Add(Middle(FIntPoint(Index % Columns, Index / Columns)));
	}
	Algo::Reverse(Raw);
	if (Raw.Num() > 0)
	{
		Raw.Last() = Goal;
	}
	const FCollisionShape Thin = FCollisionShape::MakeCapsule(Radius * 0.9f, HalfHeight * 0.6f);
	FVector Here(Start.X, Start.Y, StartFloor + FeetBelow);
	int32 Next = 0;
	while (Next < Raw.Num())
	{
		int32 Far = Next;
		for (int32 Try = Raw.Num() - 1; Try > Next; --Try)
		{
			if (FMath::Abs(Raw[Try].Z - Here.Z) < MaxStep * 2.f
				&& !World->SweepTestByObjectType(Here + FVector(0.f, 0.f, 20.f), Raw[Try] + FVector(0.f, 0.f, 20.f), FQuat::Identity, Solid, Thin, Params))
			{
				Far = Try;
				break;
			}
		}
		OutPoints.Add(Raw[Far]);
		Here = Raw[Far];
		Next = Far + 1;
	}
	return OutPoints.Num() > 0;
}

float FRRGridPath::Length(const FVector& Start, const TArray<FVector>& Points)
{
	float Total = 0.f;
	FVector Here = Start;
	for (const FVector& Point : Points)
	{
		Total += FVector::Dist2D(Here, Point);
		Here = Point;
	}
	return Total;
}
