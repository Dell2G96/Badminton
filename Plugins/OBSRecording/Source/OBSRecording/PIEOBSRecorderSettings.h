#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"

#include "PIEOBSRecorderSettings.generated.h"


// OBS 플러그인 설정
UCLASS(config = EditorPerProjectUserSettings, DefaultConfig, meta = (DisplayName = "PIE OBS Recorder"))
class OBSRECORDING_API UPIEOBSRecorderSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPIEOBSRecorderSettings();
	
	virtual FName GetCategoryName() const override ;
	
	void LogCurrentSettings() const;
	FString ResolvePassword() const;
	
	// FString ResolveDefaultSaveDirectory() const;
	
	UPROPERTY(config, EditAnywhere, Category="동작")
	bool bEnableAutoRecording = false;
	
	UPROPERTY(config, EditAnywhere, Category="연결")
	FString ServerHost = TEXT("127.0.0.1");
	
	UPROPERTY(config, EditAnywhere, Category="연결", meta=(ClampMin="1", ClampMax="65535"))
	int32 ServerPort = 4455;
	
	UPROPERTY(config, EditAnywhere, Category="연결", meta=(PasswordField="true"))
	FString Password;
	
	UPROPERTY(config, EditAnywhere, Category="연결", meta=(ClampMin="0.1", UIMin="0.1"))
	float ConnectTimeoutSeconds = 3.f;
	
};
