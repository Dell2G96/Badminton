#include "BadmintonHUD.h"

#include "BadmintonOnlineSubsystem.h"
#include "BadmintonPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"

bool ABadmintonHUD::DrawOnlineLobby()
{
	auto* Controller = Cast<ABadmintonPlayerController>(PlayerOwner);
	auto* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>() : nullptr;
	if (!Controller || !Online) { return false; }
	UIScale = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 720.f);
	const float Width = Canvas->ClipX / UIScale, Height = Canvas->ClipY / UIScale;
	const FLinearColor Accent(.25f, .95f, .77f), Muted(.65f, .72f, .8f), Warning(1.f, .72f, .28f);
	if (!Controller->IsOnlineLobbyOpen())
	{
		Label(TEXT("F6 온라인 메뉴"), Width - 220, 82, .8f, Accent, 190);
		if (!Online->GetConnectionNotice().IsEmpty())
		{
			Panel(Width * .5f - 300, 88, 600, 28, FLinearColor(.025f, .04f, .065f, .95f));
			Label(Online->GetConnectionNotice(), Width * .5f, 93, .85f, Warning, 580, true);
		}
		return false;
	}
	const float X = (Width - 920) * .5f, Y = (Height - 560) * .5f;
	Panel(0, 0, Width, Height, FLinearColor(0.f, .01f, .02f, .72f));
	Panel(X, Y, 920, 560, FLinearColor(.025f, .04f, .065f, .98f));
	Label(TEXT("온라인 대전"), X + 28, Y + 22, 1.65f, Accent, 600);
	Label(Online->IsLoggedIn() ? TEXT("에픽 계정 로그인됨") : TEXT("로그인 전"), X + 650, Y + 30, .9f, Muted, 230);
	Label(Online->GetStatus(), X + 28, Y + 68, 1.f, Online->IsBusy() ? Warning : FLinearColor::White, 860);
	const FString Notice = !Online->GetConnectionNotice().IsEmpty() ? Online->GetConnectionNotice()
		: !Online->IsEnabled() ? TEXT("인터넷 대전은 Start-BadmintonEOS.cmd로 실행하세요. F6으로 연습 코트에 돌아갑니다.")
		: TEXT("로그인 후 방을 만들거나 검색하세요. 두 플레이어가 준비하면 경기가 시작됩니다.");
	Label(Notice, X + 28, Y + 99, .88f, !Online->GetConnectionNotice().IsEmpty() ? Warning : Muted, 860);
	auto Button = [&](FName Name, const FString& Text, float BX, float BY, float BW, bool bEnabled)
	{
		Panel(BX, BY, BW, 38, bEnabled ? FLinearColor(.08f, .22f, .25f, 1.f) : FLinearColor(.055f, .07f, .09f, 1.f));
		Label(Text, BX + BW * .5f, BY + 9, .9f, bEnabled ? Accent : Muted.CopyWithNewOpacity(.5f), BW - 18, true);
		if (bEnabled) { AddHitBox(FVector2D(BX, BY) * UIScale, FVector2D(BW, 38) * UIScale, Name, true); }
	};
	const bool bIdle = !Online->IsBusy();
	const bool bCanBrowse = bIdle && Online->IsLoggedIn() && !Online->IsInRoom();
	Button(TEXT("OnlineLogin"), TEXT("F1 로그인"), X + 28, Y + 138, 206, bIdle && Online->IsEnabled());
	Button(TEXT("OnlineHost"), TEXT("F2 방 만들기"), X + 247, Y + 138, 206, bCanBrowse);
	Button(TEXT("OnlineFind"), TEXT("F3 새로고침"), X + 466, Y + 138, 206, bCanBrowse);
	Button(TEXT("OnlineLeave"), TEXT("F5 방 나가기"), X + 685, Y + 138, 206, bIdle && Online->IsInRoom());
	const TArray<FString>& Rooms = Online->GetRoomLabels();
	const int32 Selected = Online->GetSelectedRoom();
	const int32 Page = FMath::Max(Selected, 0) / 6;
	const int32 PageCount = FMath::Max(1, FMath::DivideAndRoundUp(Rooms.Num(), 6));
	Label(FString::Printf(TEXT("참가 가능한 방 %d개  /  %d / %d 쪽"), Rooms.Num(), Page + 1, PageCount), X + 28, Y + 194, .95f, Muted, 580);
	Button(TEXT("OnlinePrevious"), TEXT("이전"), X + 700, Y + 185, 89, bCanBrowse && Page > 0);
	Button(TEXT("OnlineNext"), TEXT("다음"), X + 802, Y + 185, 89, bCanBrowse && Page + 1 < PageCount);
	Panel(X + 28, Y + 234, 864, 240, FLinearColor(.015f, .025f, .04f, 1.f));
	if (Rooms.IsEmpty())
	{
		Label(Online->IsBusy() ? TEXT("잠시 기다려 주세요...") : TEXT("F3으로 방을 검색하거나 F2로 새 방을 만드세요."), X + 460, Y + 332, 1.1f, Muted, 790, true);
	}
	for (int32 Index = Page * 6; Index < FMath::Min(Rooms.Num(), (Page + 1) * 6); ++Index)
	{
		const float RowY = Y + 237 + (Index % 6) * 39;
		Panel(X + 31, RowY, 858, 36, Index == Selected ? FLinearColor(.08f, .25f, .27f, 1.f) : FLinearColor(.04f, .07f, .1f, 1.f));
		Label((Index == Selected ? TEXT(">  ") : TEXT("   ")) + Rooms[Index], X + 44, RowY + 8, 1.f, Index == Selected ? Accent : FLinearColor::White, 827);
		if (bCanBrowse) { AddHitBox(FVector2D(X + 31, RowY) * UIScale, FVector2D(858, 36) * UIScale, FName(*FString::Printf(TEXT("OnlineRoom%d"), Index)), true); }
	}
	Button(TEXT("OnlineJoin"), TEXT("F4 선택한 방 참가"), X + 28, Y + 488, 280, bCanBrowse && Rooms.IsValidIndex(Selected));
	Button(TEXT("OnlineNotice"), TEXT("연결 알림 확인"), X + 322, Y + 488, 280, !Online->GetConnectionNotice().IsEmpty());
	Button(TEXT("OnlineClose"), TEXT("F6 코트로 돌아가기"), X + 616, Y + 488, 276, true);
	Label(TEXT("클릭 / ↑↓ 방 선택 / PgUp·PgDn 페이지 / Enter 참가  ·  메뉴를 열어도 경기는 계속됩니다"), X + 460, Y + 538, .75f, Muted, 864, true);
	return true;
}

void ABadmintonHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	auto* Controller = Cast<ABadmintonPlayerController>(PlayerOwner);
	auto* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBadmintonOnlineSubsystem>() : nullptr;
	if (!Controller || !Online || !Controller->IsOnlineLobbyOpen()) { return; }
	if (BoxName == TEXT("OnlineClose")) { Controller->SetOnlineLobbyOpen(false); }
	else if (BoxName == TEXT("OnlineNotice")) { Online->ClearConnectionNotice(); }
	else if (Online->IsBusy()) { return; }
	else if (BoxName == TEXT("OnlineLogin")) { Controller->BadmintonEOSLogin(); }
	else if (BoxName == TEXT("OnlineHost")) { Controller->BadmintonEOSHost(); }
	else if (BoxName == TEXT("OnlineFind")) { Controller->BadmintonEOSFind(); }
	else if (BoxName == TEXT("OnlineJoin")) { Controller->JoinSelectedEOSRoom(); }
	else if (BoxName == TEXT("OnlineLeave")) { Controller->BadmintonEOSLeave(); }
	else if (BoxName == TEXT("OnlinePrevious")) { Online->MoveRoomSelection(-6); }
	else if (BoxName == TEXT("OnlineNext")) { Online->MoveRoomSelection(6); }
	else if (BoxName.ToString().StartsWith(TEXT("OnlineRoom")))
	{
		int32 Index = INDEX_NONE;
		if (LexTryParseString(Index, *BoxName.ToString().RightChop(10))) { Online->SelectRoom(Index); }
	}
}
