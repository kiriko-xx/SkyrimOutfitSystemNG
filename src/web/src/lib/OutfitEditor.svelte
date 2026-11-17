<script lang="ts">
    import type {
        Outfit,
        ArmorInfo,
        ArmorLocator,
        PolicyInfo,
    } from "./APITypes";
    import ArmorSearch from "./ArmorSearch.svelte";
    import {
        builtInPolicyNames,
        getSlots,
        masksConflict,
        slotNames,
    } from "./Slots";
    import * as _ from "lodash"; // Structural equality.
    import structuredClone from '@ungap/structured-clone';
    import { onMount, tick } from "svelte";

    export let outfit: Outfit;
    export let cancelAction: () => void;
    export let saveAction: (arg: Outfit, returnToHome: boolean) => void;
    
    let editsAreValid = true;

    let newSlotPolicySlot: number;
    let newSlotPolicyValue: string;

    let cache: Record<string, ArmorInfo> = {};
    async function dataForLocator(locator: ArmorLocator) {
        // @ts-ignore
        let params = new URLSearchParams(locator).toString();
        if (cache[params]) {
            return cache[params];
        }
        let response = await fetch("/api/armors?" + params);
        let response_json: ArmorInfo = await response.json();
        cache[params] = response_json;
        return response_json;
    }
    async function getSlotPolicies(): Promise<PolicyInfo[]> {
        let response = await fetch("/api/policies");
        let response_json: PolicyInfo[] = await response.json();
        return response_json;
    }
    let allArmorInfo: ArmorInfo[] = [];
    let allArmorInfoMap: Record<string, ArmorInfo> = {};
    let resolveLocator: (arg: ArmorLocator) => ArmorInfo | undefined;
    resolveLocator = (locator) => undefined;
    async function updateArmorInfo() {
        allArmorInfo = await Promise.all(
            outfit.armors.map((armor) => dataForLocator(armor))
        );
        allArmorInfoMap = allArmorInfo.reduce((map, item) => {
            // @ts-ignore
            let locSerialized = new URLSearchParams(item.locator).toString();
            map[locSerialized] = item;
            return map;
        }, allArmorInfoMap);
        resolveLocator = (locator) => {
            // @ts-ignore
            let key = new URLSearchParams(locator).toString();
            let value = allArmorInfoMap[key];
            return value;
        };
    }
    function typedClone<T>(value: T): T {
        return structuredClone(value);
    }
    onMount(async () => await updateArmorInfo());
    $: {
        // @ts-ignore
        outfit.armors; // Name it here so that Svelte knows we depend on it
        updateArmorInfo();
    }
    $: {
        editsAreValid = outfit.name.length > 0;
    }
</script>

