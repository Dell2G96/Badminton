#include "OBSWebSocketProtocol.h"


#include "OBSRecording/OBS/OBSSha256.h"
#include "OBSRecording/OBSRecording.h"
#include "Dom/JsonObject.h"
#include "OBSRecording/UI/LogPIEOBSRecorder.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace 
{
	FString SerializeObject(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
		
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}
}

bool FOBSWebSocketProtocol::ParseEnvelope(const FString& Message, int32& OutOpCode, TSharedPtr<FJsonObject>& OutData)
{
	OutOpCode = -1;
	OutData.Reset();
	
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<>::Create	(Message);
	
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return false;
	
	double OpValue = 0.0;
	if (!Root->TryGetNumberField(TEXT("op"), OutOpCode))
		return false;
	
	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!Root->TryGetObjectField(TEXT("d"), DataObject) || DataObject == nullptr || !DataObject->IsValid())
    	return false;
	
	OutOpCode = static_cast<int32>(OpValue);
	OutData = *DataObject;
	return true;
}

// Hello의 d를 해석
// authentication 필드가 있으면 인증이 필요함
bool FOBSWebSocketProtocol::ParseHello(const TSharedPtr<FJsonObject>& Data, FOBSHelloMessage& OutHello)
{
	if (!Data.IsValid())
		return false;
	
	OutHello = FOBSHelloMessage();
	Data->TryGetStringField(TEXT("obsWebSocketVersion"), OutHello.ObsWebSocketVersion);
	
	double RpcValue = 0.0;
	if (Data->TryGetNumberField(TEXT("rpcVersion"), RpcValue))
	{
		OutHello.RpcVersion = static_cast<int32>(RpcValue);
	}
	
	// 인증이 꺼진 서버는 해당 필드 자체를 안 보냄
	const TSharedPtr<FJsonObject>* AuthObject = nullptr;
	if (Data->TryGetObjectField(TEXT("authentication"), AuthObject) 
		&& AuthObject != nullptr 
		&& AuthObject->IsValid())
	{
		const bool bHasChallenge = (*AuthObject)->TryGetStringField(TEXT("challenge"), OutHello.Challenge);
		const bool bHasSalt = (*AuthObject)->TryGetStringField(TEXT("salt"), OutHello.Salt);
		
		// 두 값이 모두 있어야 계산 가능
		// 하나라도 없으면 인증 불가
		OutHello.bAuthenticationRequired = bHasChallenge && bHasSalt;
		if (!OutHello.bAuthenticationRequired)
		{
			UE_LOG(LogPIEOBSRecorder, Warning, TEXT("Hello 메시지에서 인증 필드가 불완전합니다. (challenge=%s, salt=%s)"),
				*OutHello.Challenge, *OutHello.Salt);
			
			OutHello.Challenge.Reset();
			OutHello.Salt.Reset();
		}
	}
		
	return true;
		
}

//obs-websocket
FString FOBSWebSocketProtocol::ComputeAuthentication(const FString& Password, const FString& Salt,
	const FString& Challenge)
{
	const FString Secret = OBSSha256::HashUtf8ToBase64(Password + Salt);
	return OBSSha256::HashUtf8ToBase64(Secret + Challenge);
}

// Identify 메세지를 합침
// 인증이 필요 , 비밀번호가 없으면 계산하지 않고 바로 종료
EOBSIdentifyResult FOBSWebSocketProtocol::BuildIdentify(const FOBSHelloMessage& Hello, const FString& Password,
	int32 EventSubscriptions, FString& OutMessage)
{
	OutMessage.Reset();
	
	if (Hello.bAuthenticationRequired && Password.IsEmpty())
		return EOBSIdentifyResult::PasswordRequired;
	
	const TSharedRef<FJsonObject> DataObject = MakeShared<FJsonObject>();
	DataObject->SetNumberField(TEXT("rpcVersion"), Hello.RpcVersion > 0 
		? Hello.RpcVersion
		: 1);
	
	if (Hello.bAuthenticationRequired)
		DataObject->SetStringField(TEXT("authentication"),
			ComputeAuthentication(Password, Hello.Salt, Hello.Challenge));
	
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("op"), static_cast<int32>(EOBSOpCode::Identify));
	Root->SetObjectField(TEXT("d"), DataObject);
	
	OutMessage = SerializeObject(Root);
	return EOBSIdentifyResult::Success;
	
}

// 요청 메세지 조합
FString FOBSWebSocketProtocol::BuildRequest(const FString& RequestType, const FString& RequestId)
{
	const TSharedRef<FJsonObject> DataObject = MakeShared<FJsonObject>();
	DataObject->SetStringField(TEXT("requestType"), RequestType);
	DataObject->SetStringField(TEXT("requestId"), RequestId);
	
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("op"), static_cast<int32>(EOBSOpCode::Request));
	Root->SetObjectField(TEXT("d"), DataObject);
	
	return SerializeObject(Root);
}

