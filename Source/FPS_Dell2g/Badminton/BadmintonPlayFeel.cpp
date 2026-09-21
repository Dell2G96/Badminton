#include "BadmintonPlayFeel.h"
#include "BadmintonShuttle.h"
#include "BadmintonRules.h"
#include "BadmintonCamera.h"
namespace Badminton
{
	const TCHAR* ContactHintText(EContactHint Hint)
	{
		switch (Hint)
		{
		case EContactHint::Ready: return TEXT("타격 준비 완료 / 클릭으로 타격");
		case EContactHint::SelectShot: return TEXT("Q 드롭 / E 클리어 / R 헤어핀 / Space 리시브");
		case EContactHint::WaitReturn: return TEXT("셔틀콕이 내 코트로 오기를 기다리세요");
		case EContactHint::Recovering: return TEXT("동작 회복 중 / 잠시 기다리세요");
		case EContactHint::LowStamina: return TEXT("스태미나 부족 / E 또는 Space 사용");
		case EContactHint::TooFar: return TEXT("너무 멉니다 / 셔틀콕 쪽으로 이동하세요");
		case EContactHint::TooHigh: return TEXT("너무 높습니다 / 내려올 때까지 기다리세요");
		case EContactHint::TooLow: return TEXT("너무 낮습니다 / Space 리시브를 시도하세요");
		case EContactHint::AimMiss: return TEXT("조준이 빗나갔습니다 / 라켓을 셔틀콕에 맞추세요");
		case EContactHint::ServicePosition: return TEXT("자신의 서브 구역으로 이동하세요");
		case EContactHint::TooLate: return TEXT("너무 늦었습니다 / 다시 시도하세요");
		default: return TEXT("");
		}
	}
	EContactHint ContactGeometry(int32 Side, const FVector& Player, const FVector& Shuttle, const FBadmintonShotParameters& Params)
	{
		if (Shuttle.ContainsNaN() || Player.ContainsNaN() || Side < 0 || Side > 1 || Shuttle.X * ForwardSign(Side) > 0.) { return EContactHint::WaitReturn; }
		if (FVector::Dist2D(Player, Shuttle) > Params.Reach) { return EContactHint::TooFar; }
		if (Shuttle.Z < Params.MinimumHeight) { return EContactHint::TooLow; }
		if (Shuttle.Z > Params.MaximumHeight) { return EContactHint::TooHigh; }
		return EContactHint::Ready;
	}
	bool RacketAimMatches(EBadmintonShot Shot, const FVector& Direction, const FVector& ToShuttle)
	{
		if (Direction.ContainsNaN() || ToShuttle.ContainsNaN() || Direction.IsNearlyZero() || ToShuttle.IsNearlyZero()) { return false; }
		const float Angle = Shot == EBadmintonShot::Smash ? 12.f : 20.f;
		return FVector::DotProduct(Direction.GetSafeNormal(), ToShuttle.GetSafeNormal()) >= FMath::Cos(FMath::DegreesToRadians(Angle));
	}
	bool PredictContact(int32 Side, EBadmintonShot Shot, const FBadmintonShotParameters& Params,
		FVector Position, FVector Velocity, const FVector& Player, const FVector& PlayerVelocity, float& Seconds)
	{
		Seconds = 0.f;
		float Best = TNumericLimits<float>::Max();
		const float IdealHeight = Shot == EBadmintonShot::Smash ? 265.f : 235.f;
		for (int32 Step = 0; Step <= 300; ++Step)
		{
			const float Time = Step * .01f;
			// Forecast from the actual position, with room for movement during the final frozen interval.
			FBadmintonShotParameters ForecastParams = Params;
			ForecastParams.Reach = FMath::Max(40.f, Params.Reach - 20.f - FMath::Min(450.f, static_cast<float>(PlayerVelocity.Size2D())) * .12f);
			const FVector ToContact = Position - (Player + FVector(0, 0, 85));
			const FRotator Desired = ToContact.Rotation();
			const double Yaw = FMath::FindDeltaAngleDegrees(Side == 1 ? 180. : 0., Desired.Yaw);
			const FRotator ClosestView = CourtCameraRotation(Side, ApplyMouseLook(FVector2D::ZeroVector, FVector2D(Yaw, Desired.Pitch), 1.f));
			const bool bViewReachable = FVector::DotProduct(ClosestView.Vector(), ToContact.GetSafeNormal()) >= FMath::Cos(FMath::DegreesToRadians(8.f));
			if (Velocity.Z < 0. && bViewReachable && ContactGeometry(Side, Player, Position, ForecastParams) == EContactHint::Ready)
			{
				const float Score = FMath::Abs(Position.Z - IdealHeight) + .12f * FVector::Dist2D(Player, Position);
				if (Score < Best) { Best = Score; Seconds = Time; }
			}
			if (Position.Z < 5. && Velocity.Z < 0.) { break; }
			ABadmintonShuttle::AdvanceFlight(Position, Velocity, .01f);
		}
		return Best < TNumericLimits<float>::Max();
	}
	FVector DefensiveTarget(int32 Side, const FVector& Target)
	{
		return FVector(ForwardSign(Side) * FMath::Clamp(Target.X * ForwardSign(Side), 330., 480.), Target.Y * .45, 5.);
	}
	EBadmintonShot ChooseTacticalShot(float OpponentDepth, float Height, float Stamina, const UBadmintonShotData& Data)
	{
		if (OpponentDepth < 300.f) { return EBadmintonShot::Clear; }
		if (OpponentDepth > 490.f && Stamina >= Data.Drop.StaminaCost) { return EBadmintonShot::Drop; }
		if (Height >= Data.Smash.MinimumHeight + 15.f && Stamina >= Data.Smash.StaminaCost) { return EBadmintonShot::Smash; }
		return EBadmintonShot::Clear;
	}
	void FPracticeProgress::Contact(int32 RallyId, int32 ShotSequence, int32 Hitter, int32 LocalSide, EBadmintonShot Shot)
	{
		if (RallyId < Rally || (RallyId == Rally && ShotSequence <= Sequence)) { return; }
		if (RallyId != Rally) { Rally = RallyId; RallyHits = 0; LastOwnShot = EBadmintonShot::Serve; bCombo = false; }
		Sequence = ShotSequence;
		if (Shot != EBadmintonShot::Serve) { ++RallyHits; BestRally = FMath::Max(BestRally, RallyHits); }
		if (Hitter == LocalSide)
		{
			bCombo = LastOwnShot == EBadmintonShot::Drop && Shot == EBadmintonShot::Smash;
			LastOwnShot = Shot;
		}
	}
	FVector FPracticeProgress::Target(int32 Side) const
	{
		return FVector(ForwardSign(Side) * 450.f, ForwardSign(Side) * (Targets % 2 == 0 ? -150.f : 150.f), 5.f);
	}
	bool FPracticeProgress::Landing(int32 RallyId, int32 Winner, int32 Hitter, int32 LocalSide, EBadmintonShot Shot, const FVector& Position)
	{
		if (RallyId != Rally || RallyId <= FinishedRally) { return false; }
		FinishedRally = RallyId;
		if (Winner != LocalSide || Hitter != LocalSide || Shot == EBadmintonShot::Serve || !IsInCourt(Position)) { return false; }
		bool bReward = false;
		if (FVector::Dist2D(Position, Target(LocalSide)) <= 85.f) { ++Targets; bReward = true; }
		if (bCombo && Shot == EBadmintonShot::Smash) { ++ComboPoints; bReward = true; }
		return bReward;
	}
}
