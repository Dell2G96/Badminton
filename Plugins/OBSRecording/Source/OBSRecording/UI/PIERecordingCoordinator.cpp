
#include "PIERecordingCoordinator.h"
#include "OBSRecording/Disposition/RecordingDispositionQueue.h"
#include "IPIERecordingBackend.h"
#include "LogPIEOBSRecorder.h"
#include "OBSRecording/PIEOBSRecorderSettings.h"


FPIERecordingCoordinator::~FPIERecordingCoordinator()
{
	UE_LOG(LogPIEOBSRecorder, Log, TEXT("Recording coordinator destroyed"));
}

void FPIERecordingCoordinator::HandlePostPIEStarted(bool bIsSimulating)
{
	const UPIEOBSRecorderSettings* Settings = GetDefault<UPIEOBSRecorderSettings>();
	
	if (!Settings->bEnableAutoRecording)
	{
		UE_LOG(LogPIEOBSRecorder, Verbose, TEXT("Automatic recording is disabled"));
		return;
	}

	
	/* Todo :
	 * 1. SIE 허용 여부 검사
	 * 2. 연결 설정 유효성 검사
	 * 3. Backend에 녹화 시작 요청하기
	 */
	
}

void FPIERecordingCoordinator::HandlePrePIEEnded(bool bIsSimulating)
{
}
