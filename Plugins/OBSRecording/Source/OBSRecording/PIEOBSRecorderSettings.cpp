#include "PIEOBSRecorderSettings.h"

#include "UI/LogPIEOBSRecorder.h"

UPIEOBSRecorderSettings::UPIEOBSRecorderSettings()
{
}

FName UPIEOBSRecorderSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

void UPIEOBSRecorderSettings::LogCurrentSettings() const
{
	UE_LOG(LogPIEOBSRecorder,Log,TEXT(	"PIE OBS Recorder settings: "	"AutoRecording=%s, OBS=%s:%d, "	"ConnectTimeout=%.1fs, PasswordConfigured=%s"	),
				bEnableAutoRecording ? TEXT("true") : TEXT("false"),
				*ServerHost,
				ServerPort,
				ConnectTimeoutSeconds,
				ResolvePassword().IsEmpty() ? TEXT("false") : TEXT("true")
		);
}

FString UPIEOBSRecorderSettings::ResolvePassword() const
{
	const FString FromEnvironment = FPlatformMisc::GetEnvironmentVariable(TEXT("OBS_WEBSOCKET_PASSWORD"));
	
	return FromEnvironment.IsEmpty()
		? Password
		: FromEnvironment;
}

// FString UPIEOBSRecorderSettings::ResolveDefaultSaveDirectory() const
// {
// 	
// }
