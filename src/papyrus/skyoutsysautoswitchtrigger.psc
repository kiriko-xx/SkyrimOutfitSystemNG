Scriptname SkyOutSysAutoSwitchTrigger extends ReferenceAlias

; Since 1.4.4 location / state switching is detected natively by the plugin
; (TESActorLocationChangeEvent + TESFastTravelEndEvent + a two second input
; heartbeat for weather changes). This alias is kept so that existing saves and
; the ESP stay valid, but it no longer polls on OnLocationChange.
Event OnLocationChange(Location akOldLoc, Location akNewLoc)
EndEvent
