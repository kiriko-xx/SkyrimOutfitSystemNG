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
#  - a recording settles only after a quiet moment, so a chord pressed one key
#    after the other is still recorded as a chord
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
Assert-Contains $pollTick 'if (!capturing) {' 'the polling gives up at once when nothing is being recorded'
Assert-Contains $pollTick 'CheckCaptureSettle();' 'the polling drives the recorder'
Assert-Contains $pollTick 'CheckCaptureTimeout();' 'a forgotten recording still times out'

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
Assert-Contains $hotkeys 'SkyrimOutfitSystemQuickslotSpell' 'the power is looked up by editor id'
Assert-Contains $hotkeys 'selectedPower' 'the check is against the selected power of the player'
Assert-Contains $hotkeys 'RE::SpellItem* FirstOwnPower()' 'a rebuilt esp is still found by the file it lives in'
Assert-Contains $hotkeys 'GetLocalFormID()' 'the fallback looks in the light range of our own plugin'
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
Assert-Contains $hotkeys 'if (a_trigger.device != CaptureDeviceDev()) {' 'the other device is ignored while recording'
Assert-Contains $hotkeys 'enum class CaptureState' 'the recording has the FreeClimb states'
Assert-Contains $hotkeys 'kSettling' 'a chord that was pressed key after key is still one chord'
Assert-Contains $hotkeys 'void CheckCaptureSettle()' 'accepting waits for a quiet moment'
Assert-Contains $hotkeys 'kCaptureSettleMs' 'the quiet window is a named constant'
Assert-Contains $hotkeys 'bool ValidCaptureChord(' 'a chord is rejected when it cannot be used'
Assert-Contains $hotkeys 'bool IsCaptureCancel(' 'Esc and the controller back button cancel a recording'
Assert-Contains $hotkeys 'void AdvanceCaptureLocked()' 'the state machine advances in one place'
Assert-Contains $hotkeys 'void FailCaptureLocked(' 'a rejected chord is reported instead of silently kept'
Assert-Contains $hotkeys 'kCaptureTimeoutMs' 'a forgotten recording does not lock the page forever'

# ---- the recorder is modal: the whole batch is eaten ----
Assert-Contains $hotkeys 'bool FeedEvent(RE::InputEvent* a_event)' 'one place feeds an event to the recorder'
Assert-Contains $hotkeys 'void FeedInputEvent(RE::InputEvent* a_event)' 'the menu callback feeds the recorder as well'
Assert-Contains $hotkeys 'if (g_capturing.load(std::memory_order_relaxed)) {' 'the sink asks the recorder before anything else'
Assert-Contains $hotkeys 'if (FeedEvent(event)) {' 'the event that ends a recording is the one the recorder sees'
Assert-Contains $hotkeys 'swallow = true;
						continue;' 'the recording eats the event instead of handling it'
Assert-Contains $hotkeys 'void FeedInputEvent(RE::InputEvent* a_event)
	{
		if (!g_capturing' 'feeding without a recording does nothing'
Assert-Contains $hotkeysH 'void FeedInputEvent(RE::InputEvent* a_event);' 'the recorder feed is declared for the menu'
Assert-Contains $hotkeysH 'class InputEvent;' 'the declaration needs no engine header'

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
Write-Output 'HOTKEY_CAPTURE_OK: firing comes from the input sink only, an open menu mutes every binding, one key table feeds parse/format/poll, the power key is read in the button-mask space and consumed, and a recording settles after a quiet moment'
exit 0
