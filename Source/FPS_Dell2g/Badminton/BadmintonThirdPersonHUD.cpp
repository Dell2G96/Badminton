#include "BadmintonHUD.h"

#include "BadmintonAttributeSet.h"
#include "BadmintonGameState.h"
#include "BadmintonPlayerController.h"
#include "BadmintonPlayerState.h"
#include "BadmintonThirdPerson.h"
#include "Engine/Canvas.h"

void ABadmintonHUD::DrawThirdPersonHUD(const ABadmintonGameState* Match)
{
	const auto* Controller = Cast<ABadmintonPlayerController>(PlayerOwner);
	const auto* State = PlayerOwner->GetPlayerState<ABadmintonPlayerState>();
	UIScale = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 720.f);
	const float Width = Canvas->ClipX / UIScale, Height = Canvas->ClipY / UIScale, Center = Width * .5f;
	const FLinearColor Dark(.025f, .04f, .065f, .9f), Accent(.25f, .95f, .77f), Muted(.7f, .77f, .83f), Yellow(1.f, .72f, .28f);
	if (State->CourtSide < 0 || State->CourtSide > 1) { Label(TEXT("코트에 입장하는 중..."), Center, 30, 1.2f, Accent, 400, true); return; }
	DrawShuttleTracking(Match);
	DrawAimPreview();
	float Quality, Seconds;
	FVector Contact;
	const auto Hint = Controller->GetThirdPersonContact(Quality, Seconds, Contact);
	const bool bReady = Hint == Badminton::EContactHint::Ready;
	const bool bPlay = Match->Phase == EBadmintonPhase::Rally || Match->Phase == EBadmintonPhase::ReadyToServe;
	if (!Contact.IsZero() && Seconds > -Badminton::ThirdPersonTimingWindow && Seconds < 3.f)
	{
		const FVector Ground(Contact.X, Contact.Y, 5.f);
		const float Radius = FMath::Lerp(35.f, 95.f, FMath::Clamp(Seconds / 1.2f, 0.f, 1.f));
		for (int32 Index = 0; Index < 40; ++Index)
		{
			const float A = Index * 2.f * PI / 40.f, B = (Index + 1) * 2.f * PI / 40.f;
			FVector2D Start, End;
			if (ProjectFirstPerson(Ground + FVector(FMath::Cos(A), FMath::Sin(A), 0) * Radius, Start)
				&& ProjectFirstPerson(Ground + FVector(FMath::Cos(B), FMath::Sin(B), 0) * Radius, End))
			{ DrawLine(Start.X, Start.Y, End.X, End.Y, bReady ? Accent : Yellow, 3.f * UIScale); }
		}
		FVector2D Screen;
		if (ProjectFirstPerson(Ground, Screen)) { Label(TEXT("타격 위치"), Screen.X / UIScale, Screen.Y / UIScale + 12, .9f, bReady ? Accent : Yellow, 115, true); }
	}
	Panel(16, 16, 300, 60, Dark);
	Label(TEXT("3인칭 / 타이밍 플레이"), 28, 26, 1.15f, Accent, 276);
	Label(Controller->GetAutomaticDropShot() == EBadmintonShot::Hairpin
		? TEXT("Q: 전위 헤어핀 / 네트 앞에 짧게 넘기기")
		: TEXT("Q: 후위 드롭 / 상대 전위로 떨어뜨리기"), 28, 52, .83f, Muted, 276);
	Panel(Center - 110, 16, 220, 60, Dark);
	Label(TEXT("나 : 상대"), Center, 24, .85f, Muted, 196, true);
	Label(FString::Printf(TEXT("%d : %d"), Match->GetScore(State->CourtSide), Match->GetScore(1 - State->CourtSide)), Center, 42, 1.6f, FLinearColor::White, 196, true);
	Panel(Width - 218, 16, 202, 60, Dark);
	Label(FString::Printf(TEXT("스태미나 %.0f"), State->GetAttributes()->GetStamina()), Width - 206, 27, .95f, Accent, 178);
	Panel(Width - 206, 58, 178, 5, Muted);
	Panel(Width - 206, 58, 178 * FMath::Clamp(State->GetAttributes()->GetStamina() / FMath::Max(1.f, State->GetAttributes()->GetMaxStamina()), 0.f, 1.f), 5, Accent);
	FString Status;
	if (Match->Phase == EBadmintonPhase::WaitingForReady || Match->Phase == EBadmintonPhase::WaitingForPlayers) { Status = TEXT("Enter / 경기 준비"); }
	else if (Match->Phase == EBadmintonPhase::MatchFinished) { Status = Match->WinnerSide == State->CourtSide ? TEXT("승리! / Enter 재경기") : TEXT("패배 / Enter 재경기"); }
	else if (Match->Phase == EBadmintonPhase::ReadyToServe) { Status = State->CourtSide == Match->ServingSide ? TEXT("내 서브 / 왼쪽 클릭") : TEXT("상대 서브 / 준비하세요"); }
	else if (Match->Phase == EBadmintonPhase::RallyComplete) { Status = State->CourtSide == Match->ServingSide ? TEXT("내 득점") : TEXT("상대 득점"); }
	if (!Status.IsEmpty()) { Panel(Center - 230, 90, 460, 38, Dark); Label(Status, Center, 100, 1.15f, Accent, 430, true); }
	if (bPlay)
	{
		const float TimingCenter = 220.f;
		if (Match->Phase == EBadmintonPhase::Rally)
		{
			const EBadmintonShot QuickShots[] = {EBadmintonShot::Clear, EBadmintonShot::Drop, EBadmintonShot::Receive};
			const TCHAR* Keys[] = {TEXT("E"), TEXT("Q"), TEXT("Space")};
			for (int32 Index = 0; Index < 3; ++Index)
			{
				float ShotQuality, ShotSeconds;
				FVector ShotContact;
				const auto ShotHint = Controller->GetThirdPersonContactForShot(QuickShots[Index], ShotQuality, ShotSeconds, ShotContact);
				const bool bCanHit = ShotHint == Badminton::EContactHint::Ready;
				const TCHAR* Cue = bCanHit ? TEXT("지금!") : ShotHint == Badminton::EContactHint::LowStamina ? TEXT("기력 부족")
					: ShotHint == Badminton::EContactHint::TooFar ? TEXT("이동") : ShotHint == Badminton::EContactHint::TooLow || ShotHint == Badminton::EContactHint::TooLate ? TEXT("늦음") : TEXT("대기");
				const float Left = 20.f + Index * 135.f;
				Panel(Left, Height - 207, 130, 31, bCanHit ? Accent.CopyWithNewOpacity(.9f) : Dark);
				Label(FString::Printf(TEXT("%s / %s"), Keys[Index], Cue), Left + 65, Height - 200, .85f, bCanHit ? Dark : Muted, 120, true);
			}
		}
		Panel(20, Height - 171, 400, 92, Dark);
		const auto Selected = Controller->GetSelectedShot();
		const bool bServe = Match->Phase == EBadmintonPhase::ReadyToServe && State->CourtSide == Match->ServingSide;
		const TCHAR* HitKey = bServe || Selected == EBadmintonShot::Smash ? TEXT("왼쪽 클릭")
			: Badminton::IsDropFamily(Selected) ? TEXT("Q") : Selected == EBadmintonShot::Clear ? TEXT("E") : TEXT("Space");
		Label(FString::Printf(TEXT("%s / %s 타격"), bServe ? TEXT("서브") : Badminton::ShotLabel(Selected), HitKey), TimingCenter, Height - 160, 1.1f, Accent, 374, true);
		const FString HintText = bReady ? Quality >= .99f ? TEXT("완벽한 타이밍 / 지금 타격!") : TEXT("타격 가능 / 지금 치세요")
			: Hint == Badminton::EContactHint::TooHigh ? TEXT("내려오는 공을 기다리세요 / 초록색이면 타격") : Badminton::ContactHintText(Hint);
		Label(HintText, TimingCenter, Height - 132, .95f, bReady ? Accent : Yellow, 374, true);
		const float X = TimingCenter - 170, Y = Height - 101;
		Panel(X, Y, 340, 9, Muted.CopyWithNewOpacity(.3f));
		const float ReadyHalfWidth = Badminton::ThirdPersonTimingWindow / .4f * 170.f;
		const float PerfectHalfWidth = Badminton::ThirdPersonPerfectWindow / .4f * 170.f;
		Panel(TimingCenter - ReadyHalfWidth, Y, ReadyHalfWidth * 2.f, 9, Accent.CopyWithNewOpacity(.4f));
		Panel(TimingCenter - PerfectHalfWidth, Y, PerfectHalfWidth * 2.f, 9, Accent);
		if (!Contact.IsZero()) { Panel(TimingCenter - FMath::Clamp(Seconds / .4f, -1.f, 1.f) * 170.f - 2, Y - 4, 4, 17, FLinearColor::White); }
	}
	const FString Feedback = Controller->GetTimingMessage();
	if (!Feedback.IsEmpty()) { Panel(Center - 250, 137, 500, 34, Dark); Label(Feedback, Center, 145, 1.f, Yellow, 470, true); }
	Panel(0, Height - 65, Width, 65, Dark);
	Label(TEXT("즉시 타격: Q 드롭·헤어핀 (위치 자동) | E 클리어 | Space 리시브 | 스매시: 2 선택 후 왼쪽 클릭"), Center, Height - 55, 1.f, Accent, Width - 40, true);
	Label(TEXT("WASD 이동 | Shift 대시 | 마우스 목표 이동 4배 | -/+ 감도 | 가운데 클릭 목표 초기화"), Center, Height - 28, .87f, Muted, Width - 40, true);
}
