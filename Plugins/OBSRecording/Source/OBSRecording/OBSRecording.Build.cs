// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OBSRecording : ModuleRules
{
	public OBSRecording(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",			//				
			"Engine",				//
			"UnrealEd",				// FEditorDelegates
			"DeveloperSettings",	//			
			"WebSockets",			// OBS IWebSocket
			"Json",                 //						
			"Slate",				// 저장 확인, 및 알림 창 띄위기 
			"SlateCore",			//	
			"DesktopPlatform",		// 폴더 선택 창
			"OpenSSL",				// 
			"ToolMenus",			// 툴바 메뉴 확장	
			"LevelEditor",			//	
		});
	}
}
