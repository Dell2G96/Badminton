// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"



class FOBSRecordingModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
private:
	void RegisterPIEDelegates();
	void UnregisterPIEDelegates();
	
private:
	FDelegateHandle PostPIEStartedHandle;
	FDelegateHandle PrePIEEndedHandle;
	FDelegateHandle ShutdownPIEHandle;
	FDelegateHandle CancelPIEHandle;
	FDelegateHandle EditorPreExitPIEHandle;

private:
	TSharedPtr<class FOBSWebSocketBackend> Backend;
	TSharedPtr<class FRecordingDispositionQueue> DispositionQueue;
	TUniquePtr<class FPIERecordingCoordinator> Coordinator;
	
	
	TSharedPtr<class IOBSProcessPlatform> ProcessPlatform;
	TUniquePtr<struct FOBSProcessController>ProcessController;
	
	TSharedPtr<bool> bIsAlive;
};
