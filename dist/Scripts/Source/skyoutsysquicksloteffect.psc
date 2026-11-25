Scriptname SkyOutSysQuickslotEffect extends activemagiceffect

;
; The quickslot spell no longer builds a UIListMenu: outfit browsing and
; searching live in a native popup window (SKSE Menu Framework), which is not
; limited to the ~100 pages that a Papyrus list menu could show.
;
; Casting this spell opens that window. Tick "The Change Outfit power rolls a
; random outfit" in the settings menu (or set [RandomOutfit] QuickSlotRandom=on)
; to roll a random outfit instead.
;

; Kept so that the properties stored in the ESP keep resolving.
Bool Property RandomOnCast = False Auto
Bool Property FavoritesOnlyRandom = True Auto

Event OnEffectStart(Actor akCaster, Actor akTarget)
    Actor subject = akCaster
    If subject == None
        subject = Game.GetPlayer()
    EndIf

    If SkyrimOutfitSystemNativeFuncs.IsQuickSlotRandom()
        String chosen = SkyrimOutfitSystemNativeFuncs.RandomOutfit(subject, FavoritesOnlyRandom)
        If chosen == ""
            Debug.Notification("Skyrim Outfit System: there is no outfit to pick from.")
        Else
            Debug.Notification("Outfit: " + chosen)
        EndIf
    Else
        If !SkyrimOutfitSystemNativeFuncs.OpenQuickPicker()
            Debug.Notification("Skyrim Outfit System: install SKSE Menu Framework to use the quick picker.")
        EndIf
    EndIf
EndEvent
