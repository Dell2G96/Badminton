#include "BadmintonHUD.h"

#include "AbilitySystemComponent.h"
#include "BadmintonAttributeSet.h"
#include "BadmintonCharacter.h"
#include "BadmintonMinimap.h"
#include "BadmintonClearAbility.h"
#include "BadmintonDashAbility.h"
#include "BadmintonGameState.h"
#include "BadmintonOnlineSubsystem.h"
#include "BadmintonPlayerState.h"
#include "BadmintonPlayerController.h"
#include "BadmintonShuttle.h"
#include "BadmintonShotData.h"
#include "BadmintonTiming.h"
#include "BadmintonRacketPhysics.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

namespace
{
	const FLinearColor PanelColor(.025f, .04f, .065f, .92f);
	const FLinearColor FirstPersonMuted(.62f, .7f, .78f);
	const FLinearColor FirstPersonAccent(.25f, .95f, .77f);
	const FLinearColor FirstPersonWarning(1.f, .72f, .28f);

	const TCHAR* PointReason(EBadmintonPointReason Reason)
	{
		switch (Reason)
		{
		case EBadmintonPointReason::LandedIn: return TEXT("코트 안에 떨어졌습니다");
		case EBadmintonPointReason::Out: return TEXT("셔틀콕이 코트 밖으로 나갔습니다");
		case EBadmintonPointReason::ServiceFault: return TEXT("서브 반칙");
		case EBadmintonPointReason::InvalidCrossing: return TEXT("네트 통과 실패");
		default: return TEXT("");
		}
	}
}

void ABadmintonHUD::Panel(float X, float Y, float Width, float Height, const FLinearColor& Color)
{
	DrawRect(Color, X * UIScale, Y * UIScale, Width * UIScale, Height * UIScale);
}

void ABadmintonHUD::Label(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float MaxWidth, bool bCentered)
{
	if (!KoreanFont)
	{
		// Ship the same font used for measuring and drawing; editor-only fallback is unavailable in a package.
		KoreanFont = NewObject<UFont>(this);
		KoreanFont->FontCacheType = EFontCacheType::Runtime;
		KoreanFont->LegacyFontSize = 13;
		KoreanFont->LegacyFontName = FName(TEXT("Regular"));
		KoreanFont->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Emplace(
			KoreanFont->LegacyFontName, FPaths::ProjectContentDir() / TEXT("Badminton/UI/Fonts/NanumGothic.ttf"),
			EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	}
	UFont* Font = KoreanFont;
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font, Size);
	if (MaxWidth > 0.f && Width > MaxWidth)
	{
		Size *= MaxWidth / Width;
		Width = MaxWidth;
	}
	DrawText(Text, Color, (X - (bCentered ? Width * .5f : 0.f)) * UIScale, Y * UIScale, Font, Size * UIScale);
}

void ABadmintonHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas && Canvas->ClipX > 0 && Canvas->ClipY > 0 && DrawOnlineLobby()) { return; }
	const ABadmintonGameState* Match = GetWorld()->GetGameState<ABadmintonGameState>();
	const ABadmintonPlayerState* State = PlayerOwner ? PlayerOwner->GetPlayerState<ABadmintonPlayerState>() : nullptr;
	const ABadmintonPlayerController* Controller = Cast<ABadmintonPlayerController>(PlayerOwner);
	if (!Canvas || !GEngine || !Match || !State || !Controller || Canvas->ClipX <= 0 || Canvas->ClipY <= 0) { return; }
	if (Controller->UsesThirdPersonControl()) { DrawThirdPersonHUD(Match); return; }
	if (Controller->IsDualViewEnabled()) { DrawDualViewHUD(Match); return; }
	UIScale = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 720.f);
	const float Width = Canvas->ClipX / UIScale, Height = Canvas->ClipY / UIScale, Center = Width * .5f;
	if (State->CourtSide < 0 || State->CourtSide > 1) { Label(TEXT("코트에 입장하는 중..."), Center, 40, 1.2f, FirstPersonAccent, 300, true); return; }
	const bool bServing = State->CourtSide == Match->ServingSide;
	const bool bLobby = Match->Phase == EBadmintonPhase::WaitingForReady || Match->Phase == EBadmintonPhase::WaitingForPlayers;
	const bool bFinished = Match->Phase == EBadmintonPhase::MatchFinished;
	const bool bPlay = Match->Phase == EBadmintonPhase::Rally || Match->Phase == EBadmintonPhase::ReadyToServe;
	const bool bArmed = Controller->IsTimingArmed();
	const bool bSelectedStroke = !bArmed && Match->Phase == EBadmintonPhase::Rally;
	const bool bCanHit = Controller->GetContactHint() == Badminton::EContactHint::Ready;
	const FString ContactText = bSelectedStroke && bCanHit ? TEXT("타격 준비 완료 / 왼쪽 클릭") : Badminton::ContactHintText(Controller->GetContactHint());
	DrawShuttleTracking(Match);
	DrawAimPreview();
	DrawMinimap(Match, State->CourtSide, Width);
	if (bPlay)
	{
		const FVector Target = Controller->GetPracticeProgress().Target(State->CourtSide);
		for (int32 Index = 0; Index < 32; ++Index)
		{
			const float A = Index * 2.f * PI / 32.f, B = (Index + 1) * 2.f * PI / 32.f;
			FVector2D Start, End;
			if (ProjectFirstPerson(Target + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 85.f, Start)
				&& ProjectFirstPerson(Target + FVector(FMath::Cos(B), FMath::Sin(B), 0) * 85.f, End))
			{ DrawLine(Start.X, Start.Y, End.X, End.Y, FirstPersonWarning, 2.f * UIScale); }
		}
		FVector2D Screen;
		if (ProjectFirstPerson(Target, Screen)) { Label(TEXT("도전 목표"), Screen.X / UIScale, Screen.Y / UIScale, .8f, FirstPersonWarning, 110, true); }
	}
	if (bPlay)
	{
		const FVector2D RacketScreen = Controller->GetRacketScreenPosition();
		const float CX = Width * RacketScreen.X, CY = Height * RacketScreen.Y;
		const FLinearColor Crosshair = bCanHit ? FirstPersonAccent : FLinearColor::White;
		Panel(CX-7, CY-.75f, 4, 1.5f, Crosshair);
		Panel(CX+3, CY-.75f, 4, 1.5f, Crosshair);
		Panel(CX-.75f, CY-7, 1.5f, 4, Crosshair);
		Panel(CX-.75f, CY+3, 1.5f, 4, Crosshair);
	}
	Label(Controller->IsAutomaticTimingTestRunning() ? TEXT("자동 검사 중 / 카메라 자동 조작") : TEXT("배드민턴 / 1인칭"), 24, 24, 1.25f, FirstPersonAccent, 300);
	Label(FString::Printf(TEXT("상대 난이도 %s / 11점 선취 / N 변경"), Controller->GetAIDifficulty() == 0 ? TEXT("쉬움") : Controller->GetAIDifficulty() == 1 ? TEXT("보통") : TEXT("어려움")), 24, 49, .9f, FirstPersonMuted, 310);
	Label(FString::Printf(TEXT("마우스 감도 %.2f / - + 조절"), Controller->GetMouseSensitivity()), 24, 70, .85f, FirstPersonMuted, 300);
	Label(Controller->GetSelectedShot() == EBadmintonShot::Smash ? TEXT("라켓 면 고정 / 스매시") : FString::Printf(TEXT("라켓 면 좌우 %+.0f도 / 휠 조절"), Controller->GetRacketTilt()), 24, 88, .85f, FirstPersonAccent, 300);
	const auto& Practice = Controller->GetPracticeProgress();
	Panel(24, 106, 218, 100, PanelColor);
	Label(TEXT("연습 도전 과제"), 35, 116, .85f, FirstPersonAccent, 196);
	Label(FString::Printf(TEXT("랠리 %d / 10회   최고 %d회"), Practice.RallyHits, Practice.BestRally), 35, 137, .85f, Practice.RallyHits >= 10 ? FirstPersonAccent : FirstPersonMuted, 196);
	Label(FString::Printf(TEXT("좌우 목표 %d / 2개"), FMath::Min(Practice.Targets, 2)), 35, 159, .85f, Practice.Targets >= 2 ? FirstPersonAccent : FirstPersonMuted, 196);
	Label(FString::Printf(TEXT("드롭 + 스매시 득점 %d회"), Practice.ComboPoints), 35, 181, .85f, Practice.ComboPoints > 0 ? FirstPersonAccent : FirstPersonMuted, 196);
	if (bPlay)
	{
		FVector Origin, Velocity;
		const bool bPredicted = Controller->GetShotDirectionPreview(Origin, Velocity);
		Panel(24, 218, 218, 150, PanelColor);
		Label(bPredicted ? TEXT("지금 타격할 때의 예상 방향") : TEXT("타격 방향 미리보기"), 35, 228, .8f, FirstPersonAccent, 196);
		if (bPredicted)
		{
			const float SideSign = Badminton::ForwardSign(State->CourtSide);
			const FVector2D Heading = FVector2D(Velocity.Y * SideSign, -Velocity.X * SideSign).GetSafeNormal();
			const FVector2D Start(133, 319), End = Start + Heading * 56.f;
			const FVector2D Cross(-Heading.Y, Heading.X);
			auto Line = [&](FVector2D A, FVector2D B, FLinearColor Color, float Thickness)
			{ DrawLine(A.X*UIScale, A.Y*UIScale, B.X*UIScale, B.Y*UIScale, Color, Thickness*UIScale); };
			Line(FVector2D(78,319), FVector2D(188,319), FirstPersonMuted.CopyWithNewOpacity(.35f), 1.f);
			Line(Start, FVector2D(133,261), FirstPersonMuted.CopyWithNewOpacity(.35f), 1.f);
			Line(Start, End, FirstPersonAccent, 3.f);
			Line(End, End-Heading*10.f+Cross*6.f, FirstPersonAccent, 3.f);
			Line(End, End-Heading*10.f-Cross*6.f, FirstPersonAccent, 3.f);
			Label(TEXT("좌"), 64, 297, .8f, FirstPersonMuted);
			Label(TEXT("우"), 193, 297, .8f, FirstPersonMuted);
			const float Degrees = FMath::RadiansToDegrees(FMath::Atan2(Heading.X, -Heading.Y));
			Label(FString::Printf(TEXT("타구 방향 %+.0f도 / 위에서 본 모습"), Degrees), 133, 346, .8f, FirstPersonAccent, 196, true);
#if !UE_BUILD_SHIPPING
			if (Controller->IsAutomaticTimingTestRunning() && !bDirectionPreviewTestReported)
			{
				UE_LOG(LogTemp, Display, TEXT("BADMINTON_DIRECTION_UI_TEST PASS velocity=%s angle=%.1f"), *Velocity.ToString(), Controller->GetRacketTilt());
				bDirectionPreviewTestReported = true;
			}
#endif
		}
		else
		{
			Label(Controller->GetSelectedShot() == EBadmintonShot::Smash ? TEXT("스매시 / 라켓 면 고정") : TEXT("셔틀콕이 올 때까지 기다리세요"),133,281,.8f,FirstPersonMuted,196,true);
			Label(TEXT("휠 위: 오른쪽 / 아래: 왼쪽"),133,346,.75f,FirstPersonMuted,196,true);
		}
	}
	Panel(Center - 124, 16, 248, 63, PanelColor);
	Label(TEXT("나"), Center - 83, 27, .9f, FirstPersonAccent, 65, true);
	Label(TEXT("상대"), Center + 83, 27, .9f, FirstPersonMuted, 65, true);
	Label(FString::Printf(TEXT("%d : %d"), Match->GetScore(State->CourtSide), Match->GetScore(1-State->CourtSide)), Center, 40, 2.f, FLinearColor::White, 160, true);
	const auto* Attr = State->GetAttributes();
	Panel(Width-221, 20, 197, 49, PanelColor);
	Label(FString::Printf(TEXT("스태미나 %.0f"), Attr->GetStamina()), Width-209, 29, .9f, FirstPersonAccent, 172);
	Panel(Width-209, 53, 172, 4, FirstPersonMuted);
	Panel(Width-209, 53, 172 * FMath::Clamp(Attr->GetStamina()/FMath::Max(1.f, Attr->GetMaxStamina()), 0.f, 1.f), 4, FirstPersonAccent);
	if (bLobby || bFinished)
	{
		Panel(Center-260, Height*.38f, 520, 87, PanelColor);
		const TCHAR* Heading = bLobby ? TEXT("경기를 시작할까요?") : Match->WinnerSide == State->CourtSide ? TEXT("승리!") : TEXT("패배");
		Label(Heading, Center, Height*.38f+13, 1.7f, FirstPersonAccent, 490, true);
		Label(bLobby ? TEXT("Enter - 컴퓨터 상대 경기 시작") : TEXT("Enter - 재경기"), Center, Height*.38f+50, 1.15f, FLinearColor::White, 490, true);
	}
	else if (Match->Phase == EBadmintonPhase::RallyComplete)
	{
		Label(bServing ? TEXT("내 득점") : TEXT("상대 득점"), Center, 104, 1.4f, FirstPersonAccent, 420, true);
		Label(PointReason(Match->LastPointReason), Center, 132, .95f, FirstPersonMuted, 420, true);
	}
	else if (Match->Phase == EBadmintonPhase::ReadyToServe)
	{
		Label(bServing ? TEXT("내 서브 - 왼쪽 클릭") : TEXT("상대 서브 - 준비하세요"), Center, 99, 1.1f, FirstPersonAccent, 730, true);
	}
	// Five shots occupy one compact row, leaving the lower centre clear for the timing gauge.
	const EBadmintonShot Shots[] = {EBadmintonShot::Drop, EBadmintonShot::Smash, EBadmintonShot::Clear, EBadmintonShot::Hairpin, EBadmintonShot::Receive};
	const TCHAR* ShotKeys[] = {TEXT("Q"), TEXT("2"), TEXT("E"), TEXT("R"), TEXT("Space")};
	const UBadmintonShotData* Data = Match->GetShotData();
	for (int32 Index=0; Index<5; ++Index)
	{
		const float X=Center-384+Index*156;
		const bool bSelected=Controller->GetSelectedShot()==Shots[Index];
		const FLinearColor Color=bSelected ? FirstPersonAccent : Attr->GetStamina()<Data->Get(Shots[Index]).StaminaCost ? FirstPersonWarning : FirstPersonMuted;
		Panel(X, Height-85, 150, 49, PanelColor);
		Panel(X, Height-85, 150, 2, Color);
		Label(FString::Printf(TEXT("%s  %s"),ShotKeys[Index],Badminton::ShotLabel(Shots[Index])),X+9,Height-74,.92f,Color,134);
		Label(Shots[Index] == EBadmintonShot::Smash ? TEXT("타이밍에 맞춰 왼쪽 클릭") : TEXT("선택 후 왼쪽 클릭"),X+9,Height-53,.75f,FirstPersonMuted,134);
	}
	if (bSelectedStroke)
	{
		const float Y = Height - 188;
		Panel(Center-242, Y-27, 484, 107, PanelColor);
		Label(Controller->GetSelectedShot() == EBadmintonShot::Smash ? TEXT("스매시 / 2를 눌러 준비") : FString::Printf(TEXT("%s / 왼쪽 클릭으로 타격"), Badminton::ShotLabel(Controller->GetSelectedShot())), Center, Y-16, 1.1f, FirstPersonAccent, 450, true);
		Label(TEXT("타격 후 기본 리시브로 복귀 / Q E R 선택"), Center, Y+17, 1.05f, FLinearColor::White, 455, true);
		Label(ContactText, Center, Y+52, .88f, bCanHit?FirstPersonAccent:FirstPersonWarning, 455, true);
	}
	else if (bArmed)
	{
		const float Y=Height-188, BarX=Center-220, BarWidth=440;
		Panel(Center-242,Y-27,484,107,PanelColor);
		Label(FString::Printf(TEXT("%s / 왼쪽 클릭으로 타격"),Badminton::ShotLabel(Controller->GetSelectedShot())),Center,Y-16,1.1f,FirstPersonAccent,450,true);
		Panel(BarX,Y+17,BarWidth,20,FLinearColor(.15f,.20f,.27f));
		if (Controller->HasContactForecast())
		{
		const float Perfect=Controller->GetTimingPerfectPosition();
		for (int32 Zone=0; Zone<2; ++Zone)
		{
			const float Half=Controller->GetTimingWindowWidth(Zone==0 ? .125f : .045f);
			const float Left=FMath::Clamp(Perfect-Half,0.f,1.f), Right=FMath::Clamp(Perfect+Half,0.f,1.f);
			Panel(BarX+Left*BarWidth,Y+17,(Right-Left)*BarWidth,20,Zone==0 ? FLinearColor(.23f,.49f,.43f) : FirstPersonAccent);
		}
		const float Cursor=BarX+Controller->GetTimingProgress()*BarWidth;
		Panel(Cursor-2,Y+10,4,34,FLinearColor::White);
		}
		else { Label(TEXT("타격 가능한 거리로 이동하세요"), Center, Y+20, .9f, FirstPersonWarning, 420, true); }
		Label(ContactText,Center,Y+52,.88f,bCanHit?FirstPersonAccent:FirstPersonWarning,455,true);
	}
	else if (bPlay)
	{
		Label(TEXT("마우스: 라켓 조작 | 왼쪽 클릭: 서브 | 2 + 클릭: 스매시"),Center,Height-145,1.05f,FLinearColor::White,730,true);
	}
	const FString Challenge = Controller->GetChallengeMessage();
	if (!Challenge.IsEmpty()) { Label(Challenge, Center, 165, 1.15f, FirstPersonAccent, 650, true); }
	const FString Feedback=Controller->GetTimingMessage();
	if (!Feedback.IsEmpty())
	{
		Panel(Center-242,Height-247,484,36,PanelColor);
		Label(Feedback,Center,Height-238,1.05f,FirstPersonWarning,462,true);
	}
	Label(TEXT("WASD 이동 | Shift 대시 | 휠: 라켓 면 | 오른쪽 클릭 취소 | 가운데 클릭 초기화 | V 분할 화면"),Center,Height-22,.82f,FirstPersonMuted,800,true);
}

