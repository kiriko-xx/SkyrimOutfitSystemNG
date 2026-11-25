$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.71 hotkey input core and the Change Outfit power key.
# What has to stay true:
#  - firing is driven by the input sink only: the device polling exists so the
#    settings page can record keys, and it must never fire a binding (that is what
#    made the hotkeys fire from inside the menu)
#  - while the framework menu is open nothing fires: the menu owns the input, and
#    the keys released during that time were eaten, so the chord state is dropped
#  - one key table feeds the parser, the formatter and the polling, so what the
#    page shows is what fires
#  - the Shout/Power key is read in the same code space the rest of the file uses,
#    on both devices, acted on only while our own power is selected (or while the
#    ini key is pressed), and consumed on the press
#  - the recorder is FreeClimb's: it owns the device it records (the events are
#    unlinked from the input queue on the device polls, before anything is
#    dispatched, so Escape, Back and the shoulder buttons never reach the menu),
#    nothing is accepted until the device is neutral, and a chord is stored only
#    once every key of it was released - a modifier pressed on its way to Alt+O is
#    therefore never stored on its own
#  - the Change Outfit power is recognised by the plugin file it comes from: the
#    esp and its scripts are optional installs, so nothing searches the load order
#  - a row keeps one binding per device, both fire, and the recorder is a modal
#    window that disables the page underneath, stops the host input and eats every
#    event of the batch - including the ones the framework menu would act on,
#    which is why the menu input callback is registered on every framework build
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$failures = 0

function Read-RepoFile([string]$relative) {
    $path = Join-Path $root $relative
    if (-not (Test-Path $path)) { throw "missing $relative" }
    return (Get-Content -Raw -LiteralPath $path)
}

function Assert-Contains([string]$text, [string]$needle, [string]$label) {
    if ($text.Contains($needle)) { return }
    Write-Output "FAIL $label (missing: $needle)"
    $script:failures++
}

function Assert-Absent([string]$text, [string]$needle, [string]$label) {
    if (-not $text.Contains($needle)) { return }
    Write-Output "FAIL $label (unexpected: $needle)"
    $script:failures++
}

# Returns the text between $start and $end, so an assertion about one function
# cannot be satisfied by a line that belongs to another one.
function Get-Block([string]$text, [string]$start, [string]$end) {
    $from = $text.IndexOf($start)
    if ($from -lt 0) { throw "block start not found: $start" }
    $to = $text.IndexOf($end, $from)
    if ($to -lt 0) { throw "block end not found: $end" }
    return $text.Substring($from, $to - $from)
}

# ---- firing is the input sink's job, polling only serves the recorder ----
$hotkeys = Read-RepoFile 'src/cpp/NGHotkeys.cpp'
$pollTick = Get-Block $hotkeys 'void PollTick()' 'class InputSink'
Assert-Absent $pollTick 'FireRandomThrottled()' 'the device polling never fires a binding'
Assert-Absent $pollTick 'FireQuickPicker()' 'the device polling never fires a binding'
Assert-Absent $pollTick 'CheckChords(' 'the device polling never fires a binding'
Assert-Contains $pollTick 'if (!g_capturing.load(std::memory_order_relaxed)) {' 'the polling gives up at once when nothing is being recorded'
Assert-Contains $pollTick 'PollCapture();' 'the polling drives the recorder'
Assert-Contains $hotkeys 'CheckCaptureTimeout();' 'a forgotten recording still times out'

# ---- an open menu mutes every hotkey ----
Assert-Contains $hotkeys 'std::atomic<bool> g_frameworkMenuOpen' 'the menu state is remembered'
Assert-Contains $hotkeys 'void NotifyMenuState(bool a_open)' 'the menu reports its state'
Assert-Contains $hotkeys 'bool OurMenuOpen()' 'one place decides whether our own menu is up'
Assert-Contains $hotkeys 'MenuBridge::AnyBlockingWindowOpened()' 'a blocking framework window also counts as open'
Assert-Contains $hotkeys 'bool FiringBlocked()' 'one gate for every path that can fire'
Assert-Contains $hotkeys 'bool GameWindowFocused()' 'a background game window does not fire'
Assert-Contains $hotkeys 'ui->GameIsPaused()' 'a paused game does not fire'
Assert-Contains $hotkeys 'const bool blocked = FiringBlocked();' 'the input sink asks the gate once per batch'

