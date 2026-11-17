Scriptname SkyOutSysMCM extends Quest Hidden

;
; The SkyUI MCM of the original mod has been retired: settings, the outfit
; browser and the actor assignments now live in the native SKSE Menu Framework
; menu ("Skyrim Outfit System" section, default key F1).
;
; The quest record still ships (removing it would break existing saves), so this
; script keeps the two properties the VMAD writes and does nothing else.
;

String[] Property Pages Auto
String   Property modName Auto

Event OnInit()
    Debug.Trace("Skyrim Outfit System: the SkyUI MCM has been replaced by the native menu.")
EndEvent

;
; The original script exposed these three helpers to other mods. They are kept
; so existing third-party calls keep compiling and working even though the MCM
; UI itself is gone. Keep the constant in sync with SkyrimOutfitSystemNG's
; version (0xMMmmpp).
;
Int Function GetModVersion() Global
    Return 0x01030000
EndFunction

Int Function GetVersion()
    Return GetModVersion()
EndFunction

SkyOutSysQuickslotManager Function GetQuickslotManager() Global
    Return Quest.GetQuest("SkyrimOutfitSystemQuickslotManager") as SkyOutSysQuickslotManager
EndFunction