void ABadmintonHUD::DrawShuttleTracking(const ABadmintonGameState* Match)
{
	if (!TrackedShuttle.IsValid())
	{
		for (TActorIterator<ABadmintonShuttle> It(GetWorld()); It; ++It) { TrackedShuttle = *It; break; }
	}
	if (!TrackedShuttle.IsValid()) { return; }
	const FBadmintonFlightState& Flight = TrackedShuttle->GetFlight();
	if (Match->Phase != EBadmintonPhase::Rally || !Flight.bFlying || Flight.RallyId != Match->RallyId)
	{
		ShuttleTrail.Reset();
		return;
	}
	if (TrailRallyId != Flight.RallyId || TrailSequence != Flight.ShotSequence)
	{
		ShuttleTrail.Reset();
		TrailRallyId = Flight.RallyId;
		TrailSequence = Flight.ShotSequence;
		LastTrailSampleTime = 0;
	}
	if (Flight.bHasLanding)
	{
		const FVector Landing = FVector(Flight.Landing) + FVector(0, 0, 3.f);
		const bool bOut = FMath::Abs(Landing.X) > Badminton::HalfLength || FMath::Abs(Landing.Y) > Badminton::HalfWidth;
		const FLinearColor LandingColor = bOut ? FLinearColor(1.f, .3f, .25f) : FLinearColor(.25f, .8f, 1.f);
		for (int32 Index = 0; Index < 48; ++Index)
		{
			const float A = Index * 2.f * PI / 48.f, B = (Index + 1) * 2.f * PI / 48.f;
			FVector2D Start, End;
			if (ProjectFirstPerson(Landing + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 38.f, Start)
				&& ProjectFirstPerson(Landing + FVector(FMath::Cos(B), FMath::Sin(B), 0) * 38.f, End))
			{ DrawLine(Start.X, Start.Y, End.X, End.Y, LandingColor, 3.f * UIScale); }
		}
		FVector2D Screen;
		if (ProjectFirstPerson(Landing, Screen))
		{
			DrawLine(Screen.X - 6 * UIScale, Screen.Y, Screen.X + 6 * UIScale, Screen.Y, LandingColor, 2.f * UIScale);
			DrawLine(Screen.X, Screen.Y - 6 * UIScale, Screen.X, Screen.Y + 6 * UIScale, LandingColor, 2.f * UIScale);
			Panel(Screen.X / UIScale - 65, Screen.Y / UIScale - 29, 130, 22, PanelColor);
			Label(bOut ? TEXT("예상 낙하 / 아웃") : TEXT("예상 낙하"), Screen.X / UIScale, Screen.Y / UIScale - 25, .85f, LandingColor, 124, true);
		}
	}
	const FVector Position = TrackedShuttle->GetActorLocation();
	if (GetWorld()->GetTimeSeconds() - LastTrailSampleTime >= .025)
	{
		ShuttleTrail.Add(Position);
		if (ShuttleTrail.Num() > 6) { ShuttleTrail.RemoveAt(0); }
		LastTrailSampleTime = GetWorld()->GetTimeSeconds();
	}
	// Sample only the displayed path. This is not a landing prediction or a collision shape.
	for (int32 Index = 1; Index < ShuttleTrail.Num(); ++Index)
	{
		FVector2D Start, End;
		if (ProjectFirstPerson(ShuttleTrail[Index - 1], Start)
			&& ProjectFirstPerson(ShuttleTrail[Index], End))
		{
			FLinearColor TrailColor = FirstPersonWarning;
			TrailColor.A = .65f * Index / ShuttleTrail.Num();
			DrawLine(Start.X, Start.Y, End.X, End.Y, TrailColor, 2.f * UIScale);
		}
	}
	FVector2D Screen;
	if (ProjectFirstPerson(Position, Screen))
	{
		const float Radius = 8.f * UIScale;
		const FVector2D Corners[] = {Screen + FVector2D(0, -Radius), Screen + FVector2D(Radius, 0),
			Screen + FVector2D(0, Radius), Screen + FVector2D(-Radius, 0)};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector2D& A = Corners[Index];
			const FVector2D& B = Corners[(Index + 1) % 4];
			DrawLine(A.X, A.Y, B.X, B.Y, FirstPersonWarning, 1.5f * UIScale);
		}
	}
}

