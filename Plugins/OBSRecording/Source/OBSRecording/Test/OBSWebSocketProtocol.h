#pragma once

#include "CoreMinimal.h"

enum class EOBSOpCode : int32
{
	Hello = 0,
	Identify = 1,
	Identified = 2,
	Reidentify = 3,
	Event = 5,
	Request = 6,
	RequestResponse = 7,
};

// Oupputs 계열만 최소 수독
enum class EOBSEventSubscription : int32
{
	None = 0,
	Outputs = 1 << 6, 
};

// 실패 사유를 구분할 수 있어야 안내 문구가 정확함
enum class EOBSIdentifyResult : uint8
{
	// 정상 메세지
	Success,
	// 인증 요구시 비밀번호가 비어있으면 계산을 시도하지 않고 실패르 끝낸다.
	PasswordRequired,
};

// 서버가 보낸 Hello(op = 0)
struct FOBSHelloMessage
{
	FString ObsWebSocketVersion;
	int32 RpcVersion = 0;
	
	// authentication 이 비어있으면 true
	// 인증이 꺼진 서버는 이 멤버 자체를 보내지 않는다.
	bool bAuthenticationRequired = false;
	FString Challenge;
	FString Salt;
};

// 요청 응답(op=7)의 내용.
struct FOBSRequestResponse
{
	FString RequestType;
	FString RequestId;
	
	// requestStatus.Result  이 값이 곧 성공 여부이며 추측할 여지가 없음
	bool  bSuccess = false;
	int32 Code = 0;
	FString Comment;
	
	// responseData 없을 수 있으므로 항상 유효성을 확인해야한다.
	TSharedPtr<FJsonObject> ResponseData;
};

// GetRecordStatus 응답 내용. 소유권 판정의 근거가 된다.
struct FOBSRecordStatus
{
	// true면 이미 녹화가 실행 중
	bool  bOutputActive = false;
	bool  bOutputPaused = false;
	int64 OutputDurationMs = 0;
};


// RecordStateChanged 이벤트 내용. 요청 응답을 보조하는 용도로만 사용.
struct FOBSRecordStateChanged
{
	bool    bOutputActive = false;
	FString OutputState;
	FString OutputPath;
};


class FOBSWebSocketProtocol
{
public:
	// 수신 문자열에서 op와 d를 꺼내. 
	// 파싱 실패나 필드 누락이면 false를 돌려주고 크래시하지 않는다.
	static bool ParseEnvelope(const FString& Message, int32& OutOpCode, TSharedPtr<FJsonObject>& OutData);

	// Hello(op=0)의 d를 해석
	// authentication 필드 유무로 인증 필요 여부가 결정
	static bool ParseHello(const TSharedPtr<FJsonObject>& Data, FOBSHelloMessage& OutHello);
	
	// obs-websocket 5.x 인증 문자열을 계산.
	// secret = Base64(SHA256(password + salt)), authentication = Base64(SHA256(secret + challenge))
	static FString ComputeAuthentication(const FString& Password, const FString&  Salt, const FString&  Challenge);
	
	// Identify(op=1) 메시지를 만든다. 
	// 인증이 꺼진 서버면 authentication 필드를 아예 넣지 않는다.
	static EOBSIdentifyResult BuildIdentify(const FOBSHelloMessage& Hello, const FString& Password, int32 EventSubscriptions, FString& OutMessage );
	
	// 요청(op=6) 메시지를 생성.
	// requestData가 필요 없는 요청만 v1에서 쓴다.
	static FString BuildRequest(const FString&  RequestType, const FString& RequestId);
	
	// 요청 응답(op=7)의 d를 해석
	static bool ParseRequestResponse(const TSharedPtr<FJsonObject>& Data, FOBSRequestResponse& OutResponse);
	
	// GetRecordStatus 응답의 responseData를 해석
	static bool ParseRecordStatus(const TSharedPtr<FJsonObject>& ResponseData, FOBSRecordStatus& OutStatus);
	
	// StopRecord 응답의 responseData에서 녹화 파일 경로를 꺼낸다
	// 이 값이 파일 처리의 유일한 근거가 됨
	static bool ParseStopRecordOutputPath(const TSharedPtr<FJsonObject>& ResponseData, FString& OutPath);
	
	// 이벤트(op=5)가 RecordStateChanged면 내용을 해석한다. 다른 이벤트면 false.
	static bool ParseRecordStateChanged(const TSharedPtr<FJsonObject>& Data, FOBSRecordStateChanged& OutEvent);
	
	// 새 requestId를 만든다. 응답 상관관계의 열쇠이므로 매번 새 값이어야 한다.
	static FString MakeRequestId();
	
};
