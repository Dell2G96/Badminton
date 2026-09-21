#include "BadmintonOnlineSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"

namespace
{
	const FName ProtocolKey(TEXT("BADMINTON_PROTOCOL"));
	const FName RoomKey(TEXT("BADMINTON_ROOM"));
	const FName CourtMap(TEXT("/Game/Badminton/Maps/L_Badminton_Prototype"));
	constexpr int32 ProtocolVersion = 1;
}

void UBadmintonOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Keep disconnect feedback alive across map travel, including direct-IP games.
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::OnNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::OnTravelFailure);
	}
	bEnabled = FParse::Param(FCommandLine::Get(), TEXT("BadmintonEOS"));
	if (!bEnabled) { return; }
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS || OSS->GetSubsystemName() != FName(TEXT("EOS")))
	{
		SetStatus(TEXT("온라인 기능을 사용할 수 없습니다. Start-BadmintonEOS.cmd로 실행하세요."));
		return;
	}
	Identity = OSS->GetIdentityInterface();
	Sessions = OSS->GetSessionInterface();
	SetStatus(TEXT("F1: 에픽 계정 로그인. 브라우저에서 동의가 필요합니다."));
}

void UBadmintonOnlineSubsystem::SetStatus(const FString& Message)
{
	Status = Message;
	UE_LOG(LogTemp, Display, TEXT("BADMINTON_EOS %s"), *Message);
}

void UBadmintonOnlineSubsystem::ClearDelegates()
{
	if (Identity.IsValid()) { Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle); }
	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
}

void UBadmintonOnlineSubsystem::Deinitialize()
{
	ClearDelegates();
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	Search.Reset();
	Rooms.Reset();
	Sessions.Reset();
	Identity.Reset();
	Super::Deinitialize();
}

bool UBadmintonOnlineSubsystem::RequireLogin()
{
	if (bBusy) { SetStatus(TEXT("온라인 작업을 처리 중입니다. 잠시 기다리세요.")); return false; }
	if (!Identity.IsValid() || !Sessions.IsValid()) { SetStatus(TEXT("온라인 기능을 사용할 수 없습니다. Start-BadmintonEOS.cmd로 실행하세요.")); return false; }
	if (Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn) { SetStatus(TEXT("먼저 F1을 눌러 로그인하세요.")); return false; }
	return true;
}

void UBadmintonOnlineSubsystem::Login()
{
	if (!bEnabled || bBusy) { return; }
	if (!Identity.IsValid()) { SetStatus(TEXT("온라인 연결 설정이 없거나 올바르지 않습니다.")); return; }
	if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn) { SetStatus(TEXT("로그인됨. F2: 방 만들기, F3: 방 검색.")); return; }
	bBusy = true;
	SetStatus(TEXT("브라우저에서 에픽 로그인을 완료하거나 취소하세요."));
	LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::OnLogin));
	if (!Identity->Login(0, FOnlineAccountCredentials(TEXT("accountportal"), TEXT(""), TEXT(""))))
	{
		Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
		bBusy = false;
		SetStatus(TEXT("로그인을 시작하지 못했습니다. 온라인 설정과 계정 권한을 확인하세요."));
	}
}

void UBadmintonOnlineSubsystem::OnLogin(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error)
{
	Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
	bBusy = false;
	// Avoid displaying raw service errors, which may contain account or token data.
	if (!bSuccess && Error.Contains(TEXT("EOS_Auth_WrongClient")))
	{
		SetStatus(TEXT("온라인 클라이언트가 일치하지 않습니다. 저장된 인증 설정과 연결된 클라이언트를 확인하세요."));
		return;
	}
	SetStatus(bSuccess ? TEXT("로그인 성공. F2: 방 만들기, F3: 방 검색.") : TEXT("로그인 실패 또는 취소. 계정 권한을 확인한 뒤 F1로 재시도하세요."));
}

void UBadmintonOnlineSubsystem::Host(const FString& RoomName)
{
	if (!RequireLogin()) { return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("먼저 F5를 눌러 현재 방에서 나가세요.")); return; }
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = 2;
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = false;
	Settings.bUsesPresence = false;
	Settings.bAllowJoinViaPresence = false;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.BuildUniqueId = ProtocolVersion;
	FString Label = RoomName.TrimStartAndEnd().Left(40);
	Label.ReplaceInline(TEXT("\r"), TEXT(" "));
	Label.ReplaceInline(TEXT("\n"), TEXT(" "));
	if (Label.IsEmpty()) { Label = TEXT("배드민턴 경기방"); }
	Settings.Set(RoomKey, Label, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ProtocolKey, ProtocolVersion, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(SETTING_MAPNAME, CourtMap.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	bBusy = true;
	SetStatus(TEXT("2인 경기방을 만드는 중..."));
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreate));
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		bBusy = false;
		SetStatus(TEXT("방 만들기를 시작하지 못했습니다."));
	}
}

void UBadmintonOnlineSubsystem::OnCreate(FName Name, bool bSuccess)
{
	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	bBusy = false;
	SetStatus(bSuccess ? TEXT("방을 만들었습니다. 상대를 기다리는 중입니다.") : TEXT("방 만들기 실패. F2로 재시도하세요."));
	if (bSuccess) { UGameplayStatics::OpenLevel(GetGameInstance(), CourtMap, true, TEXT("listen")); }
}