void ABadmintonHUD::DrawAimPreview()
{
	const ABadmintonPlayerController* Controller = Cast<ABadmintonPlayerController>(PlayerOwner);
	FVector Target;
	EBadmintonShot Shot;
	if (!Controller || !Controller->GetAimPreview(Target, Shot)) { return; }
	// A ground reference for the shot direction, not an exact landing prediction.
	Target.Z = 4.f;
	for (int32 Index = 0; Index < 24; ++Index)
	{
		const float A = Index * 2.f * PI / 24.f;
		const float B = (Index + 1) * 2.f * PI / 24.f;
		FVector2D Start, End;
		if (ProjectFirstPerson(Target + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 28, Start)
			&& ProjectFirstPerson(Target + FVector(FMath::Cos(B), FMath::Sin(B), 0) * 28, End))
		{
			DrawLine(Start.X, Start.Y, End.X, End.Y, FirstPersonAccent, 1.5f * UIScale);
		}
	}
	FVector2D Screen;
	if (ProjectFirstPerson(Target, Screen))
	{
		DrawLine(Screen.X - 5 * UIScale, Screen.Y, Screen.X + 5 * UIScale, Screen.Y, FirstPersonAccent, UIScale);
		DrawLine(Screen.X, Screen.Y - 5 * UIScale, Screen.X, Screen.Y + 5 * UIScale, FirstPersonAccent, UIScale);
		const TCHAR* Name = Controller->IsTimingArmed() ? TEXT("목표 고정") : TEXT("타격 목표");
		Panel(Screen.X / UIScale - 53, Screen.Y / UIScale + 10, 106, 22, PanelColor);
		Label(FString::Printf(TEXT("%s"), Name), Screen.X / UIScale, Screen.Y / UIScale + 14, .9f, FirstPersonAccent, 150, true);
	}
}

