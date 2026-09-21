#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace Badminton
{
	inline bool UseRoundTripClock()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		// Keep the measured candidate opt-in until stress rallies are stable.
		static const bool bEnabled = FParse::Param(FCommandLine::Get(), TEXT("BadmintonRoundTripClock"))
			&& !FParse::Param(FCommandLine::Get(), TEXT("BadmintonLegacyClock"));
		return bEnabled;
#endif
	}

	// Presentation only: assumes approximately symmetric transit/queue delays.
	// Never use this estimate to approve client shots or change authoritative time.
	struct FShuttleClock
	{
		uint32 BeginProbe(double RealNow, double GameNow)
		{
			if (LastProbeReal >= 0. && (GameNow < LastProbeGame
				|| FMath::Abs((GameNow - LastProbeGame) - (RealNow - LastProbeReal)) > .1))
			{
				ClearEstimate();
			}
			LastProbeReal = RealNow;
			LastProbeGame = GameNow;
			if (++Sequence == 0) { ++Sequence; }
			bPending = true;
			return Sequence;
		}

		bool Accept(uint32 ReplySequence, double ServerGameTime, double RealNow, double GameNow)
		{
			if (!bPending || ReplySequence != Sequence) { return false; }
			bPending = false;
			const double RTT = RealNow - LastProbeReal;
			if (!FMath::IsFinite(ServerGameTime) || ServerGameTime < 0. || !FMath::IsFinite(RTT)
				|| RTT <= 0. || RTT > 1. || !FMath::IsFinite(GameNow)
				|| FMath::Abs((GameNow - LastProbeGame) - RTT) > .1)
			{
				ClearEstimate();
				return false;
			}
			if (RealNow - LastAcceptedReal > 3. || GameNow < LastAcceptedGame) { ClearEstimate(); }
			Samples.RemoveAll([RealNow](const FSample& Sample) { return RealNow - Sample.RealTime > 8.; });
			if (Samples.Num() >= 8) { Samples.RemoveAt(0); }
			Samples.Add({RealNow, RTT, ServerGameTime + RTT * .5 - GameNow});
			const FSample* Best = &Samples[0];
			for (const FSample& Sample : Samples)
			{
				if (Sample.RTT <= Best->RTT) { Best = &Sample; }
			}
			// Limit established-clock corrections to 5ms per accepted probe.
			Offset = bValid ? Offset + FMath::Clamp(Best->Offset - Offset, -.005, .005) : Best->Offset;
			SelectedRTT = Best->RTT;
			LastAcceptedReal = RealNow;
			LastAcceptedGame = GameNow;
			bValid = true;
			return true;
		}

		bool Estimate(double RealNow, double GameNow, double& ServerNow) const
		{
			if (!bValid || !FMath::IsFinite(RealNow) || !FMath::IsFinite(GameNow)
				|| RealNow < LastAcceptedReal || RealNow - LastAcceptedReal > 3.
				|| GameNow < LastAcceptedGame
				|| FMath::Abs((GameNow - LastAcceptedGame) - (RealNow - LastAcceptedReal)) > .1) { return false; }
			ServerNow = GameNow + Offset;
			return true;
		}

		double GetSelectedRTT() const { return SelectedRTT; }

	private:
		struct FSample { double RealTime; double RTT; double Offset; };
		void ClearEstimate() { bValid = false; Samples.Reset(); SelectedRTT = -1.; }
		TArray<FSample> Samples;
		uint32 Sequence = 0;
		bool bPending = false;
		bool bValid = false;
		double LastProbeReal = -1.;
		double LastProbeGame = 0.;
		double LastAcceptedReal = -1.;
		double LastAcceptedGame = 0.;
		double Offset = 0.;
		double SelectedRTT = -1.;
	};
}
