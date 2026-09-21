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
		SetStatus(TEXT("EOS unavailable. Use Start-BadmintonEOS.cmd."));
		return;
	}
	Identity = OSS->GetIdentityInterface();
	Sessions = OSS->GetSessionInterface();
	SetStatus(TEXT("F1: sign in to Epic. Browser consent is required."));
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
	if (bBusy) { SetStatus(TEXT("Please wait for the current online operation.")); return false; }
	if (!Identity.IsValid() || !Sessions.IsValid()) { SetStatus(TEXT("EOS unavailable. Use Start-BadmintonEOS.cmd.")); return false; }
	if (Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn) { SetStatus(TEXT("Sign in with F1 first.")); return false; }
	return true;
}

void UBadmintonOnlineSubsystem::Login()
{
	if (!bEnabled || bBusy) { return; }
	if (!Identity.IsValid()) { SetStatus(TEXT("EOS configuration is missing or invalid.")); return; }
	if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn) { SetStatus(TEXT("Signed in. F2: create room, F3: search.")); return; }
	bBusy = true;
	SetStatus(TEXT("Complete Epic login in the browser (or cancel there)."));
	LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::OnLogin));
	if (!Identity->Login(0, FOnlineAccountCredentials(TEXT("accountportal"), TEXT(""), TEXT(""))))
	{
		Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
		bBusy = false;
		SetStatus(TEXT("Could not start login. Check EOS settings and account access."));
	}
}

void UBadmintonOnlineSubsystem::OnLogin(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error)
{
	Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
	bBusy = false;
	// Avoid displaying raw service errors, which may contain account or token data.
	if (!bSuccess && Error.Contains(TEXT("EOS_Auth_WrongClient")))
	{
		SetStatus(TEXT("EOS client mismatch. Check the saved client ID/secret and EAS linked client."));
		return;
	}
	SetStatus(bSuccess ? TEXT("Login successful. F2: create room, F3: search.") : TEXT("Login failed/cancelled. Check account access, then retry F1."));
}

void UBadmintonOnlineSubsystem::Host(const FString& RoomName)
{
	if (!RequireLogin()) { return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("Leave the current room with F5 first.")); return; }
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
	if (Label.IsEmpty()) { Label = TEXT("Badminton Room"); }
	Settings.Set(RoomKey, Label, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ProtocolKey, ProtocolVersion, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(SETTING_MAPNAME, CourtMap.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	bBusy = true;
	SetStatus(TEXT("Creating a two-player room..."));
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreate));
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		bBusy = false;
		SetStatus(TEXT("Could not start room creation."));
	}
}

void UBadmintonOnlineSubsystem::OnCreate(FName Name, bool bSuccess)
{
	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	bBusy = false;
	SetStatus(bSuccess ? TEXT("Room created. Waiting for another player.") : TEXT("Room creation failed. Retry F2."));
	if (bSuccess) { UGameplayStatics::OpenLevel(GetGameInstance(), CourtMap, true, TEXT("listen")); }
}

void UBadmintonOnlineSubsystem::FindRooms()
{
	if (!RequireLogin()) { return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("Leave the current room with F5 before searching.")); return; }
	Rooms.Reset();
	RoomLabels.Reset();
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = false;
	Search->MaxSearchResults = 20;
	Search->TimeoutInSeconds = 30.f;
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(ProtocolKey, ProtocolVersion, EOnlineComparisonOp::Equals);
	bBusy = true;
	SetStatus(TEXT("Searching rooms..."));
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFind));
	if (!Sessions->FindSessions(0, Search.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		bBusy = false;
		SetStatus(TEXT("Could not start search."));
	}
}

void UBadmintonOnlineSubsystem::OnFind(bool bSuccess)
{
	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	bBusy = false;
	if (!bSuccess || !Search.IsValid()) { SetStatus(TEXT("Search failed. Retry F3.")); return; }
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
	SetStatus(FString::Printf(TEXT("Found %d room(s). F4 joins room 0; F3 refreshes."), Rooms.Num()));
}

void UBadmintonOnlineSubsystem::JoinRoom(int32 Index)
{
	if (!RequireLogin()) { return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("Leave the current room with F5 first.")); return; }
	if (!Rooms.IsValidIndex(Index)) { SetStatus(TEXT("No room at that index. Search with F3 first.")); return; }
	bBusy = true;
	SetStatus(TEXT("Joining room..."));
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoin));
	if (!Sessions->JoinSession(0, NAME_GameSession, Rooms[Index]))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		bBusy = false;
		SetStatus(TEXT("Could not start join. Search again with F3."));
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
		SetStatus(TEXT("Joined lobby. Connecting to court..."));
		PC->ClientTravel(Address, TRAVEL_Absolute);
		return;
	}
	SetStatus(TEXT("Join failed: room full, unavailable, or connection unresolved."));
	RecoverFromTravelFailure();
}

void UBadmintonOnlineSubsystem::Leave()
{
	if (bBusy) { SetStatus(TEXT("Wait for the current operation before leaving.")); return; }
	if (!Sessions.IsValid()) { return; }
	if (!Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("Not in an online room.")); return; }
	bBusy = true;
	SetStatus(TEXT("Leaving room..."));
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroy));
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		bBusy = false;
		bRecovering = false;
		SetStatus(TEXT("Could not leave room. Retry F5."));
	}
}

void UBadmintonOnlineSubsystem::OnDestroy(FName Name, bool bSuccess)
{
	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	bBusy = false;
	bRecovering = false;
	Rooms.Reset();
	RoomLabels.Reset();
	SetStatus(bSuccess ? TEXT("Left room. F2: create, F3: search.") : TEXT("Room cleanup failed. Retry F5."));
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
		ConnectionNotice = TEXT("Connection lost or rejected. Reconnect or create another room.");
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_CONNECTION_NOTICE connection lost or rejected"));
		if (!bEnabled) { return; }
		SetStatus(TEXT("Connection lost or rejected. Cleaning up room."));
		RecoverFromTravelFailure();
	}
}

void UBadmintonOnlineSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		ConnectionNotice = TEXT("Could not open the court. Check the address and try again.");
		UE_LOG(LogTemp, Display, TEXT("BADMINTON_CONNECTION_NOTICE travel failed"));
		if (!bEnabled) { return; }
		SetStatus(TEXT("Could not open the court. Cleaning up room."));
		RecoverFromTravelFailure();
	}
}
