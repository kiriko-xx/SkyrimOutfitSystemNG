interface OutfitSystem {
    enabled: boolean;
    outfits: Outfit[];
    actorOutfitAssignments: Record<number, ActorOutfitAssignment>;
}

interface Outfit {
    name: string;
    isFavorite: boolean;
    armors: ArmorLocator[];
    slotPolicy: string;
    slotPolicies: Record<number, string>;
}

interface ActorOutfitAssignment {
    currentOutfitName: string;
    stateBasedAutoSwitchEnabled: boolean
    stateBasedOutfits: Record<number, string>;
}

interface ArmorLocator {
    rawFormId: number;
    localFormId: number;
    modName: string;
}

interface ArmorInfo {
    locator: ArmorLocator;
    name: string;
    mask: number;
}

interface ActorInfo {
    name: string;
}

interface PolicyInfo {
    value: string;
    ui_name: string;
    sort_order: number;
    advanced: boolean;
}


export type {
    Outfit,
    OutfitSystem,
    ActorOutfitAssignment,
    ArmorLocator,
    ArmorInfo,
    ActorInfo,
    PolicyInfo,
};
