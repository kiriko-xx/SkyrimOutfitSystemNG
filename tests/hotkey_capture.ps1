$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.70 hotkey recorder and the Change Outfit power key.
# What has to stay true:
#  - the recorder is a window drawn after the page, and the page underneath is
#    disabled and greyed out while it is open (FreeClimb does the same)
#  - the host stops accepting input for the whole recording, not only when the
#    framework reports a close callback: swallowing raw events is not enough
#  - a row keeps one binding per device, so recording the keyboard never replaces
#    the controller binding and both fire
#  - the recording only settles once every key is released, and it ends when the
#    menu closes or the player presses Esc / the controller back button
#  - the Shout/Power key is acted on only while our own power is the selected one,
#    and that press is consumed so the game never casts it (no cast animation)
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

# ---- the recorder owns one device at a time, but both bindings stay set ----
$hotkeysH = Read-RepoFile 'src/cpp/include/NGHotkeys.h'
Assert-Contains $hotkeysH 'enum class Input' 'the recorder knows which device it is recording'
Assert-Contains $hotkeysH 'void BeginCapture(Slot a_slot, Input a_device' 'a recording starts for one device'
Assert-Contains $hotkeysH 'Input CaptureDevice();' 'the recorder window can show what it is recording'
Assert-Contains $hotkeysH 'bool CaptureArmed();' 'the window can tell waiting from listening'
Assert-Contains $hotkeysH 'int CaptureProblem();' 'the window can explain a rejected combination'
Assert-Contains $hotkeysH 'bool TakeCaptureResult(Slot& a_slot, Input& a_device' 'the result says which device it belongs to'

$settingsH = Read-RepoFile 'src/core/Settings.h'
Assert-Contains $settingsH 'randomOutfitGamepadHotkey' 'the random outfit keeps a controller binding of its own'
Assert-Contains $settingsH 'quickPickerGamepadHotkey' 'the quick picker keeps a controller binding of its own'

$settings = Read-RepoFile 'src/core/Settings.cpp'
Assert-Contains $settings '"HotkeyGamepad"' 'the controller binding is read from the ini'
Assert-Contains $settings '"QuickPickerHotkeyGamepad"' 'the quick picker controller binding is read from the ini'

$hotkeys = Read-RepoFile 'src/cpp/NGHotkeys.cpp'
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
Assert-Contains $hotkeys 'bool ValidCaptureChord(' 'a chord is rejected when it cannot be used'
Assert-Contains $hotkeys 'bool IsCaptureCancel(' 'Esc and the controller back button cancel a recording'
Assert-Contains $hotkeys 'void AdvanceCaptureLocked()' 'the state machine advances in one place'
Assert-Contains $hotkeys 'void FailCaptureLocked(' 'a rejected chord is reported instead of silently kept'
Assert-Contains $hotkeys 'kCaptureTimeoutMs' 'a forgotten recording does not lock the page forever'
Assert-Contains $hotkeys 'bool OurPowerSelected()' 'the plugin checks its own power is the selected one'
Assert-Contains $hotkeys 'SkyrimOutfitSystemQuickslotSpell' 'the power is looked up by editor id'
Assert-Contains $hotkeys 'selectedPower' 'the check is against the selected power of the player'
Assert-Contains $hotkeys 'bool IsPowerKey(const Trigger& a_trigger)' 'the Shout/Power key is recognised'
Assert-Contains $hotkeys 'if (!OurPowerSelected()) {' 'nothing happens while another power is equipped'
Assert-Contains $hotkeys 'swallow = true;' 'that press is consumed, so the game does not cast the power'
Assert-Contains $hotkeys 'kQuickslotOpenCooldownMs' 'only our own timer throttles the power key'

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
Assert-Contains $menu 'Hotkeys::BeginCapture(a_slot, Hotkeys::Input::kKeyboard)' 'a row can record the keyboard and the mouse'
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
Assert-Contains $menu 'void __stdcall OnFrameworkMenuEvent(int a_eventType)' 'the menu lifecycle is watched'
Assert-Contains $menu 'a_eventType != MenuBridge::kEventMenuClosed' 'only the close notification is used'
Assert-Contains $menu 'MenuBridge::RegisterEventCallback(OnFrameworkMenuEvent);' 'the lifecycle callback is registered'
Assert-Contains $menu 'IG::SetMenuInputSuppressed(false);' 'a recording that ends with the menu gives input back'

$bridgeH = Read-RepoFile 'src/cpp/include/MenuBridge.h'
Assert-Contains $bridgeH 'typedef void(__stdcall* EventCallback)(int a_eventType);' 'the framework event callback is declared'
Assert-Contains $bridgeH 'kEventMenuClosed = 2' 'the close notification keeps its framework value'
Assert-Contains $bridgeH 'bool RegisterEventCallback(EventCallback a_callback);' 'the callback can be registered'

$bridge = Read-RepoFile 'src/cpp/MenuBridge.cpp'
Assert-Contains $bridge '"RegisterEventPriority"' 'the framework entry point is resolved by name'
Assert-Contains $bridge 'std::int64_t (*)(EventCallback, float)' 'the export signature is the one the framework declares'

if ($failures -gt 0) {
    Write-Output "HOTKEY_CAPTURE_FAILED: $failures problem(s)"
    exit 1
}
Write-Output 'HOTKEY_CAPTURE_OK: recorder locks the page, both device bindings stay set, host input suppressed while recording, power key only while the Change Outfit power is selected and consumed on the press'
exit 0
