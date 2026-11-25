Scriptname SkyrimOutfitSystemNativeFuncs Hidden

;
; Information on the outfit system:
;
;  - Outfit names are not sorted; functions to retrieve a list of outfits 
;    will return an unsorted list. List order will remain consistent until 
;    the list changes (i.e. the std::unordered_map's iterators invalidate).
;
;  - Inventory lists are unsorted but likely to remain similarly consistent.
;
;  - Armor search results are not intentionally sorted, but will most 
;    likely be returned in form ID order.
;
;  - Body part listings are sorted by body part index.
;
;  - Outfit names shouldn't be longer than the constant returned by a call 
;    to GetOutfitNameMaxLength(); currently this limit is due to how SKSE 
;    serializes and loads strings in the co-save.
;
;  - Outfit names shouldn't be blank; a blank outfit name refers to "not 
;    using an outfit."
;

Int Function GetOutfitNameMaxLength() Global Native

Armor[] Function GetCarriedArmor (Actor akSubject) Global Native
Armor[] Function GetWornItems    (Actor akSubject) Global Native
        Function RefreshArmorFor (Actor akSubject) Global Native ; force akSubject to update their ArmorAddons
        Function RefreshArmorForAllConfiguredActors () Global Native ; force all known actors to update their ArmorAddons
;
; Searching for actors. Used in menus.
;
Actor[] Function ActorNearPC  () Global Native

;
; Search through all armor forms defined in the game (excluding templated ones). 
; Filter by name or require the "playable" flag.
;
         Function PrepArmorSearch           (String asNameFilter = "", Bool abMustBePlayable = True) Global Native
Armor[]  Function GetArmorSearchResultForms () Global Native
String[] Function GetArmorSearchResultNames () Global Native
         Function ClearArmorSearch          () Global Native

;
; Given an outfit, generate string and form arrays representing which body slots 
; are taken by which armors.
;
         Function PrepOutfitBodySlotListing           (String asOutfitName) Global Native
Armor[]  Function GetOutfitBodySlotListingArmorForms  () Global Native
String[] Function GetOutfitBodySlotListingArmorNames  () Global Native
Int[]    Function GetOutfitBodySlotListingSlotIndices () Global Native
         Function ClearOutfitBodySlotListing          () Global Native

;
; String functions, to be synchronized with CobbAPI:
;
String[] Function NaturalSort_ASCII          (String[] asStrings, Bool abDescending = False) Global Native
Armor[]  Function NaturalSortPairArmor_ASCII (String[] asStrings, Armor[] akForms, Bool abDescending = False) Global Native ; uses asStrings to sort akForms
;
Int      Function HexToInt32 (String asHex) Global Native
String   Function ToHex      (Int aiValue, Int aiDigits) Global Native

;
; Functions for working with the armor-addon override service.
;
         Function AddArmorToOutfit  (String asOutfitName, Armor akArmor) Global Native
Bool     Function ArmorConflictsWithOutfit (Armor akTest, String asOutfitName) Global Native
         Function CreateOutfit      (String asOutfitName) Global Native
         Function DeleteOutfit      (String asOutfitName) Global Native
Armor[]  Function GetOutfitContents (String asOutfitName) Global Native
Bool     Function GetOutfitFavoriteStatus(String asOutfitName) Global Native
         Function SetOutfitFavoriteStatus(String asOutfitName, Bool abFavorite) Global Native
String[] Function BodySlotPolicyNamesForOutfit(String asOutfitName) Global Native
         Function SetBodySlotPoliciesForOutfit(String asOutfitName, Int aiSlot, String aiPolicyCode) Global Native
         Function SetAllBodySlotPoliciesForOutfit(String asOutfitName, String aiPolicyCode) Global Native
         Function SetBodySlotPolicyToDefaultForOutfit(String asOutfitName) Global Native
String[] Function GetAvailablePolicyNames() Global Native
String[] Function GetAvailablePolicyCodes() Global Native
String   Function GetSelectedOutfit (Actor actor) Global Native
Bool     Function IsEnabled         () Global Native
String[] Function ListOutfits       (Bool favoritesOnly = False) Global Native
         Function RemoveArmorFromOutfit (String asOutfitName, Armor akArmor) Global Native
         Function RemoveConflictingArmorsFrom (Armor akTest, String asOutfitName) Global Native
Bool     Function RenameOutfit      (String asOutfitName, String asRenameTo) Global Native
Bool     Function OutfitExists      (String asOutfitName) Global Native
         Function OverwriteOutfit   (String asOutfitName, Armor[] akArmors) Global Native
         Function SetEnabled        (Bool abEnabled) Global Native
         Function SetSelectedOutfit (Actor actor, String asOutfitName) Global Native
         Function AddActor (Actor akSubject) Global Native
         Function RemoveActor (Actor akSubject) Global Native
Actor[]  Function ListActors() Global Native

         Function SetLocationBasedAutoSwitchEnabled (Actor actor, Bool abEnabled) Global Native
Bool     Function GetLocationBasedAutoSwitchEnabled (Actor actor) Global Native
Int[]    Function GetAutoSwitchStateArray () Global Native
Int      Function IdentifyStateType (Actor actor) Global Native
         Function SetOutfitUsingState (Actor actor) Global Native
         Function SetOutfitUsingStateForAllConfiguredActors () Global Native
         Function SetStateOutfit (Actor actor, Int aiStateType, String asOutfitName) Global Native
         Function UnsetStateOutfit (Actor actor, Int aiStateType) Global Native
String   Function GetStateOutfit (Actor actor, Int aiStateType) Global Native
Bool     Function ExportSettings () Global Native
Bool     Function ImportSettings () Global Native

         Function NotifyCombatStateChanged (Actor actor) Global Native

;
; NG additions (CommonLibSSE-NG build).
;
; RandomOutfit picks a random outfit (optionally favourites only), assigns it to
; the actor and refreshes the appearance. It returns the name of the chosen
; outfit, or an empty string when there was nothing to pick.
;
String   Function RandomOutfit (Actor akSubject, Bool abFavoritesOnly = False) Global Native
Int      Function GetOutfitCount () Global Native
String   Function GetOutfitNameByIndex (Int aiIndex, Bool abFavoritesOnly = False) Global Native
String[] Function SearchOutfits (String asQuery, Bool abFavoritesOnly = False) Global Native

; NOTE: the former generic perception API (IsPerceptionEnabled,
; SetPerceptionEnabled, GetPerceivedArmor) has been removed. Compatibility with
; mods that inspect the worn equipment is handled inside the plugin and is
; configured in Data\SKSE\Plugins\SkyrimOutfitSystemNG.ini under [Compatibility].

;
; Quick picker: a small standalone window (search + paging) that the
; "Change Outfit" lesser power and the quick-picker hotkey open.
; OpenQuickPicker returns False when the SKSE Menu Framework is not installed.
;
Bool     Function OpenQuickPicker () Global Native
         Function CloseQuickPicker () Global Native
Bool     Function IsQuickPickerVisible () Global Native

;
; Quickslot spell behaviour: False (default) opens the quick picker window,
; True rolls a random outfit. The ini key is [RandomOutfit] QuickSlotRandom.
;
Bool     Function IsQuickSlotRandom () Global Native
         Function SetQuickSlotRandom (Bool abRandom) Global Native

