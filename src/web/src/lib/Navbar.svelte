<script lang="ts">
    import { link, useLocation } from "svelte-navigator";

    let location = useLocation();
</script>

<nav id="sidebarMenu" class="col-md-3 col-lg-2 d-md-block sidebar collapse">
    <div class="position-sticky pt-3 sidebar-sticky">
        <ul class="nav flex-column">
            <li class="nav-item">
                <a
                    class="nav-link {$location.pathname.startsWith('/outfits')
                        ? 'active'
                        : ''}"
                    href="/outfits"
                    use:link
                >
                    <i class="bi bi-collection" />
                    Outfits
                </a>
            </li>
            <li class="nav-item">
                <a
                    class="nav-link {$location.pathname.startsWith('/assignments')
                        ? 'active'
                        : ''}"
                    href="/assignments"
                    use:link
                >
                    <i class="bi bi-person" />
                    Assignments
                </a>
            </li>
        </ul>
    </div>
</nav>
