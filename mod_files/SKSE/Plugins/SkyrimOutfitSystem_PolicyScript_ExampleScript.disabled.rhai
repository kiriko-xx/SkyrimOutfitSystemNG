// This is an example of a custom slot policy script.
// The `.disabled` before the extension will hide this
// from the game. Rename the file to simply 
// `SkyrimOutfitSystem_PolicyScript_ExampleScript.rhai` to
// have it appear in-game. 
// 
// You can add more scripts next to this one by
// creating new scripts where you replace 
// `ExampleScript` in the name with some other names.
// You can only have a maximum of 127 scripts.

// If your script has a `name` variable at global scope
// this will be the name that shows in the MCM menu.
let name = "Demo Script";

// This is the core function of the script.
// It must return a 32-length array, where
// the element at each index can be one of:
// 
//   1) the item at the same index from the `equipped` array
//   2) the item at the same index from the `outfit` array
//   3) the rhai unit type ()
// 
// The `equipped` and `outfit` array contain
// the items at each of the 32 slots, or the
// rhai unit type if the slot is empty.
//
// Print statements will go the the outfit system log file.
fn select_armors(ctx, equipped, outfit) {
    print(outfit);
    return outfit;
}

// For reference, the indices of the various slots are as follows:
// Head - 0
// Hair - 1
// Body - 2
// Hands - 3
// Forearms - 4
// Amulet - 5
// Ring - 6
// Feet - 7
// Calves - 8
// Shield - 9
// Tail - 10
// LongHair - 11
// Circlet - 12
// Ears - 13
// ModMouth - 14
// ModNeck - 15
// ModChestPrimary - 16
// ModBack - 17
// ModMisc1 - 18
// ModPelvisPrimary - 19
// DecapitateHead - 20
// Decapitate - 21
// ModPelvisSecondary - 22
// ModLegRight - 23
// ModLegLeft - 24
// ModFaceJewelry - 25
// ModChestSecondary - 26
// ModShoulder - 27
// ModArmLeft - 28
// ModArmRight - 29
// ModMisc2 - 30
// FX01 - 31