// 요청응답 해석
bool FOBSWebSocketProtocol::ParseRequestResponse(const TSharedPtr<FJsonObject>& Data, FOBSRequestResponse& OutResponse)
{
	if (!Data.IsValid())
		return false;
	
	OutResponse = FOBSRequestResponse();
	Data->TryGetStringField(TEXT("requestType"), OutResponse.RequestType);
	
	// requestId가 없으면 어느 요청의 응답인지 알 수 없어 처리 불가
	if (!Data->TryGetStringField(TEXT("requestId"), OutResponse.RequestId) || OutResponse.RequestId.IsEmpty())
	{
		UE_LOG(LogPIEOBSRecorder, Warning, TEXT("요청 응답에 requestId가 없습니다. 무시합니다."));
		return false;
	}
	
	const TSharedPtr<FJsonObject>* StatusObject = nullptr;
	if (!Data->TryGetObjectField(TEXT("requestStatus"), StatusObject) || StatusObject == nullptr || !StatusObject->IsValid())
	{
		UE_LOG(LogPIEOBSRecorder, Warning, TEXT("요청 응답에 requestStatus가 없습니다. 실패로 처리합니다. (requestId=%s)"), *OutResponse.RequestId);
		return true;
	}
	
	(*StatusObject)->TryGetBoolField(TEXT("result"), OutResponse.bSuccess);
	(*StatusObject)->TryGetStringField(TEXT("comment"), OutResponse.Comment);
	
	double CodeValue = 0.0;
	if ((*StatusObject)->TryGetNumberField(TEXT("code"), CodeValue))
		OutResponse.Code = static_cast<int32>(CodeValue);
	
	// responseData는 없는 경우가 없음
	const TSharedPtr<FJsonObject>* ResponseDataObject = nullptr;
	if (Data->TryGetObjectField(TEXT("responseData"), ResponseDataObject) && ResponseDataObject != nullptr)
		OutResponse.ResponseData = *ResponseDataObject;
	
	return true;
	
	
}

// GetRecordStatus 응답 
bool FOBSWebSocketProtocol::ParseRecordStatus(const TSharedPtr<FJsonObject>& ResponseData, FOBSRecordStatus& OutStatus)
{
	if (!ResponseData.IsValid())
		return false;
	
	OutStatus = FOBSRecordStatus();
	
	// outputActive가 없으면 상태를 알 수 없음므로 실패로 종료
	if (!ResponseData->TryGetBoolField(TEXT("outputActive"), OutStatus.bOutputActive))
	{
		UE_LOG(LogPIEOBSRecorder, Warning, TEXT("GetRecordStatus 응답에 outputActive가 없습니다."));
		return false;
	}
	
	ResponseData->TryGetBoolField(TEXT("outputPaused"), OutStatus.bOutputPaused);
	
	double DurationValue = 0.0;
	if (ResponseData->TryGetNumberField(TEXT("outputDuration"), DurationValue))
		OutStatus.OutputDurationMs = static_cast<int64>(DurationValue);
	
	return true;
}

// StopRecord 응답에서 파일 경로를 가져옴
bool FOBSWebSocketProtocol::ParseStopRecordOutputPath(const TSharedPtr<FJsonObject>& ResponseData, FString& OutPath)
{
	OutPath.Reset();
	
	if (!ResponseData.IsValid())
		return false;
	
	return ResponseData->TryGetStringField(TEXT("outputPath"), OutPath) && !OutPath.IsEmpty();
}

// 이벤트가 RecordStateChanged인지 확인하고 내용 해석
bool FOBSWebSocketProtocol::ParseRecordStateChanged(const TSharedPtr<FJsonObject>& Data,
	FOBSRecordStateChanged& OutEvent)
{
	if (!Data.IsValid())
		return false;
	
	FString EventType;
	if (!Data->TryGetStringField(TEXT("eventType"), EventType) || EventType != TEXT("RecordStateChanged"))
		return false;
	
	const TSharedPtr<FJsonObject>* EventDataObject = nullptr;
	if (!Data->TryGetObjectField(TEXT("eventData"), EventDataObject) || EventDataObject == nullptr || !EventDataObject->IsValid())
		return false;
	
	OutEvent = FOBSRecordStateChanged();
	(*EventDataObject)->TryGetBoolField(TEXT("outputActive"), OutEvent.bOutputActive);
	(*EventDataObject)->TryGetStringField(TEXT("outputState"), OutEvent.OutputState);
	(*EventDataObject)->TryGetStringField(TEXT("outputPath"), OutEvent.OutputPath);
	
	return true;
}

// 요청마다 새 requestId를 만듦
FString FOBSWebSocketProtocol::MakeRequestId()
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
}

