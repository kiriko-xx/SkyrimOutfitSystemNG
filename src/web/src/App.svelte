<script lang="ts">
    import OutfitTable from "./lib/OutfitTable.svelte";
    import OutfitEditor from "./lib/OutfitEditor.svelte";
    import { Router, Link, Route, navigate } from "svelte-navigator";
    import { onMount } from "svelte";
    import structuredClone from '@ungap/structured-clone';
    import Navbar from "./lib/Navbar.svelte";
    import type { Outfit, OutfitSystem } from "./lib/APITypes";
    import ActorAssignmentsTable from "./lib/ActorAssignmentsTable.svelte";
    let state: OutfitSystem = undefined;
    var generation = 0;
    const reload = async () => {
        let expected_generation = generation;
        let response = await fetch("/api/state");
        let response_json = await response.json();
        if (generation == expected_generation) {
            state = response_json;
            generation += 1;
        }
    };
    const changedState = async () => {
        if (!state) {
            await reload();
            return;
        }
        let response = await fetch("/api/state", {
            method: "POST", // or 'PUT'
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify(state),
        });
        let code = response.ok;
        console.log("Response for new state:", code);
        if (response.ok) {
            await reload();
        }
    };
    async function getDefaultOutfitTemplate() {
        let response = await fetch("/api/outfits/default");
        let response_json: Outfit = await response.json();
        return response_json;
    }
    onMount(reload);
</script>

<main>
    <Router>
        <header
            class="navbar navbar-dark sticky-top bg-dark flex-md-nowrap p-0 navbar-shadow"
        >
            <a class="navbar-brand col-md-3 col-lg-2 me-0 px-3 fs-6" href="."
                >Skyrim Outfit System</a
            >
            <button
                class="navbar-toggler position-absolute d-md-none collapsed"
                type="button"
                data-bs-toggle="collapse"
                data-bs-target="#sidebarMenu"
                aria-controls="sidebarMenu"
                aria-expanded="false"
                aria-label="Toggle navigation"
            >
                <span class="navbar-toggler-icon" />
            </button>
            <div class="nav-item w-100 rounded-0 border-0" />
            <div class="d-flex">
                <div class="navbar-nav me-2">
                    <div class="nav-item text-nowrap">
                        <button
                            type="button"
                            class="btn btn-sm btn-outline-secondary px-3"
                            on:click={reload}>Reload from Game</button
                        >
                    </div>
                </div>
            </div>
        </header>
        {#if state}
            <div class="container-fluid">
                <div class="row">
                    <Navbar />
                    <main class="col-md-9 ms-sm-auto col-lg-10 px-md-4">
                        <Route path="/outfits/*">
                            <Route path="/:outfitName" let:params>
                                <OutfitEditor
                                    outfit={structuredClone(
                                        state.outfits.find(
                                            (item) =>
                                                item.name ==
                                                decodeURIComponent(
                                                    params.outfitName
                                                )
                                        )
                                    )}
                                    saveAction={(outfit, returnToHome) => {
                                        let index = state.outfits.findIndex(
                                            (item) =>
                                                item.name ==
                                                decodeURIComponent(
                                                    params.outfitName
                                                )
                                        );
                                        state.outfits[index] = outfit;
                                        if (returnToHome) {
                                            navigate("/outfits/");
                                        } else {
                                            navigate(
                                                `/outfits/${encodeURIComponent(
                                                    outfit.name
                                                )}`
                                            );
                                        }
                                        changedState();
                                    }}
                                    cancelAction={() => {
                                        navigate("/outfits/");
                                    }}
                                />
                            </Route>
                            <Route fallback>
                                    <OutfitTable
                                        toggleFavorite={outfitName => {
                                            let outfit = state.outfits.find(
                                                (item) =>
                                                    item.name == outfitName
                                            );
                                            console.log(outfit);
                                            if (outfit.isFavorite) {
                                                outfit.isFavorite = false;
                                            } else {
                                                outfit.isFavorite = true;
                                            }
                                            changedState();
                                        }}
                                        createOutfit={async newOutfitName => {
                                            let newOutfitTemplate =
                                                await getDefaultOutfitTemplate();
                                            newOutfitTemplate.name =
                                                newOutfitName;
                                            state.outfits = [
                                                ...state.outfits,
                                                newOutfitTemplate,
                                            ];
                                            navigate(
                                                `/outfits/${encodeURIComponent(
                                                    newOutfitName
                                                )}`
                                            );
                                            changedState();
                                        }}
                                        deleteOutfit={(outfitName) => {
                                            state.outfits =
                                                state.outfits.filter(
                                                    (item) =>
                                                        item.name != outfitName
                                                );
                                            changedState();
                                        }}
                                        outfits={(
                                            [...state.outfits]
                                        ).sort((a, b) =>
                                            a.name?.localeCompare(b.name)
                                        )}
                                    />
                            </Route>
                        </Route>
                        <Route path="/assignments/*">
                            <Route fallback>
                                <ActorAssignmentsTable
                                    assignments={state.actorOutfitAssignments}
                                    outfits={([...state.outfits]).sort(
                                        (a, b) => a.name?.localeCompare(b.name)
                                    )}
                                    didUpdateSelections={(newAssignments) => {
                                        state.actorOutfitAssignments =
                                            newAssignments;
                                        changedState();
                                    }}
                                />
                            </Route>
                        </Route>
                    </main>
                </div>
            </div>
        {:else}
            <div class="px-5">
                <h2 style="text-align: center;">Loading...</h2>
            </div>
        {/if}
    </Router>
</main>

<style>
</style>
