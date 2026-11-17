<script lang="ts">
    import { onMount } from "svelte";
    import type { ActorOutfitAssignment, Outfit } from "./APITypes";
    import { autoswitchLabels, autoswitchLabelsOrdered } from "./Slots";

    export let assignments: Record<number, ActorOutfitAssignment>;
    export let outfits: Outfit[];
    export let didUpdateSelections: (
        assignments: Record<number, ActorOutfitAssignment>
    ) => void;

    let names: Record<number, string> = {};

    function lookupNames() {
        Object.keys(assignments)
            .map((value) => parseInt(value))
            .forEach((actorId) => {
                fetch(`/api/actors/${actorId}`).then((response) => {
                    response.json().then((data: any) => {
                        names[actorId] = data.name;
                    });
                });
            });
    }
    onMount(lookupNames);
    $: {
        assignments;
        lookupNames();
    }
</script>

<div
    class="d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pt-3 pb-2 mb-3 border-bottom"
>
    <h1 class="h2">Assignments</h1>
    <div class="btn-group me-2" />
</div>

<div class="mb-3">
    <div id="outfitItemsList" class="list-group">
        {#each Object.keys(assignments).map( (value) => parseInt(value) ) as assignmentIdx}
            <div class="list-group-item">
                <div class="d-flex w-100 justify-content-between">
                    <h5 style="--bs-breadcrumb-divider: '&mdash;';">
                        <ol class="breadcrumb mb-0">
                            <li class="breadcrumb-item">
                                {names[assignmentIdx] ?? ""}
                            </li>
                            <li class="breadcrumb-item active">
                                <span class="text-muted fs-5 font-monospace"
                                    >0x{assignmentIdx
                                        .toString(16)
                                        .padStart(8, "0")}</span
                                >
                            </li>
                        </ol>
                    </h5>
                </div>
                <form>
                    <div class="row g-3">
                        <div class="col-md-12">
                            <div class="form-floating">
                                <select
                                    class="form-select"
                                    style="width: max-content;"
                                    id="actorCurrentOutfit{assignmentIdx}"
                                    bind:value={assignments[assignmentIdx]
                                        .currentOutfitName}
                                    on:change={() =>
                                        didUpdateSelections(assignments)}
                                >
                                    <option value={undefined}>
                                        Unassigned
                                    </option>
                                    {#each outfits as outfit}
                                        <option value={outfit.name}>
                                            {outfit.name}
                                        </option>
                                    {/each}
                                </select>
                                <label for="actorCurrentOutfit{assignmentIdx}"
                                    >Current Outfit</label
                                >
                            </div>
                        </div>
                        <div class="col-md-12">
                            <div class="form-check form-switch">
                                <input
                                    class="form-check-input"
                                    type="checkbox"
                                    role="switch"
                                    id="flexSwitchCheckChecked"
                                    bind:checked={assignments[assignmentIdx]
                                        .stateBasedAutoSwitchEnabled}
                                    on:change={() =>
                                        didUpdateSelections(assignments)}
                                />
                                <label
                                    class="form-check-label"
                                    for="flexSwitchCheckChecked"
                                    >Enable autoswitching</label
                                >
                            </div>
                        </div>
                        {#if assignments[assignmentIdx].stateBasedAutoSwitchEnabled}
                            <div class="col-md-12 gap-3 d-flex flex-row flex-wrap align-items-start ">
                                    {#each autoswitchLabelsOrdered as stateIdx}
                                        <div class="">
                                            <div class="form-floating">
                                                <select
                                                    class="form-select"
                                                    style="width: max-content;"
                                                    id="actorAutoswitchOutfit{assignmentIdx}-{stateIdx}"
                                                    bind:value={assignments[
                                                        assignmentIdx
                                                    ].stateBasedOutfits[
                                                        stateIdx
                                                    ]}
                                                    on:change={() =>
                                                        didUpdateSelections(
                                                            assignments
                                                        )}
                                                >
                                                    <option value={undefined}>
                                                        Unassigned
                                                    </option>
                                                    {#each outfits as outfit}
                                                        <option
                                                            value={outfit.name}
                                                        >
                                                            {outfit.name}
                                                        </option>
                                                    {/each}
                                                </select>
                                                <label
                                                    for="actorAutoswitchOutfit{assignmentIdx}-{stateIdx}"
                                                    >{autoswitchLabels[
                                                        stateIdx
                                                    ]}</label
                                                >
                                            </div>
                                        </div>
                                    {/each}
                            </div>
                        {/if}
                    </div>
                </form>
            </div>
        {/each}
    </div>
</div>