# ---- one key table for parsing, formatting and polling ----
Assert-Contains $hotkeys 'struct KeyEntry' 'the keyboard keys live in one table'
Assert-Contains $hotkeys 'KeyTable()' 'the table is the single source of truth'
Assert-Contains $hotkeys 'struct PadEntry' 'the controller buttons live in one table'
Assert-Contains $hotkeys 'PadTable()' 'the table is the single source of truth'
Assert-Contains $hotkeys 'void PollDevices(' 'the recorder polls the devices'
Assert-Contains $hotkeys 'bool CachedTextInputActive()' 'a text field keeps the hotkeys quiet'

# ---- the power key ----
Assert-Contains $hotkeys 'RE::BSWin32GamepadDevice::Keys' 'controller codes come from the button masks the engine uses'
Assert-Contains $hotkeys 'GamepadKeycodeToMask' 'the game power binding is converted into that same space'
Assert-Contains $hotkeys 'kMacro_GamepadOffset' 'a control-map keycode is recognised by its range before it is converted'
Assert-Contains $hotkeys 'SKSE::InputMap::GamepadKeycodeToMask(gamepad)' 'only a keycode is converted, a raw mask is kept as it is'
Assert-Contains $hotkeys 'std::uint32_t mask = gamepad;' 'the mask survives when the engine reports it directly'
Assert-Contains $hotkeys 'the power key is 0x{:02X} on the keyboard and 0x{:04X} on the controller' 'the resolved pad binding is visible in the log'
Assert-Contains $hotkeys 'bool OurPowerSelected()' 'the plugin checks its own power is the selected one'
Assert-Contains $hotkeys 'selectedPower' 'the check is against the selected power of the player'
Assert-Contains $hotkeys 'constexpr std::string_view kPowerPlugin = "SkyrimOutfitSystem.esp";' 'the power is recognised by the plugin it comes from'
Assert-Contains $hotkeys 'bool SameFilename(' 'the plugin name is compared without caring about case'
Assert-Absent $hotkeys 'FirstOwnPower' 'the load order is never searched for the power'
Assert-Absent $hotkeys 'GetFormArray' 'the load order is never walked for the power'
Assert-Absent $hotkeys 'SkyrimOutfitSystemQuickslotSpell' 'no editor id is looked up either'
Assert-Absent $hotkeys 'lastTryMs' 'nothing is retried on a timer any more'
Assert-Contains $hotkeys 'bool IsGamePowerKey(const Trigger& a_trigger)' 'the game Shout/Power binding is recognised on both devices'
Assert-Contains $hotkeys 'bool IsQuickslotKey(const Trigger& a_trigger)' 'the ini key opens the window as well'
Assert-Contains $hotkeys 'bool IsPowerKey(const Trigger& a_trigger)' 'the Shout/Power key is recognised'
Assert-Contains $hotkeys 'const bool ours = OurPowerSelected();' 'the press is ours only while our power is the selected one'
Assert-Contains $hotkeys 'if (!ours && !IsQuickslotKey(a_trigger)) {' 'nothing happens while another power is equipped and the ini key is not it'
Assert-Contains $hotkeys 'return ours;' 'with another power equipped the ini key still opens the window, the event stays with the game'
Assert-Contains $hotkeys 'swallow = true;' 'that press is consumed, so the game does not cast the power'
Assert-Contains $hotkeys 'if (HandlePowerPress(trigger, blocked)) {' 'the press is handled on the press itself'
Assert-Contains $hotkeys 'bool PowerKeyHeld()' 'the polled path knows both devices and the ini key'
Assert-Contains $hotkeys 'kQuickslotOpenCooldownMs' 'only our own timer throttles the power key'
Assert-Absent $hotkeys 'InterruptCast' 'no extra address-library id is added just to stop a cast'

