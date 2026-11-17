<script lang="ts">
    import { getSlots, masksConflict } from "./Slots";
    import * as _ from "lodash"; // Structural equality.
    import type { ArmorInfo, ArmorLocator } from "./APITypes";

    export let onAdd: (locator: ArmorInfo) => void;
    export let existingArmors: ArmorInfo[];

    let searchTerm: string = "";
    async function searchForArmor(locator: string) {
        if (locator.length < 3) {
            return [];
        }
        let params = new URLSearchParams({
            search: locator,
        }).toString();
        let response = await fetch("/api/armors/search?" + params);
        let response_json: ArmorInfo[] = await response.json();
        response_json.length = 100;
        return response_json;
    }
</script>

<div class="mb-3">
    <label for="exampleFormControlTextarea1" class="form-label">Add Armor</label
    >
    <div class="list-group">
        <input
            class="list-group-item"
            type="search"
            placeholder="Search"
            aria-label="Search"
            bind:value={searchTerm}
        />
        <!-- svelte-ignore empty-block -->
        {#await searchForArmor(searchTerm)}
            <div class="pre-scrollable">
                {#each Array.from(Array(15).keys()) as item}
                    <div class="list-group-item">
                        <h5>-</h5>
                    </div>
                {/each}
            </div>
        {:then results}
            {#if results.length == 0}{:else}
                <div class="pre-scrollable">
                    {#each results.filter((armorInfo) => !existingArmors.find( (other) => _.isEqual(other.locator, armorInfo.locator) )) as armorInfo}
                        <button class="list-group-item list-group-item-action list-group-item-addition" on:click|preventDefault={() => {onAdd(armorInfo)}}>
                            <div class="d-flex w-100 justify-content-between">
                                <h5
                                    class="mb-0"
                                    style="--bs-breadcrumb-divider: '&mdash;';"
                                >
                                    <ol class="breadcrumb mb-0">
                                        <li class="breadcrumb-item">
                                            {armorInfo.name}
                                        </li>
                                        <li class="breadcrumb-item active">
                                            <span class="text-muted"
                                                >{getSlots(
                                                    armorInfo.mask ?? 0
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
                                        {armorInfo.locator.modName}
                                    </li>
                                    <li class="breadcrumb-item">
                                        <span class="font-monospace"
                                            >0x{armorInfo.locator.localFormId
                                                .toString(16)
                                                .padStart(8, "0")}</span
                                        >
                                    </li>
                                </ol>
                            </div>
                            {#each existingArmors.filter( (other) => masksConflict(other.mask, armorInfo.mask) ) as other}
                                <small class="text-danger"
                                    >Will replace <b>{other.name}</b></small
                                ><br />
                            {/each}
                        </button>
                    {/each}
                </div>
            {/if}
        {/await}
    </div>
</div>

<style lang="scss">
    .pre-scrollable {
        max-height: 340px;
        overflow-y: scroll;
    }
</style>
