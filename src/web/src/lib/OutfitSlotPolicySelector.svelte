<script lang="ts">
    import type { Outfit, PolicyInfo } from "./APITypes";
    export let outfit: Outfit;
    export let policies: PolicyInfo[];
    export let slot: number;
</script>

{#if outfit}
<select class="form-control-sm" id="baseSlotPolicySelect" bind:value={outfit.slotPolicies[slot]}>
    {#each policies as policy}
    <option value={policy.value}>{policy.ui_name}</option>
    {/each}
</select>
{/if}
