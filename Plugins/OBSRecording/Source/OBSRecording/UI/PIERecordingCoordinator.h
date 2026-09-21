#pragma once

#include "IPIERecordingBackend.h"
#include "Templates/SharedPointer.h" 

class FPIERecordingCoordinator
{
public:
	explicit FPIERecordingCoordinator(TSharedRef<IPIERecordingBackend> InBackend, TSharedRef<class FRecordingDispositionQueue> InDispositionQueue) 
		: Backend(InBackend)
		, DispositionQueue(InDispositionQueue)
	{}
	
	~FPIERecordingCoordinator();

public:
	void HandlePostPIEStarted(bool bIsSimulating);
	void HandlePrePIEEnded(bool bIsSimulating);
	
private:
	TSharedRef<IPIERecordingBackend> Backend;
	TSharedRef<FRecordingDispositionQueue> DispositionQueue;
};
