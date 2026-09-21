#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "BadmintonOnlineSubsystem.generated.h"

class UNetDriver;

// Owns online operations across map travel. Credentials remain in the EOS plugin configuration.
UCLASS()
class FPS_DELL2G_API UBadmintonOnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	void Login();
	void Host(const FString& RoomName);
	void FindRooms();
	void JoinRoom(int32 Index);
	void Leave();
	bool IsEnabled() const { return bEnabled; }
	const FString& GetStatus() const { return Status; }
	const FString& GetConnectionNotice() const { return ConnectionNotice; }
	void ClearConnectionNotice() { ConnectionNotice.Empty(); }
	const TArray<FString>& GetRoomLabels() const { return RoomLabels; }

private:
	bool RequireLogin();
	void SetStatus(const FString& Message);
	void OnLogin(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error);
	void OnCreate(FName Name, bool bSuccess);
	void OnFind(bool bSuccess);
	void OnJoin(FName Name, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroy(FName Name, bool bSuccess);
	void OnNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error);
	void RecoverFromTravelFailure();
	void ClearDelegates();

	IOnlineIdentityPtr Identity;
	IOnlineSessionPtr Sessions;
	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FOnlineSessionSearchResult> Rooms;
	TArray<FString> RoomLabels;
	FDelegateHandle LoginHandle, CreateHandle, FindHandle, JoinHandle, DestroyHandle;
	FDelegateHandle NetworkFailureHandle, TravelFailureHandle;
	FString Status = TEXT("오프라인 모드");
	FString ConnectionNotice;
	bool bEnabled = false;
	bool bBusy = false;
	bool bRecovering = false;
};
