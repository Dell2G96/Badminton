#pragma once

#include "CoreMinimal.h"

// obs-websocket 인증 계산에 쓰는 SHA-256 유틸.
namespace OBSSha256
{
	void Compute(const uint8* Data, int32 Size, uint8 OutDigest[32]);
	
	// 문자열 UTF-8 바이트로 변환 
	FString HashUtf8ToBase64(const FString& Input);
	
	// 문자열을 UTF-8로 해시한 뒤 소문자 16진 문자열로 변환
	FString HashUtf8ToHex(const FString& Input);

	
}
