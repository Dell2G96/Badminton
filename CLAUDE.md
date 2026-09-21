# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 프로젝트 개요

Unreal Engine 5.7 기반의 FPS 학습/실습 프로젝트. C++ 모듈은 `FPS_Dell2g` 하나(Runtime)뿐이며, 현재 캐릭터/플레이어 컨트롤러/게임모드 골격만 구현된 초기 단계다.

- 엔진 설치 경로: `D:\Game\UE_5.7` (`FPS_Dell2g.uproject`의 `EngineAssociation`은 `"5.7"`)
- `Content/`에는 아직 실제 에셋이 없다. Input Mapping Context / Input Action 등은 에디터에서 만들어야 한다.

## 자주 쓰는 명령

빌드 (에디터 타깃):
```
"D:\Game\UE_5.7\Engine\Build\BatchFiles\Build.bat" FPS_Dell2gEditor Win64 Development -Project="D:\_Projects\FPS_Dell2g\FPS_Dell2g.uproject" -WaitMutex
```

리빌드 / 클린:
```
"D:\Game\UE_5.7\Engine\Build\BatchFiles\Rebuild.bat" FPS_Dell2gEditor Win64 Development -Project="D:\_Projects\FPS_Dell2g\FPS_Dell2g.uproject"
"D:\Game\UE_5.7\Engine\Build\BatchFiles\Clean.bat"   FPS_Dell2gEditor Win64 Development -Project="D:\_Projects\FPS_Dell2g\FPS_Dell2g.uproject"
```

에디터 실행:
```
"D:\Game\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "D:\_Projects\FPS_Dell2g\FPS_Dell2g.uproject"
```

`.sln` / `Intermediate/ProjectFiles` 재생성 (파일 추가·삭제 후):
```
"D:\Game\UE_5.7\Engine\Binaries\Win64\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="D:\_Projects\FPS_Dell2g\FPS_Dell2g.uproject" -game -rocket -progress
```

자동화 테스트 프레임워크는 아직 도입되어 있지 않다. 필요해지면 `UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests <필터>" -unattended -nullrhi` 형태로 실행한다.

## 아키텍처

입력 처리의 주체가 Character가 아니라 **PlayerController**다. 이 프로젝트의 가장 중요한 구조적 결정이므로 새 입력을 추가할 때 관례를 따를 것.

- `Character/ShooterPlayerController` — Enhanced Input 전담. `BeginPlay`에서 `ShooterIMC`를 `UEnhancedInputLocalPlayerSubsystem`에 등록하고, `SetupInputComponent`에서 각 `UInputAction`을 `Input_*` 핸들러에 바인딩한다. 핸들러는 `GetCharacter()` / `GetPawn()`을 통해 폰을 조작한다(예: 크라우치는 `UCharacterMovementComponent::bWantsToCrouch` 토글). `AShooterCharacter::SetupPlayerInputComponent`는 의도적으로 비어 있다.
  - `ShooterIMC`, `MoveAction`, `LookAction`, `CrouchAction`, `JumpAction`은 모두 `EditAnywhere`(카테고리 `FPS|Input`)이며 C++에서 로드하지 않는다. **반드시 블루프린트 서브클래스에서 에셋을 할당**해야 동작한다.
- `Character/ShooterCharacter` — 1인칭 카메라 리그. `SpringArm`(TargetArmLength 0, `bUsePawnControlRotation=true`, 카메라 랙 사용) → `FirstPersonCamera` → `Mesh1P` 순으로 부착된다. 회전은 스프링암이 담당하므로 카메라 자체는 `bUsePawnControlRotation=false`. `Mesh1P`는 `bOnlyOwnerSee`, 기본 `GetMesh()`(3인칭 바디)는 `bOwnerNoSee`로 1인칭/3인칭 메시를 분리한다. 크라우치는 생성자에서 `MovementState.bCanCrouch`로 활성화.
- `Game/ShooterGameModeBase` — `AGameMode`(`AGameModeBase` 아님) 상속. 아직 비어 있고, `Config/DefaultEngine.ini`에 기본 게임모드로 등록되어 있지도 않다(`GameDefaultMap`은 엔진 템플릿 OpenWorld). 실제로 쓰려면 프로젝트 설정 또는 맵의 World Settings에서 지정해야 한다.

## 코드 규약

- 모든 클래스 접두사는 `Shooter`, export 매크로는 `FPS_DELL2G_API`.
- 컴포넌트/에셋 참조는 원시 포인터가 아니라 `TObjectPtr<>`, 전방 선언(`class U...`)을 헤더에서 사용하고 실제 include는 `.cpp`에 둔다.
- 널 체크는 `IsValid()`, 확정적으로 유효해야 하는 캐스팅은 `CastChecked<>`.
- 새 모듈 의존성은 `Source/FPS_Dell2g/FPS_Dell2g.Build.cs`의 `PublicDependencyModuleNames`에 추가한다(현재: Core, CoreUObject, Engine, InputCore, EnhancedInput).
