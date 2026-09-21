#include "OBSSha256.cpp"

#include "Misc/Base64.h"

THIRD_PARTY_INCLUDES_START
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

namespace OBSSha256
{
	// 입력 바이트열의 SHA_256 해시 를 계산
	void Compute(const uint8* Data, int32 Size, uint8 OutDigest[32])
	{
		// Size가 0이어도 OpenSSl은 빈 입력의 정상 해시를 반환
		SHA256(Data, static_cast<size_t>(FMath::Max(Size,0)), OutDigest);
	}
	
	// 문자열을 UTF-8로 변환 하여 해시
	// TCHAR 그대로 넘기면 인증이 실패한다
	static void HashUtf8(const FString& Input, uint8 OutDigest[32])
	{
		const FTCHARToUTF8 Utf8(*Input);
		Compute(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), OutDigest);
	}
	
	// UTF-8 해시 결과를 Base64 문자열로 반환
	// OBS-WebSocket 인증 시 위 형태 필요
	FString HashUtf8ToBase64(const FString& Input)
	{
		uint8 Digest[32];
		HashUtf8(Input, Digest);
		
		return FBase64::Encode(Digest, sizeof(Digest));
	}
	
	// UTF-8 해시 결과를 소문자 16진 문자열로 반환
	FString HashUtf8ToHex(const FString& Input)
	{
		uint8 Digest[32];
		HashUtf8(Input, Digest);
		
		FString Hex;
		Hex.Reserve(64);
		for (int32 Index = 0; Index < 32; ++Index)
		{
			Hex += FString::Printf(TEXT("%02x"), Digest[Index]);
		}
		return Hex;
	}
}
