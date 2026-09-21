// Copyright Epic Games, Inc. All Rights Reserved.

#include "OBSRecording.h"

#include "OBSWebSocketBackend.h"
#include "PIEOBSRecorderSettings.h"
#include "Disposition/RecordingDispositionQueue.h"
#include "UI/LogPIEOBSRecorder.h"
#include "UI/PIERecordingCoordinator.h"

DEFINE_LOG_CATEGORY(LogPIEOBSRecorder);

void FOBSRecordingModule::StartupModule()
{
	UE_LOG(LogPIEOBSRecorder, Log, TEXT("PIEOBSRecorder Moudle Started"));
	
	const UPIEOBSRecorderSettings* Settings = GetDefault<UPIEOBSRecorderSettings>();
	Settings->LogCurrentSettings();
	
	bIsAlive = MakeShared<bool>(true);
	DispositionQueue = MakeShared<FRecordingDispositionQueue>();
	Backend = MakeShared<FOBSWebSocketBackend>();
	
	Coordinator = MakeUnique<FPIERecordingCoordinator>(Backend.ToSharedRef(), DispositionQueue.ToSharedRef());
	
	RegisterPIEDelegates();
}

void FOBSRecordingModule::ShutdownModule()
{
	
	if (bIsAlive.IsValid())
		*bIsAlive = false;
	
	// 델리게이트 해제
	UnregisterPIEDelegates();
	
	Coordinator.Reset();
	Backend.Reset();
	DispositionQueue.Reset();
	bIsAlive.Reset();
	
	UE_LOG(LogPIEOBSRecorder, Log, TEXT("PIEOBSRecorder Moudle Stopped"));
	
}

void FOBSRecordingModule::RegisterPIEDelegates()
{
	PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddLambda([this](bool bIsSimulating)
	{
		if (Coordinator)
		{
			Coordinator->HandlePostPIEStarted(bIsSimulating);
		}
	});
	
	PrePIEEndedHandle = FEditorDelegates::PrePIEEnded.AddLambda(
	  [this](bool bIsSimulating)
	  {
		  if (Coordinator)
		  {
			  Coordinator->HandlePrePIEEnded(bIsSimulating);
		  }
	  });
}

void FOBSRecordingModule::UnregisterPIEDelegates()
{
	if (PostPIEStartedHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
		PostPIEStartedHandle.Reset();
	}
	if (PrePIEEndedHandle.IsValid())
	{
		FEditorDelegates::PrePIEEnded.Remove(PrePIEEndedHandle);
		PrePIEEndedHandle.Reset();
		
	}
}



IMPLEMENT_MODULE(FOBSRecordingModule, OBSRecording)
