#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace Badminton
{
	// Opt-in diagnostics only. No additional replicated state or gameplay decisions.
	inline bool NetMetricsEnabled()
	{
#if !UE_BUILD_SHIPPING
		static const bool bEnabled = FParse::Param(FCommandLine::Get(), TEXT("BadmintonNetMetrics"));
		return bEnabled;
#else
		return false;
#endif
	}
}