{#if outfit}
    <div
        class="d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pt-3 pb-2 mb-3 border-bottom"
    >
        <h1 class="h2">
            Editing {outfit.name}
        </h1>
        <div class="btn-group me-2">
            <button
                type="button"
                class="btn btn-outline-danger btn-sm"
                on:click={() => cancelAction()}>Cancel</button
            >
            <button
                type="button"
                class="btn btn-outline-primary btn-sm {editsAreValid ? '' : 'disabled'}"
                on:click={() => saveAction(outfit, false)}>Apply</button
            >
            <button
                type="button"
                class="btn btn-outline-primary btn-sm {editsAreValid ? '' : 'disabled'}"
                on:click={() => saveAction(outfit, true)}>Save</button
            >
        </div>
    </div>
    <form>
        <div class="row">
            <div class="col">
                <div class="mb-3">
                    <label for="outfitNameInput" class="form-label"
                        >Outfit Name</label
                    >
                    <input
                        type="text"
                        class="form-control"
                        id="outfitNameInput"
                        bind:value={outfit.name}
                    />
                    <div id="nameHelp" class="form-text">
                        Names must be unique in a <b>case insensitive</b> sense and cannot be empty.
                    </div>
                </div>
            </div>
            <div class="col">
                <div class="mb-3">
                    <label for="baseSlotPolicySelect" class="form-label"
                        >Base Slot Policy</label
                    >
                    {#await getSlotPolicies()}
                        Loading
                    {:then policies}
                        <select
                            class="form-control"
                            id="baseSlotPolicySelect"
                            bind:value={outfit.slotPolicy}
                        >
                            {#each policies.filter((p) => !p.advanced) as policy}
                                <option value={policy.value}>
                                    {#if builtInPolicyNames[policy.value]}
                                        {builtInPolicyNames[policy.value]}
                                    {:else}
                                        {policy.ui_name}
                                    {/if}
                                </option>
                            {/each}
                        </select>
                    {:catch error}
                        Unable to load policies
                    {/await}
                </div>
            </div>
        </div>

        <ArmorSearch
            onAdd={(armor) => {
                outfit.armors = outfit.armors.filter(
                    (otherLoc) =>
                        !masksConflict(
                            resolveLocator(otherLoc)?.mask,
                            armor.mask
                        )
                );
                outfit.armors = outfit.armors.concat([armor.locator]);
            }}
            existingArmors={allArmorInfo}
        />

        <div class="mb-3">
            <label class="form-label" for="outfitItemsList">Outfit Items</label>
            <div id="outfitItemsList" class="list-group">
                {#each typedClone(outfit.armors).sort((a, b) => (resolveLocator(a)?.mask ?? 0 + resolveLocator(a)?.name?.length ?? 0) - (resolveLocator(b)?.mask ?? 0 + resolveLocator(b)?.name?.length ?? 0)) as armor}
                    <button class="list-group-item list-group-item-action list-group-item-deletion " on:click|preventDefault={() => {
                        outfit.armors = 
                            outfit.armors.filter((item) => !_.isEqual(item, armor));
                    }}>
                        <div class="d-flex w-100 justify-content-between">
                            <h5 style="--bs-breadcrumb-divider: '&mdash;';">
                                <ol class="breadcrumb mb-0">
                                    <li class="breadcrumb-item">
                                        {resolveLocator(armor)?.name ??
                                            "Unknown"}
                                    </li>
                                    <li class="breadcrumb-item active">
                                        <span class="text-muted"
                                            >{getSlots(
                                                resolveLocator(armor)?.mask ?? 0
                                            ).join(", ")}</span
                                        >
                                    </li>
                                </ol>
                            </h5>
                        </div>
                        <div
                            style="--bs-breadcrumb-divider: '&rsaquo';"
                            class="fw-semibold"
                        >
                            <ol class="breadcrumb mb-0">
                                <li class="breadcrumb-item">
                                    {armor.modName}
                                </li>
                                <li class="breadcrumb-item">
                                    <span class="font-monospace"
                                        >0x{armor.localFormId
                                            .toString(16)
                                            .padStart(8, "0")}</span
                                    >
                                </li>
                            </ol>
                        </div>
                    </button>
                {/each}
            </div>
        </div>

        <div class="mb-3">
            <label class="form-label" for="slotPoliciesList"
                >Slot Policies</label
            >
            <div id="slotPoliciesList" class="list-group">
                {#each Object.keys(outfit.slotPolicies).map( (value) => parseInt(value) ) as slot}
                    <div class="list-group-item">
                        <div class="d-flex justify-content-between">
                            <h5>{slot} - {slotNames[slot]}</h5>
                            {#await getSlotPolicies()}
                                Loading
                            {:then policies}
                                <div class="d-flex">
                                    <select
                                        class="form-control form-control-sm me-2"
                                        style="width: max-content;"
                                        id="baseSlotPolicySelect{slot}"
                                        bind:value={outfit.slotPolicies[slot]}
                                    >
                                        {#each policies.filter((p) => !p.advanced) as policy}
                                            <option value={policy.value}>
                                                {#if builtInPolicyNames[policy.value]}
                                                    {builtInPolicyNames[
                                                        policy.value
                                                    ]}
                                                {:else}
                                                    {policy.ui_name}
                                                {/if}
                                            </option>
                                        {/each}
                                    </select>
                                    <button
                                        class="btn btn-outline-danger btn-sm"
                                        on:click|preventDefault={() => {
                                            outfit.slotPolicies[slot] =
                                                undefined; // Hack to make Svelte notice it
                                            delete outfit.slotPolicies[slot];
                                        }}>Remove</button
                                    >
                                </div>
                            {:catch error}
                                Unable to load policies
                            {/await}
                        </div>
                    </div>
                {/each}
                <div class="list-group-item">
                    <div class="d-flex">
                        <select
                            class="form-control form-control-sm me-2"
                            id="newSlotPolicySlotSelect"
                            bind:value={newSlotPolicySlot}
                        >
                            {#each Object.keys(slotNames)
                                .filter((slot) => !Object.keys(outfit.slotPolicies).find((other) => other == slot))
                                .map((value) => parseInt(value)) as slot}
                                <option value={slot}>
                                    {slot} - {slotNames[slot]}
                                </option>
                            {/each}
                        </select>

                        {#await getSlotPolicies()}
                            Loading
                        {:then policies}
                            <div class="d-flex">
                                <select
                                    class="form-control form-control-sm me-2"
                                    style="width: max-content;"
                                    id="newSlotPolicyPolicySelect"
                                    bind:value={newSlotPolicyValue}
                                >
                                    {#each policies.filter((p) => !p.advanced) as policy}
                                        <option value={policy.value}>
                                            {#if builtInPolicyNames[policy.value]}
                                                {builtInPolicyNames[
                                                    policy.value
                                                ]}
                                            {:else}
                                                {policy.ui_name}
                                            {/if}
                                        </option>
                                    {/each}
                                </select>
                                <button
                                    class="btn btn-outline-success btn-sm"
                                    on:click|preventDefault={() => {
                                        outfit.slotPolicies = {
                                            [newSlotPolicySlot]:
                                                newSlotPolicyValue,
                                            ...outfit.slotPolicies,
                                        };
                                    }}>Add</button
                                >
                            </div>
                        {:catch error}
                            Unable to load policies
                        {/await}
                    </div>
                </div>
            </div>
        </div>
    </form>
{/if}
