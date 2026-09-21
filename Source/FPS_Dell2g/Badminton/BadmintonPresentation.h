#pragma once

#include "CoreMinimal.h"

namespace Badminton
{
	struct FPresentationAge
	{
		double Raw = 0.;
		double SinceReceipt = 0.;
		double Requested = 0.;
		float Seconds = 0.f;
	};

	// A received snapshot is at least as old as the game time elapsed since receipt.
	// This lower bound does not estimate transport delay or alter the server clock.
	inline FPresentationAge GetPresentationAge(double EstimatedServerNow, double SnapshotServerTime,
		double LocalNow, double LocalReceiptTime)
	{
		FPresentationAge Result;
		Result.Raw = EstimatedServerNow - SnapshotServerTime;
		Result.SinceReceipt = FMath::Max(0., LocalNow - LocalReceiptTime);
		Result.Requested = FMath::Max(Result.Raw, Result.SinceReceipt);
		Result.Seconds = static_cast<float>(FMath::Clamp(Result.Requested, 0., .15));
		return Result;
	}
}