# ---- the recorder: one device at a time, settle before accepting ----
$hotkeysH = Read-RepoFile 'src/cpp/include/NGHotkeys.h'
Assert-Contains $hotkeysH 'enum class Input' 'the recorder knows which device it is recording'
Assert-Contains $hotkeysH 'void BeginCapture(Slot a_slot, Input a_device' 'a recording starts for one device'
Assert-Contains $hotkeysH 'Input CaptureDevice();' 'the recorder window can show what it is recording'
Assert-Contains $hotkeysH 'bool CaptureArmed();' 'the window can tell waiting from listening'
Assert-Contains $hotkeysH 'int CaptureProblem();' 'the window can explain a rejected combination'
Assert-Contains $hotkeysH 'bool TakeCaptureResult(Slot& a_slot, Input& a_device' 'the result says which device it belongs to'
Assert-Contains $hotkeysH 'void NotifyMenuState(bool a_open);' 'the menu state entry point is declared'

$settingsH = Read-RepoFile 'src/core/Settings.h'
Assert-Contains $settingsH 'randomOutfitGamepadHotkey' 'the random outfit keeps a controller binding of its own'
Assert-Contains $settingsH 'quickPickerGamepadHotkey' 'the quick picker keeps a controller binding of its own'

$settings = Read-RepoFile 'src/core/Settings.cpp'
Assert-Contains $settings '"HotkeyGamepad"' 'the controller binding is read from the ini'
Assert-Contains $settings '"QuickPickerHotkeyGamepad"' 'the quick picker controller binding is read from the ini'