void UBadmintonOnlineSubsystem::FindRooms()
{
	if (!RequireLogin()) { return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("F5로 현재 방에서 나간 뒤 검색하세요.")); return; }
	Rooms.Reset();
	RoomLabels.Reset();
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = false;
	Search->MaxSearchResults = 20;
	Search->TimeoutInSeconds = 30.f;
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(ProtocolKey, ProtocolVersion, EOnlineComparisonOp::Equals);
	bBusy = true;
	SetStatus(TEXT("경기방을 검색하는 중..."));
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFind));
	if (!Sessions->FindSessions(0, Search.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		bBusy = false;
		SetStatus(TEXT("검색을 시작하지 못했습니다."));
	}
}

void UBadmintonOnlineSubsystem::OnFind(bool bSuccess)
{
	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	bBusy = false;
	if (!bSuccess || !Search.IsValid()) { SetStatus(TEXT("검색 실패. F3으로 재시도하세요.")); return; }
	for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
	{
		int32 Version = 0;
		FString Label;
		if (!Result.IsValid() || !Result.Session.SessionSettings.Get(ProtocolKey, Version) || Version != ProtocolVersion
			|| Result.Session.SessionSettings.NumPublicConnections != 2 || Result.Session.NumOpenPublicConnections <= 0) { continue; }
		Result.Session.SessionSettings.Get(RoomKey, Label);
		Label = Label.Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")).Left(40);
		RoomLabels.Add(FString::Printf(TEXT("%d: %s [%d/2]"), Rooms.Num(), *Label, 2 - Result.Session.NumOpenPublicConnections));
		Rooms.Add(Result);
	}
	SetStatus(FString::Printf(TEXT("경기방 %d개 검색됨. F4: 첫 번째 방 참가, F3: 새로고침."), Rooms.Num()));
}

void UBadmintonOnlineSubsystem::JoinRoom(int32 Index)
{
	if (!RequireLogin()) { return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("먼저 F5를 눌러 현재 방에서 나가세요.")); return; }
	if (!Rooms.IsValidIndex(Index)) { SetStatus(TEXT("해당 방이 없습니다. 먼저 F3으로 검색하세요.")); return; }
	bBusy = true;
	SetStatus(TEXT("경기방에 참가하는 중..."));
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoin));
	if (!Sessions->JoinSession(0, NAME_GameSession, Rooms[Index]))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		bBusy = false;
		SetStatus(TEXT("참가를 시작하지 못했습니다. F3으로 다시 검색하세요."));
	}
}

void UBadmintonOnlineSubsystem::OnJoin(FName Name, EOnJoinSessionCompleteResult::Type Result)
{
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	bBusy = false;
	FString Address;
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (Result == EOnJoinSessionCompleteResult::Success && PC && Sessions->GetResolvedConnectString(NAME_GameSession, Address) && !Address.IsEmpty())
	{
		SetStatus(TEXT("대기실에 참가했습니다. 코트에 연결하는 중..."));
		PC->ClientTravel(Address, TRAVEL_Absolute);
		return;
	}
	SetStatus(TEXT("참가 실패: 방이 가득 찼거나 사용할 수 없거나 연결할 수 없습니다."));
	RecoverFromTravelFailure();
}

void UBadmintonOnlineSubsystem::Leave()
{
	if (bBusy) { SetStatus(TEXT("진행 중인 작업이 끝난 뒤 방에서 나가세요.")); return; }
	if (!Sessions.IsValid()) { return; }
	if (!Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("참가 중인 온라인 방이 없습니다.")); return; }
	bBusy = true;
	SetStatus(TEXT("경기방에서 나가는 중..."));
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroy));
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		bBusy = false;
		bRecovering = false;
		SetStatus(TEXT("방에서 나가지 못했습니다. F5로 재시도하세요."));
	}
}

void UBadmintonOnlineSubsystem::OnDestroy(FName Name, bool bSuccess)
{
	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	bBusy = false;
	bRecovering = false;
	Rooms.Reset();
	RoomLabels.Reset();
	SetStatus(bSuccess ? TEXT("방에서 나왔습니다. F2: 방 만들기, F3: 방 검색.") : TEXT("방 정리에 실패했습니다. F5로 재시도하세요."));
	if (bSuccess) { UGameplayStatics::OpenLevel(GetGameInstance(), CourtMap); }
}

void UBadmintonOnlineSubsystem::RecoverFromTravelFailure()
{
	if (bRecovering || bBusy || !Sessions.IsValid()) { return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { bRecovering = true; Leave(); }
}

void UBadmintonOnlineSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		ConnectionNotice = TEXT("연결이 끊겼거나 거부되었습니다. 다시 접속하거나 새 방을 만드세요.");
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_CONNECTION_NOTICE connection lost or rejected"));
		if (!bEnabled) { return; }
		SetStatus(TEXT("연결이 끊겼거나 거부되었습니다. 방을 정리하는 중입니다."));
		RecoverFromTravelFailure();
	}
}

void UBadmintonOnlineSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		ConnectionNotice = TEXT("코트를 열지 못했습니다. 주소를 확인한 뒤 재시도하세요.");
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_CONNECTION_NOTICE travel failed"));
		if (!bEnabled) { return; }
		SetStatus(TEXT("코트를 열지 못했습니다. 방을 정리하는 중입니다."));
		RecoverFromTravelFailure();
	}
}
