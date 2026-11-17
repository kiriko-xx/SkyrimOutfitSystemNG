//
// Created by m on 10/8/2022.
//

#ifndef SKYRIMOUTFITSYSTEMSE_INCLUDE_RE_REAUGMENTS_H
#define SKYRIMOUTFITSYSTEMSE_INCLUDE_RE_REAUGMENTS_H

namespace RE {
    class ActorWeightModel;

    class IItemChangeVisitorAugment {
    public:
        inline static constexpr auto RTTI = RTTI_InventoryChanges__IItemChangeVisitor;

        virtual ~IItemChangeVisitorAugment(){};// 00

        enum VisitorReturn : std::uint32_t {
            kStop = 0,
            kContinue = 1
        };

        // add
        // The vtable layout has to match RE::InventoryChanges::IItemChangeVisitor exactly; otherwise the
        // engine reads garbage out of the return register on the slots this augment does not use.
        virtual VisitorReturn Visit(InventoryEntryData* a_entryData) = 0;// 01
        virtual bool ShouldVisit(InventoryEntryData* a_entryData, TESBoundObject* a_object) { return true; };// 02 - { return 1; }
        virtual VisitorReturn Unk_03(InventoryEntryData* a_entryData, void* a_arg2, bool* a_arg3) { *a_arg3 = true; return Visit(a_entryData); };// 03
    };
    static_assert(sizeof(IItemChangeVisitorAugment) == 0x8);

    namespace InventoryChangesAugments {
        void ExecuteVisitor(RE::InventoryChanges* thisPtr, RE::InventoryChanges::IItemChangeVisitor* a_visitor);
        void ExecuteAugmentVisitor(RE::InventoryChanges* thisPtr, RE::IItemChangeVisitorAugment* a_visitor);
        void ExecuteVisitorOnWorn(RE::InventoryChanges* thisPtr, RE::InventoryChanges::IItemChangeVisitor* a_visitor);
        void ExecuteAugmentVisitorOnWorn(RE::InventoryChanges* thisPtr, RE::IItemChangeVisitorAugment* a_visitor);
    }// namespace InventoryChangesAugments

    namespace AIProcessAugments {
        enum class Flag : std::uint8_t {
            kNone = 0,
            kUnk01 = 1 << 0,
            kUnk02 = 1 << 1,
            kUnk03 = 1 << 2,
            kDrawHead = 1 << 3,
            kMobile = 1 << 4,
            kReset = 1 << 5
        };
        void SetEquipFlag(RE::AIProcess* thisPtr, Flag a_flag);
        void UpdateEquipment(RE::AIProcess* thisPtr, Actor* a_actor);
    }// namespace AIProcessAugments

    namespace TESObjectARMOAugments {
        bool ApplyArmorAddon(const RE::TESObjectARMO* thisPtr, TESRace* a_race, ActorWeightModel* a_model, bool a_isFemale);
        bool TestBodyPartByIndex(const RE::TESObjectARMO* thisPtr, std::uint32_t a_index);
    }// namespace TESObjectARMOAugments

    namespace BipedAnimAugments {
        // Detaches the slot node and clears partClone; passing true also releases
        // the weapon manager. Used to force a fresh armour-addon build so a new
        // .tri morph target is picked up.
        void ClearBipedPart(RE::BipedAnim* a_this, RE::BIPOBJECT* a_object, bool a_arg3, std::int32_t a_arg4);
    }// namespace BipedAnimAugments

    namespace BIPED_OBJECTS_META {
        inline constexpr std::uint32_t kFirstSlot = RE::BIPED_OBJECTS::kHead;
        inline constexpr std::uint32_t kNumSlots = RE::BIPED_OBJECTS::kEditorTotal;
    }// namespace BIPED_OBJECTS_META
}// namespace RE

#endif//SKYRIMOUTFITSYSTEMSE_INCLUDE_RE_REAUGMENTS_H