Assert-Contains $hotkeys 'g_randomKb' 'the random outfit has one chord per device'
Assert-Contains $hotkeys 'g_randomPad' 'the random outfit has one chord per device'
Assert-Contains $hotkeys 'g_quickKb' 'the quick picker has one chord per device'
Assert-Contains $hotkeys 'g_quickPad' 'the quick picker has one chord per device'
Assert-Contains $hotkeys 'bool AnyChordBound(' 'either binding counts as bound'
Assert-Contains $hotkeys 'bool AnyChordDown(' 'either binding can fire'
Assert-Contains $hotkeys 'Chord OnlyDevice(' 'an old single-string value can be split by device'
Assert-Contains $hotkeys 'AppendChord(' 'the old value is merged instead of dropped'
Assert-Contains $hotkeys 'enum class CaptureState' 'the recording has the FreeClimb states'
Assert-Absent $hotkeys 'kSettling' 'a chord is not ended by a quiet moment any more'
Assert-Absent $hotkeys 'kCaptureSettleMs' 'the settle window is gone'
Assert-Absent $hotkeys 'g_captureQuietMs' 'no quiet moment is timed'
Assert-Absent $hotkeys 'g_captureDown' 'the recorder works on what is held, not on a down list'
Assert-Contains $hotkeys 'bool SampleCapture(Dev a_device, const std::vector<Trigger>& a_pressed)' 'one snapshot is all the state machine needs'
Assert-Contains $hotkeys 'if (state == CaptureState::kWaiting) {' 'nothing is accepted before the device is neutral'
Assert-Contains $hotkeys 'if (pressed.empty()) {' 'an empty device is what ends a recording'
Assert-Contains $hotkeys 'bool Covered(' 'a released key is told apart from a new one'
Assert-Contains $hotkeys 'FailCaptureLocked(CaptureProblem::kNotTogether);' 'keys pressed one after the other are refused, not stored one by one'
Assert-Contains $hotkeys 'if (pressed.empty()) {
					g_captureState.store(static_cast<int>(CaptureState::kArmed)' 'the recorder starts listening once nothing is held'
Assert-Contains $hotkeys 'bool ValidCaptureChord(' 'a chord is rejected when it cannot be used'
Assert-Contains $hotkeys 'bool IsCaptureCancel(' 'Esc and the controller back button cancel a recording'
Assert-Contains $hotkeys 'void FailCaptureLocked(' 'a rejected chord is reported instead of silently kept'
Assert-Contains $hotkeys 'kCaptureTimeoutMs' 'a forgotten recording does not lock the page forever'
Assert-Absent $hotkeys 'AdvanceCaptureLocked' 'the chord is no longer accumulated key by key'
Assert-Absent $hotkeys 'PromoteCaptureLocked' 'the waiting phase ends on a snapshot, not on a timer'

# ---- the recorder owns the device: the events never reach the menu ----
Assert-Absent $hotkeys 'bool FeedEvent(RE::InputEvent* a_event)' 'no event of a recording is fed to a chord builder'
Assert-Contains $hotkeys 'bool EventAsksCancel(RE::InputEvent* a_event)' 'Esc and Back are recognised on the events as well'
Assert-Contains $hotkeys 'void FilterInputQueue()' 'the recorder takes its events off the input queue'
Assert-Contains $hotkeys 'RE::BSInputEventQueue::GetSingleton()' 'the queue the game is about to dispatch is the one filtered'
Assert-Contains $hotkeys 'auto*&      head = queue->GetQueueHead();' 'the queue head is followed, not copied'
Assert-Contains $hotkeys 'tail = previous;' 'the queue tail survives removing the last event'
Assert-Contains $hotkeys 'previous->next = next;' 'the event is unlinked instead of marked'
Assert-Contains $hotkeys 'bool FilterEvent(RE::InputEvent* a_event, bool& a_cancel)' 'one place decides what leaves the queue'
Assert-Contains $hotkeys 'class QueueOwnership' 'a key swallowed on the way down is swallowed on the way up'
Assert-Contains $hotkeys 'if (a_up) {' 'the release of an owned key is eaten as well'
Assert-Contains $hotkeys 'return a_capturing;' 'a key the recorder does not know is still eaten'
Assert-Contains $hotkeys 'void KeyboardPollHook(RE::BSWin32KeyboardDevice* a_device, float a_delta)' 'the keyboard poll is hooked'
Assert-Contains $hotkeys 'void GamepadPollHook(RE::BSPCGamepadDeviceHandler* a_handler, float a_delta)' 'the controller poll is hooked'
Assert-Contains $hotkeys 'if (g_keyboardPoll) {' 'the original keyboard poll still runs'
Assert-Contains $hotkeys 'if (g_gamepadPoll) {' 'the original controller poll still runs'
Assert-Contains $hotkeys 'Hooking::InstallVFunc(' 'the slot is replaced through the checked helper'
Assert-Contains $hotkeys 'Hooking::IdExists(a_table)' 'an unknown table is never resolved'
Assert-Contains $hotkeys 'static const REL::RelocationID keyboardTable{ 285449, 236808 };' 'the keyboard table is a single relocation id'
Assert-Contains $hotkeys 'static const REL::RelocationID gamepadTable{ 285457, 236813 };' 'the controller table is a single relocation id'
Assert-Contains $hotkeys 'void InstallInputHooks()' 'the device hooks are installed in one place'
Assert-Contains $hotkeys 'InstallInputHooks();
		g_installed = true;' 'installing the sink installs the device hooks'
Assert-Contains $hotkeys 'a_device->GetRuntimeData().curState' 'the recorder reads the state the game built the events from'
Assert-Contains $hotkeys 'std::uint32_t KnownScanCode(std::uint32_t a_dik)' 'only keys the ini can name are recorded'
Assert-Contains $hotkeys 'skyrim_cast<RE::BSWin32GamepadDevice*>(delegate)' 'the pad behind the handler is the one that is read'
Assert-Contains $hotkeys 'pad->GetRuntimeData().currentState.gamepad' 'the pad buttons and triggers are read from the device'
Assert-Contains $hotkeys 'if (g_capturing.load(std::memory_order_relaxed)) {' 'the sink asks the recorder before anything else'
Assert-Contains $hotkeys 'if (EventAsksCancel(event)) {' 'the event that ends a recording is the one the recorder sees'
Assert-Contains $hotkeys 'swallow = true;' 'the recording eats the event instead of handling it'
Assert-Contains $hotkeys 'void FeedInputEvent(RE::InputEvent* a_event)
	{
		// The events of a recording are unlinked' 'feeding without a recording does nothing'
Assert-Contains $hotkeysH 'void FeedInputEvent(RE::InputEvent* a_event);' 'the recorder feed is declared for the menu'
Assert-Contains $hotkeysH 'class InputEvent;' 'the declaration needs no engine header'

$vtableH = Read-RepoFile 'src/cpp/include/hooking/VTableHook.h'
Assert-Contains $vtableH 'void* InstallVFunc(' 'the virtual table helper is declared'
$vtable = Read-RepoFile 'src/cpp/hooking/VTableHook.cpp'
Assert-Contains $vtable 'InterlockedCompareExchangePointer' 'the slot is replaced atomically'
Assert-Contains $vtable 'VirtualProtect' 'the table is made writable for the exchange'
Assert-Contains $vtable 'return nullptr;' 'a refused slot replacement hands nothing back'

# ---- the page under the recorder is disabled, the host stops taking input ----
$imGuiH = Read-RepoFile 'src/cpp/include/ImGuiApi.h'
Assert-Contains $imGuiH 'void (*BeginDisabled)(bool' 'the host can grey out the page under the recorder'
Assert-Contains $imGuiH 'void (*EndDisabled)()' 'the host can grey out the page under the recorder'
Assert-Contains $imGuiH 'hasDisabled' 'the greying out is optional, it depends on the host exports'
Assert-Contains $imGuiH 'inline bool DisabledReady()' 'the page is only disabled when the host can undo it'
Assert-Contains $imGuiH 'ImGuiIO_SetAppAcceptingEvents' 'the host input can be switched off'
Assert-Contains $imGuiH 'ImGuiIO_ClearEventsQueue' 'queued input is dropped when a recording starts'
Assert-Contains $imGuiH 'ImGuiIO_ClearInputKeys' 'held keys are cleared when a recording starts'
Assert-Contains $imGuiH 'void SetMenuInputSuppressed(bool' 'one entry point for suppressing host input'
Assert-Contains $imGuiH 'void SetNextWindowFocus()' 'the recorder window can raise itself'

$imGui = Read-RepoFile 'src/cpp/ImGuiApi.cpp'
Assert-Contains $imGui '"igBeginDisabled"' 'the disable switch is resolved at run time'
Assert-Contains $imGui '"igEndDisabled"' 'the disable switch is resolved at run time'
Assert-Contains $imGui '"igGetIO"' 'the ImGuiIO is resolved at run time'
Assert-Contains $imGui '"ImGuiIO_SetAppAcceptingEvents"' 'the accepting-events switch is resolved'
Assert-Contains $imGui '"ImGuiIO_ClearEventsQueue"' 'the event queue drop is resolved'
Assert-Contains $imGui '"ImGuiIO_ClearInputKeys"' 'the input key clear is resolved'
Assert-Contains $imGui '"igSetNextWindowFocus"' 'raising the window is resolved'
Assert-Absent $imGui 'hasExtras = extrasTotal == extrasResolved &&' 'the new bindings are optional and do not gate the extras'

# ---- the settings page and the recorder window ----
$menu = Read-RepoFile 'src/cpp/MenuUI.cpp'
Assert-Contains $menu 'void RenderSettingsBody()' 'the page body can be drawn inside a disabled block'
Assert-Contains $menu 'IG::DisabledReady()' 'the page is only disabled when the host can undo it'
Assert-Contains $menu 'IG::BeginDisabled(true);' 'the page under the recorder is greyed out'
Assert-Contains $menu 'IG::EndDisabled();' 'the page under the recorder is greyed out'
Assert-Contains $menu 'void RenderHotkeyCapture()' 'the recorder is drawn after the page'
Assert-Contains $menu 'Hotkeys::BeginCapture(a_slot, Hotkeys::Input::kKeyboard)' 'a row can record the keyboard'
Assert-Contains $menu 'Hotkeys::BeginCapture(a_slot, Hotkeys::Input::kGamepad)' 'a row can record the controller'
Assert-Contains $menu 'IG::PushID(static_cast<int>(a_slot));' 'rows with the same label keep separate widget ids'
Assert-Contains $menu 'IG::SetNextWindowFocus();' 'the recorder window is raised above the settings page'
Assert-Contains $menu 'IG::BeginWindow(I18n::T("Set a hotkey"' 'the recorder is its own window'
Assert-Contains $menu 'ImGuiWindowFlags_NoNav' 'the recorder window does not take navigation of its own'
Assert-Contains $menu 'Hotkeys::CaptureArmed()' 'the window tells waiting from listening'
Assert-Contains $menu 'Hotkeys::CaptureProblem()' 'the window explains a rejected combination'
Assert-Contains $menu 'Hotkeys::CaptureDevice() == Hotkeys::Input::kGamepad' 'the window says which device is being recorded'
Assert-Contains $menu 'void ApplyHotkeySetting(Hotkeys::Slot a_slot, Hotkeys::Input a_device' 'each device writes its own field'
Assert-Contains $menu 'IG::SetMenuInputSuppressed(Hotkeys::CaptureActive());' 'the host stops reading input for the whole recording'
Assert-Absent $menu 'Hotkeys::CaptureActive() && MenuBridge::EventCallbackAvailable()' 'the suppression no longer depends on a close callback'
Assert-Absent $menu 'and the mouse' 'the page no longer promises mouse buttons that are not recorded'
Assert-Contains $menu 'void __stdcall OnFrameworkMenuEvent(int a_eventType)' 'the menu lifecycle is watched'
Assert-Contains $menu 'a_eventType != MenuBridge::kEventMenuOpened && a_eventType != MenuBridge::kEventMenuClosed' 'both notifications are used'
Assert-Contains $menu 'Hotkeys::NotifyMenuState(open);' 'the hotkeys are muted while the menu is up'
Assert-Contains $menu 'MenuBridge::RegisterEventCallback(OnFrameworkMenuEvent);' 'the lifecycle callback is registered'
Assert-Contains $menu 'IG::SetMenuInputSuppressed(false);' 'the menu gives the host input back'

# ---- the menu input callback belongs to the recorder, on every framework ----
Assert-Contains $menu 'MenuBridge::RegisterInputEvent(OnFrameworkInput)' 'the input callback is registered on every framework build'
Assert-Absent $menu 'if (legacyGamepad && MenuBridge::RegisterInputEvent(OnFrameworkInput))' 'a recording needs the callback even where the framework translates the pad itself'
Assert-Contains $menu 'g_rawGamepad.store(legacyGamepad, std::memory_order_release);' 'only the picker fallback stays limited to the old frameworks'
Assert-Contains $menu 'Hotkeys::FeedInputEvent(a_event);' 'the events the framework would act on are handed to the recorder instead'
Assert-Contains $menu 'if (!g_rawGamepad.load(std::memory_order_acquire)) {' 'outside a recording the framework keeps the pad on the new frameworks'
Assert-Contains $menu 'Hotkeys::CaptureActive()) {' 'the recorder is asked before the pad fallback runs'
Assert-Contains $menu 'the framework did not accept the input callback' 'a refused callback is reported'


$i18n = Read-RepoFile 'src/cpp/I18n.cpp'
Assert-Absent $i18n 'and the mouse' 'the translation table has no mouse promise either'
Assert-Contains $i18n '"Recording the keyboard: hold up to four keys, then let go of all of them."' 'the recorder text matches the devices it records'

$bridgeH = Read-RepoFile 'src/cpp/include/MenuBridge.h'
Assert-Contains $bridgeH 'typedef void(__stdcall* EventCallback)(int a_eventType);' 'the framework event callback is declared'
Assert-Contains $bridgeH 'kEventMenuOpened = 1' 'the open notification keeps its framework value'
Assert-Contains $bridgeH 'kEventMenuClosed = 2' 'the close notification keeps its framework value'
Assert-Contains $bridgeH 'bool RegisterEventCallback(EventCallback a_callback);' 'the callback can be registered'
Assert-Contains $bridgeH 'bool AnyBlockingWindowOpened();' 'the blocking-window query is declared'

$bridge = Read-RepoFile 'src/cpp/MenuBridge.cpp'
Assert-Contains $bridge '"RegisterEventPriority"' 'the framework entry point is resolved by name'
Assert-Contains $bridge 'std::int64_t (*)(EventCallback, float)' 'the export signature is the one the framework declares'
Assert-Contains $bridge '"IsAnyBlockingWindowOpened"' 'the blocking-window query is resolved by name'
Assert-Contains $bridge 'bool AnyBlockingWindowOpened()' 'the blocking-window query is implemented'
Assert-Contains $bridge 'if (!resolved) {' 'a failed lookup is not cached before the framework loaded'

if ($failures -gt 0) {
    Write-Output "HOTKEY_CAPTURE_FAILED: $failures problem(s)"
    exit 1
}
Write-Output 'HOTKEY_CAPTURE_OK: firing comes from the input sink only, an open menu mutes every binding, one key table feeds parse/format/poll, the power key is read in the button-mask space and consumed, and a recording owns the device it records: it is unlinked from the queue, nothing is taken until the device is neutral, and a chord lands only once every key of it is released'
exit 0
