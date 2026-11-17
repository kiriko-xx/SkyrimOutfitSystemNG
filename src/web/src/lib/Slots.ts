let slotNames: Record<number, string> = {
    0: "Head",
    1: "Hair",
    2: "Body",
    3: "Hands",
    4: "Forearms",
    5: "Amulet",
    6: "Ring",
    7: "Feet",
    8: "Calves",
    9: "Shield",
    10: "Tail",
    11: "LongHair",
    12: "Circlet",
    13: "Ears",
    14: "Mod Mouth",
    15: "Mod Neck",
    16: "Mod Chest Primary",
    17: "Mod Back",
    18: "Mod Misc 1",
    19: "Mod Pelvis Primary",
    20: "Decapitate Head",
    21: "Decapitate",
    22: "Mod Pelvis Secondary",
    23: "Mod Leg Right",
    24: "Mod Leg Left",
    25: "Mod Face Jewelry",
    26: "Mod Chest Secondary",
    27: "Mod Shoulder",
    28: "Mod Arm Left",
    29: "Mod Arm Right",
    30: "Mod Misc 2",
    31: "FX01",
}

let builtInPolicyNames: Record<string, string> = {
    "XXXX": "Never show anything",
    "XXXE": "If outfit and equipped, show equipped",
    "XXXO": "Require equipped",
    "XXOX": "If only outfit, show outfit",
    "XXOE": "If only outfit, show outfit. If both, show equipped",
    "XXOO": "Always use outfit",
    "XEXX": "If only equipped, show equipped",
    "XEXE": "If equipped, show equipped",
    "XEXO": "Passthrough",
    "XEOX": "If only equipped, show equipped. If only outfit, show outfit",
    "XEOE": "If only equipped, show equipped. If only outfit, show outfit. If both, show equipped",
    "XEOO": "If only equipped, show equipped. If only outfit, show outfit. If both, show outfit",
}

let autoswitchLabels: Record<number, string> = {
    12: "Combat",
    0: "World",
    3: "World (Snowy)",
    6: "World (Rainy)",
    9: "City",
    10: "City (Snowy)",
    11: "City (Rainy)",
    1: "Towns",
    4: "Towns (Snowy)",
    7: "Towns (Rainy)",
    2: "Dungeons",
    5: "Dungeons (Snowy)",
    8: "Dungeons (Rainy)",
}

let autoswitchLabelsOrdered: number[] = [
    12, 0, 3, 6, 9, 10, 11, 1, 4, 7, 2, 5, 8
]

function masksConflict(first: number, second: number): boolean {
    return (first & second) != 0;
}

function getSlots(mask: number): string[] {
    return Object.keys(slotNames)
        .map((value): [number, string] => [parseInt(value), slotNames[parseInt(value)]])
        .filter((item) => ((2 ** item[0]) & mask) != 0)
        .map((item) => item[1]);
}

export { slotNames, builtInPolicyNames, masksConflict, getSlots, autoswitchLabels, autoswitchLabelsOrdered };