void ABadmintonHUD::DrawMinimap(const ABadmintonGameState* Match, int32 LocalSide, float Width)
{
	const float X = Width - 221.f, Y = 86.f;
	const FVector2D Origin(X + 22.5f, Y + 35.f);
	const FVector2D Size(152.f, 243.2f); // Same centimetres-per-pixel on both axes.
	const FLinearColor OpponentColor(1.f, .38f, .38f), LandingColor(.25f, .8f, 1.f);
	Panel(X, Y, 197.f, 345.f, PanelColor);
	Label(TEXT("코트 지도"), X + 98.5f, Y + 11.f, .95f, FLinearColor::White, 175.f, true);
	Panel(Origin.X, Origin.Y, Size.X, Size.Y, FLinearColor(.04f, .075f, .09f));
	auto Project = [&](const FVector& Point)
	{
		const FVector2D UV = Badminton::MinimapPoint(Point, LocalSide);
		return Origin + FVector2D(UV.X * Size.X, UV.Y * Size.Y);
	};
	auto Line = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness = 1.f)
	{ DrawLine(A.X * UIScale, A.Y * UIScale, B.X * UIScale, B.Y * UIScale, Color, Thickness * UIScale); };
	auto CourtLine = [&](const FVector& A, const FVector& B) { Line(Project(A), Project(B), FirstPersonMuted, .8f); };
	const FVector2D A = Project(FVector(-Badminton::HalfLength, -Badminton::HalfWidth, 0));
	const FVector2D B = Project(FVector(Badminton::HalfLength, Badminton::HalfWidth, 0));
	Panel(FMath::Min(A.X,B.X), FMath::Min(A.Y,B.Y), FMath::Abs(A.X-B.X), FMath::Abs(A.Y-B.Y), FLinearColor(.035f,.20f,.17f));
	for (float Sign : {-1.f, 1.f})
	{
		CourtLine(FVector(Sign * Badminton::HalfLength,-Badminton::HalfWidth,0), FVector(Sign * Badminton::HalfLength,Badminton::HalfWidth,0));
		CourtLine(FVector(-Badminton::HalfLength,Sign * Badminton::HalfWidth,0), FVector(Badminton::HalfLength,Sign * Badminton::HalfWidth,0));
		CourtLine(FVector(Sign * Badminton::ShortServiceLine,-Badminton::HalfWidth,0), FVector(Sign * Badminton::ShortServiceLine,Badminton::HalfWidth,0));
		CourtLine(FVector(Sign * Badminton::ShortServiceLine,0,0), FVector(Sign * Badminton::HalfLength,0,0));
	}
	Line(Project(FVector(0,-305,0)), Project(FVector(0,305,0)), FLinearColor::White, 1.6f);
	auto MarkerPoint = [&](const FVector& Point)
	{
		const FVector2D Raw = Project(Point);
		const FVector2D Clamped(FMath::Clamp(Raw.X, Origin.X+7., Origin.X+Size.X-7.), FMath::Clamp(Raw.Y, Origin.Y+7., Origin.Y+Size.Y-7.));
		if (!Raw.Equals(Clamped, .01)) { Line(Clamped, Clamped + (Raw-Clamped).GetSafeNormal()*5.f, FirstPersonWarning, 2.f); }
		return Clamped;
	};
	auto Ring = [&](const FVector2D& Center, float Radius, const FLinearColor& Color)
	{
		for (int32 Index=0; Index<20; ++Index)
		{
			const float P = Index * 2.f * PI / 20.f, Q = (Index+1) * 2.f * PI / 20.f;
			Line(Center + FVector2D(FMath::Cos(P),FMath::Sin(P))*Radius, Center + FVector2D(FMath::Cos(Q),FMath::Sin(Q))*Radius, Color, 1.5f);
		}
	};
	const bool bShuttle = TrackedShuttle.IsValid();
	const bool bLanding = bShuttle && Match->Phase == EBadmintonPhase::Rally && TrackedShuttle->GetFlight().bFlying
		&& TrackedShuttle->GetFlight().RallyId == Match->RallyId && TrackedShuttle->GetFlight().bHasLanding;
	if (bLanding)
	{
		const FVector Landing = TrackedShuttle->GetFlight().Landing;
		const FLinearColor Color = FMath::Abs(Landing.X)>Badminton::HalfLength || FMath::Abs(Landing.Y)>Badminton::HalfWidth ? OpponentColor : LandingColor;
		const FVector2D End = MarkerPoint(Landing), Start = MarkerPoint(TrackedShuttle->GetActorLocation());
		for (int32 Index=0; Index<10; Index+=2) { Line(FMath::Lerp(Start,End,Index/10.f),FMath::Lerp(Start,End,(Index+1)/10.f),LandingColor,.75f); }
		Ring(End,7.f,Color);
		Line(End-FVector2D(3,3),End+FVector2D(3,3),Color,1.5f);
		Line(End-FVector2D(3,-3),End+FVector2D(3,-3),Color,1.5f);
	}
	int32 Players = 0;
	for (TActorIterator<ABadmintonCharacter> It(GetWorld()); It; ++It)
	{
		const auto* State = It->GetPlayerState<ABadmintonPlayerState>();
		if (!State || State->CourtSide < 0 || State->CourtSide > 1) { continue; }
		++Players;
		const FVector2D Center = MarkerPoint(It->GetActorLocation());
		const bool bLocal = State->CourtSide == LocalSide;
		Panel(Center.X-5,Center.Y-5,10,10,FLinearColor(.01f,.02f,.03f));
		if (bLocal) { Panel(Center.X-3.5f,Center.Y-3.5f,7,7,FirstPersonAccent); }
		else
		{
			const FVector2D Points[] = {Center+FVector2D(0,-5),Center+FVector2D(5,0),Center+FVector2D(0,5),Center+FVector2D(-5,0)};
			for (int32 Index=0;Index<4;++Index) { Line(Points[Index],Points[(Index+1)%4],OpponentColor,2.f); }
		}
	}
	if (bShuttle)
	{
		const FVector2D Center = MarkerPoint(TrackedShuttle->GetActorLocation());
		Ring(Center,3.5f,FirstPersonWarning);
		Panel(Center.X-1.5f,Center.Y-1.5f,3,3,FLinearColor::White);
	}
	Panel(X+13,Y+293,6,6,FirstPersonAccent); Label(TEXT("나"),X+24,Y+290,.75f,FirstPersonAccent,65);
	Label(TEXT("상대"),X+112,Y+290,.75f,OpponentColor,65); Ring(FVector2D(X+102,Y+296),3.f,OpponentColor);
	Ring(FVector2D(X+16,Y+317),3.f,FirstPersonWarning); Label(TEXT("셔틀콕"),X+24,Y+312,.75f,FirstPersonWarning,72);
	Ring(FVector2D(X+102,Y+317),4.f,LandingColor); Label(TEXT("예상 낙하"),X+112,Y+312,.75f,LandingColor,77);
#if !UE_BUILD_SHIPPING
	if (!bMinimapTestReported && Players == 2 && bShuttle && bLanding && FParse::Param(FCommandLine::Get(), TEXT("BadmintonTimingTest")))
	{
		UE_LOG(LogTemp,Display,TEXT("BADMINTON_MINIMAP_TEST PASS players=2 shuttle=1 landing=1 localSide=%d"),LocalSide);
		bMinimapTestReported = true;
	}
#endif
}
