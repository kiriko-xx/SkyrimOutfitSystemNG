$ErrorActionPreference = 'Stop'
# Static guard for the 1.5.69 hotkey recorder and the Change Outfit power key.
# What has to stay true:
#  - a row has one recorder per device, so a pad button is not mistaken for a key
#  - the recorder is its own window again and the host stops reading input while it
#    is open (swallowing raw input events alone does not stop ImGui navigation)
#  - a recording ends when the menu closes, or the host keeps withholding input
#  - the Shout/Power key is only acted on while our own power is the selected one,
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

$hotkeysH = Read-RepoFile 'src/cpp/include/NGHotkeys.h'
Assert-Contains $hotkeysH 'enum class Input' 'the recorder knows which device it is recording'
Assert-Contains $hotkeysH 'void BeginCapture(Slot a_slot, Input a_device' 'a recording starts for one device'
Assert-Contains $hotkeysH 'Input CaptureDevice();' 'the recorder window can show what it is recording'

$hotkeys = Read-RepoFile 'src/cpp/NGHotkeys.cpp'
Assert-Contains $hotkeys 'g_captureDevice' 'the recorded device is remembered'
Assert-Contains $hotkeys 'if (a_trigger.device != wanted)' 'the other device is ignored while recording'
Assert-Contains $hotkeys 'bool OurPowerSelected()' 'the plugin checks its own power is the selected one'
Assert-Contains $hotkeys 'SkyrimOutfitSystemQuickslotSpell' 'the power is looked up by editor id'
Assert-Contains $hotkeys 'selectedPower' 'the check is against the selected power of the player'
Assert-Contains $hotkeys 'bool IsPowerKey(const Trigger& a_trigger)' 'the Shout/Power key is recognised'
Assert-Contains $hotkeys 'if (!OurPowerSelected()) {' 'nothing happens while another power is equipped'
Assert-Contains $hotkeys 'if (HandlePowerPress(inputBlocked, menuOpen || consoleOpen)) {' 'the power key press is handled on the press itself'
Assert-Contains $hotkeys 'swallow = true;' 'that press is consumed, so the game does not cast the power'
Assert-Contains $hotkeys 'kQuickslotOpenCooldownMs' 'only our own timer throttles the power key'

$imGuiH = Read-RepoFile 'src/cpp/include/ImGuiApi.h'
Assert-Contains $imGuiH 'ImGuiIO_SetAppAcceptingEvents' 'the host input can be switched off'
Assert-Contains $imGuiH 'ImGuiIO_ClearEventsQueue' 'queued input is dropped when a recording starts'
Assert-Contains $imGuiH 'ImGuiIO_ClearInputKeys' 'held keys are cleared when a recording starts'
Assert-Contains $imGuiH 'hasInputSuppression' 'the suppression is optional, it depends on the host exports'
Assert-Contains $imGuiH 'void SetMenuInputSuppressed(bool' 'one entry point for suppressing host input'
Assert-Contains $imGuiH 'void SetNextWindowFocus()' 'the recorder window can raise itself'

$imGui = Read-RepoFile 'src/cpp/ImGuiApi.cpp'
Assert-Contains $imGui '"igGetIO"' 'the ImGuiIO is resolved at run time'
Assert-Contains $imGui '"ImGuiIO_SetAppAcceptingEvents"' 'the accepting-events switch is resolved'
Assert-Contains $imGui '"ImGuiIO_ClearEventsQueue"' 'the event queue drop is resolved'
Assert-Contains $imGui '"ImGuiIO_ClearInputKeys"' 'the input key clear is resolved'
Assert-Contains $imGui '"igSetNextWindowFocus"' 'raising the window is resolved'
Assert-Absent $imGui 'hasExtras = extrasTotal == extrasResolved &&' 'the new bindings are optional and do not gate the extras'

$menu = Read-RepoFile 'src/cpp/MenuUI.cpp'
Assert-Contains $menu 'Hotkeys::BeginCapture(a_slot, Hotkeys::Input::kKeyboard)' 'a row can record the keyboard and the mouse'
Assert-Contains $menu 'Hotkeys::BeginCapture(a_slot, Hotkeys::Input::kGamepad)' 'a row can record the controller'
Assert-Contains $menu 'IG::PushID(static_cast<int>(a_slot));' 'rows with the same label keep separate widget ids'
Assert-Contains $menu 'IG::SetNextWindowFocus();' 'the recorder window is raised above the settings page'
Assert-Contains $menu 'IG::BeginWindow(I18n::T("Set a hotkey"' 'the recorder is its own window'
Assert-Contains $menu 'Hotkeys::CaptureDevice() == Hotkeys::Input::kGamepad' 'the window says which device is being recorded'
Assert-Contains $menu 'IG::SetMenuInputSuppressed(Hotkeys::CaptureActive() && MenuBridge::EventCallbackAvailable());' 'the host stops reading input only when the menu reports being closed'
Assert-Contains $menu 'void __stdcall OnFrameworkMenuEvent(int a_eventType)' 'the menu lifecycle is watched'
Assert-Contains $menu 'a_eventType != MenuBridge::kEventMenuClosed' 'only the close notification is used'
Assert-Contains $menu 'MenuBridge::RegisterEventCallback(OnFrameworkMenuEvent);' 'the lifecycle callback is registered'
Assert-Contains $menu 'IG::SetMenuInputSuppressed(false);' 'a recording that ends with the menu gives input back'

$bridgeH = Read-RepoFile 'src/cpp/include/MenuBridge.h'
Assert-Contains $bridgeH 'typedef void(__stdcall* EventCallback)(int a_eventType);' 'the framework event callback is declared'
Assert-Contains $bridgeH 'kEventMenuClosed = 2' 'the close notification keeps its framework value'
Assert-Contains $bridgeH 'bool RegisterEventCallback(EventCallback a_callback);' 'the callback can be registered'
Assert-Contains $bridgeH 'bool EventCallbackAvailable();' 'the plugin can tell whether that callback exists'

$bridge = Read-RepoFile 'src/cpp/MenuBridge.cpp'
Assert-Contains $bridge '"RegisterEventPriority"' 'the framework entry point is resolved by name'
Assert-Contains $bridge 'std::int64_t (*)(EventCallback, float)' 'the export signature is the one the framework declares'

if ($failures -gt 0) {
    Write-Output "HOTKEY_CAPTURE_FAILED: $failures problem(s)"
    exit 1
}
Write-Output 'HOTKEY_CAPTURE_OK: per-device recorder window, host input suppressed while recording, power key only while the Change Outfit power is selected and consumed on the press'
exit 0
