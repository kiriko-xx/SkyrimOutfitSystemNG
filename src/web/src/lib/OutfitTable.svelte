<script lang="ts">
    import { link } from "svelte-navigator";
    import type { Outfit } from "./APITypes";
    import { Modal } from "bootstrap";
    export let outfits: Outfit[];
    export let toggleFavorite: (outfitName: string) => void;
    export let deleteOutfit: (outfitName: string | undefined) => void;
    export let createOutfit: (outfitName: string) => void;

    let newOutfitName: string = "";
    let newOutfitAllowed = false;
    let newOutfitModalElement: Element | undefined;
    var newOutfitModal: Modal | undefined;
    $: {
        if (newOutfitModalElement) {
            newOutfitModal = new Modal(newOutfitModalElement);
        }
    }
    function openNewOutfitModel() {
        newOutfitName = "";
        newOutfitModal.show();
    }
    function createOutfitHandler() {
        createOutfit(newOutfitName);
        newOutfitModal.hide();
    }

    let deleteOutfitName: string = "";
    let deleteOutfitModalElement: Element | undefined;
    var deleteOutfitModal: Modal | undefined;
    $: {
        if (deleteOutfitModalElement) {
            deleteOutfitModal = new Modal(deleteOutfitModalElement);
        }
    }
    function openDeleteOutfitModel(outfit: Outfit) {
        deleteOutfitName = outfit.name;
        deleteOutfitModal.show();
    }
    function deleteOutfitHandler() {
        deleteOutfit(deleteOutfitName);
        deleteOutfitModal.hide();
    }

    // Comment this out when not hacking on modal.
    // setTimeout(openModel, 0.1);
    $: {
        newOutfitAllowed = !(
            newOutfitName === "" ||
            outfits.find(
                (outfit) =>
                    outfit.name.toLocaleLowerCase() ===
                    newOutfitName.toLocaleLowerCase()
            )
        );
    }
</script>

<div
    class="d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pt-3 pb-2 mb-3 border-bottom"
>
    <h1 class="h2">Outfits</h1>
    <div class="btn-group me-2">
        <button
            type="button"
            class="btn btn-outline-primary btn-sm"
            on:click={() => openNewOutfitModel()}>New Outfit</button
        >
    </div>
</div>

<div
    class="modal fade"
    id="exampleModal"
    bind:this={newOutfitModalElement}
    tabindex="-1"
    aria-labelledby="exampleModalLabel"
    aria-hidden="true"
>
    <div class="modal-dialog">
        <div class="modal-content">
            <div class="modal-header">
                <h1 class="modal-title fs-5" id="exampleModalLabel">
                    New Outfit
                </h1>
                <button
                    type="button"
                    class="btn-close"
                    data-bs-dismiss="modal"
                    aria-label="Close"
                />
            </div>
            <div class="modal-body">
                <form>
                    <div class="mb-3">
                        <label for="newOutfitName" class="form-label"
                            >Outfit Name</label
                        >
                        <input
                            type="email"
                            class="form-control"
                            id="newOutfitName"
                            bind:value={newOutfitName}
                            aria-describedby="name"
                        />
                        <div id="nameHelp" class="form-text">
                            Names must be unique in a <b>case insensitive</b> sense.
                        </div>
                    </div>
                </form>
            </div>
            <div class="modal-footer">
                <button
                    type="button"
                    class="btn btn-secondary"
                    data-bs-dismiss="modal">Cancel</button
                >
                <button
                    type="button"
                    class="btn btn-primary {newOutfitAllowed ? '' : 'disabled'}"
                    on:click={createOutfitHandler}>Create Outfit</button
                >
            </div>
        </div>
    </div>
</div>

<div
    class="modal fade"
    id="exampleModal"
    bind:this={deleteOutfitModalElement}
    tabindex="-1"
    aria-labelledby="exampleModalLabel"
    aria-hidden="true"
>
    <div class="modal-dialog">
        <div class="modal-content">
            <div class="modal-header">
                <h1 class="modal-title fs-5" id="exampleModalLabel">
                    Delete {deleteOutfitName}?
                </h1>
                <button
                    type="button"
                    class="btn-close"
                    data-bs-dismiss="modal"
                    aria-label="Close"
                />
            </div>
            <div class="modal-body">
                This action will be sent to the game immediately. You would need to reload a save to undo it.
            </div>
            <div class="modal-footer">
                <button
                type="button"
                class="btn btn-danger"
                on:click={deleteOutfitHandler}>Delete</button
                >

                <button
                    type="button"
                    class="btn btn-secondary"
                    data-bs-dismiss="modal">Cancel</button
                >
            </div>
        </div>
    </div>
</div>

<div class="mb-3">
    <div id="outfitItemsList" class="list-group">
        {#each outfits as outfit}
        <div class="list-group-item">
            <div class="d-flex w-100 justify-content-between">
                <h5>
                    <i
                    class={outfit.isFavorite
                        ? "bi-star-fill"
                        : "bi-star"}
                    on:keyup={() => toggleFavorite(outfit.name)}
                    on:click={() => toggleFavorite(outfit.name)}
                />
                    <a class="text-body-emphasis" style="text-decoration: none;" href="/outfits/{outfit.name}" use:link>{outfit.name}</a>
                </h5>
                <span>
                    <a class="btn btn-outline-primary" href="/outfits/{outfit.name}" use:link>
                        Edit
                    </a>
                    <span class="btn btn-outline-danger" on:click={() => openDeleteOutfitModel(outfit)} on:keyup={() => openDeleteOutfitModel(outfit)}>
                        Delete
                    </span>
                </span>

            </div>
            <div class="d-flex w-100 justify-content-start">
                {outfit.armors.length} Items
            </div>
        </div>
        {/each}
    </div>
</div>
