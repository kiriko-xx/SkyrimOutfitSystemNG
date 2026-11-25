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
#  - the recorder is FreeClimb's, copied verbatim into input/FreeClimbCapture.h:
#    it looks at what is held right now, nothing is accepted before the device is
#    neutral, and a chord is stored only once every key of it was released - a
#    modifier pressed on its way to Alt+O is therefore never stored on its own
#  - while a recording runs, the recorded keys are taken off the engine's input
#    queue the way FreeClimb does it: the keyboard poll and the pad poll are hooked
#    through the vtable constants CommonLibSSE-NG publishes
#    (RE::VTABLE_BSWin32KeyboardDevice / RE::VTABLE_BSPCGamepadDeviceHandler, no
#    hand written address-library id), the original poll still runs first, and the
#    events are unlinked before anything is dispatched - so Escape, Back and the
#    shoulder buttons reach the recording instead of the framework menu. Both hooks
#    are best effort and say so in the log; with no recording running they return
#    without touching the queue. The framework and ImGui suppression stays as the
#    second layer (SetMenuInputSuppressed drops the events, clears the held keys and
#    stops the host from accepting events)
#  - a chord is checked against the devices themselves, never against what the
#    sink happened to see: our own window swallows the key release, so a key that
#    stayed in the sink's idea of "held" used to make a recorded Alt+O fire on Alt
#    alone and a recorded LB+up fire on LB alone
#  - the pad state has to be the structure XInput writes, with the buttons as a
#    WORD: reading them as a DWORD moved both trigger axes onto a stick and made
#    LT and RT look released for ever, in both recorder windows
#  - a controller chord is shown in the order its buttons went down in, not in
#    bit order
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
# ---- the state the window shows is a mirror of the copied recorder ----
Assert-Contains $hotkeys 'enum class CaptureState' 'the window still has its own names for the states'
Assert-Contains $hotkeys 'g_captureState.store(static_cast<int>(CaptureState::kCaptured)' 'a finished chord is flagged for the window'
Assert-Contains $hotkeys 'bool CaptureArmed()' 'the window can tell waiting from listening'
Assert-Contains $hotkeys 'return state == CaptureState::kArmed;' 'listening is read from the mirrored state'
Assert-Contains $hotkeys 'bool IsCaptureCancel(' 'the keys that end a recording are named in one place'
Assert-Contains $hotkeys 'bool EventAsksCancel(RE::InputEvent* a_event)' 'Esc and Back are recognised on the events as well'
Assert-Contains $hotkeys 'kCaptureTimeoutMs' 'a forgotten recording does not lock the page forever'
Assert-Absent $hotkeys 'AdvanceCaptureLocked' 'the chord is not accumulated key by key'
Assert-Absent $hotkeys 'PromoteCaptureLocked' 'the waiting phase ends on a snapshot, not on a timer'
Assert-Absent $hotkeys 'kCaptureSettleMs' 'no quiet moment ends a chord'

# ---- the recorded keys are taken off the queue before anything is dispatched ----
$filter = Read-RepoFile 'src/cpp/InputQueueFilter.cpp'
Assert-Contains $hotkeys 'InstallInputFilters();' 'the queue filter is installed together with the input sink'
Assert-Contains $hotkeysH 'void InstallInputFilters();' 'the filter belongs to the hotkey interface'
Assert-Contains $hotkeysH 'void ResetInputOwnership();' 'the filter can forget what an earlier recording took'
Assert-Contains $hotkeys 'ResetInputOwnership();' 'a new recording starts with nothing marked as taken'
Assert-Contains $filter 'RE::VTABLE_BSWin32KeyboardDevice[0]' 'the keyboard vtable comes from the CommonLib constant'
Assert-Contains $filter 'RE::VTABLE_BSPCGamepadDeviceHandler[0]' 'the pad vtable comes from the CommonLib constant'
Assert-Absent $filter '285449' 'no hand written keyboard table id'
Assert-Absent $filter '285457' 'no hand written pad table id'
Assert-Contains $filter 'constexpr std::size_t kKeyboardPollSlot = 2;' 'the keyboard poll is the third vtable entry'
Assert-Contains $filter 'constexpr std::size_t kGamepadPollSlot = 2;' 'the pad poll is the third vtable entry'
Assert-Contains $filter 'void RemoveInputEvents(' 'the queue walk is the copied one'
Assert-Contains $filter 'queue->GetQueueHead(), queue->GetQueueTail()' 'the engine input queue is what is walked'
Assert-Contains $filter 'if (!CaptureActive() && !g_ownership.Pending()) {' 'nothing is filtered unless a recording runs or a taken key is still open'
Assert-Contains $filter 'const bool capturing = CaptureActive();' 'the recorder decides whether the queue is filtered'
Assert-Contains $filter 'button->IsDown(), button->IsUp(), capturing' 'press and release are both handed to the filter'
Assert-Contains $filter 'if (a_up) {' 'the release of a taken key is taken as well'
Assert-Contains $filter '_owned.set(index, a_capturing);' 'a taken key stays marked until it is released'
Assert-Contains $filter 'SKSE::InputMap::GamepadMaskToKeycode(code)' 'a pad button mask is turned into its key code'
Assert-Contains $filter 'SKSE::InputMap::kMacro_GamepadOffset' 'the gamepad offset is removed before the filter sees the code'
Assert-Contains $filter '::VirtualProtect(location, sizeof(void*), PAGE_EXECUTE_READWRITE, &previous)' 'the vtable slot is written through a writable page'
Assert-Contains $filter '::InterlockedCompareExchangePointer(' 'the slot is replaced atomically'
Assert-Contains $filter '::VirtualQuery(' 'the slot is checked before it is touched'
Assert-Contains $filter 'g_keyboardPoll(a_device, a_timeDelta);' 'the original keyboard poll still runs first'
Assert-Contains $filter 'g_gamepadPoll(a_handler, a_timeDelta);' 'the original pad poll still runs first'
Assert-Contains $filter 'the keyboard poll slot could not be hooked' 'a missing slot is reported instead of assumed'
Assert-Contains $filter 'the controller poll slot could not be hooked' 'a missing slot is reported instead of assumed'
Assert-Absent $hotkeys 'BSInputEventQueue' 'the hotkey file itself never walks the queue'
Assert-Absent $hotkeys 'VTableHook' 'no home made vtable hook class is back'
Assert-Absent $hotkeys 'InterruptCast' 'no extra address-library id is added just to stop a cast'

$fcC = Read-RepoFile 'src/cpp/include/input/FreeClimbCapture.h'
Assert-Contains $hotkeys '#include "input/FreeClimbCapture.h"' 'the copied recorder is what the hotkeys use'
Assert-Contains $fcC 'Source: https://github.com/Eelta/FreeClimb' 'the recorder names the project it was copied from'
Assert-Contains $fcC 'commit 8a48ffe' 'the copy names the commit it came from'
Assert-Contains $fcC 'class BindingCapture' 'the copied state machine is present'
Assert-Contains $fcC 'inline bool validKeyChord(const KeyChord& chord)' 'the keyboard chord rules are the copied ones'
Assert-Contains $fcC 'if (chordHasGroup(chord, anyAlt) && (chordHasGroup(chord, 0x0f) || chordHasGroup(chord, 0x3e))) return false;' 'Alt+Tab and Alt+F4 are refused'
Assert-Contains $fcC 'if (chordHasGroup(chord, anyAlt) && chordHasGroup(chord, anyControl) && chordHasGroup(chord, 0xd3)) return false;' 'Ctrl+Alt+Del is refused'
Assert-Contains $fcC 'return chord && !(chord & (gamepadButtonMask(4) | gamepadButtonMask(5))) && std::popcount(chord) <= 4;' 'Start and Back never end up in a pad chord, four buttons are the limit'
Assert-Contains $fcC 'if (snapshot.keyboardValid && snapshot.keyboard.held(0x01)) return cancel();' 'Escape cancels inside the recorder itself'
Assert-Contains $fcC 'if (snapshot.gamepadValid && (snapshot.gamepad & gamepadButtonMask(5))) return cancel();' 'the pad Back button cancels inside the recorder itself'
Assert-Contains $fcC 'if (pressed.none() && !snapshot.activationHeld) value.status = BindingCaptureStatus::armed;' 'nothing is accepted while the device is not neutral'
Assert-Contains $fcC 'if (pressed.none()) {' 'an empty device is what ends a chord'
Assert-Contains $fcC 'if (candidate.any()) value.status = BindingCaptureStatus::captured;' 'the chord lands only after every key of it was released'
Assert-Contains $fcC 'if ((pressed & ~candidate).none()) return value;' 'a key that was already part of the chord is not a new chord'
Assert-Contains $fcC 'if ((pressed & candidate) != candidate) return finish(BindingCaptureStatus::error, BindingCaptureError::notSimultaneous);' 'keys pressed one after the other are refused'
Assert-Contains $fcC 'if (pressed.count() > 4) return finish(BindingCaptureStatus::error, BindingCaptureError::tooMany);' 'more than four keys is refused'

# ---- the recorder is fed by a poll and drives nothing in the engine ----
Assert-Contains $hotkeys 'fc::BindingCapture g_capture;' 'the copied recorder is the one running'
Assert-Contains $hotkeys 'void PollDevices(fc::BindingCaptureSnapshot& a_snapshot)' 'one snapshot of both devices is taken'
Assert-Contains $hotkeys 'fc::BindingCaptureSnapshot snapshot;' 'the snapshot is the only input the recorder gets'
Assert-Contains $hotkeys 'g_capture.sample(snapshot)' 'the state machine is stepped once per frame'
Assert-Contains $hotkeys 'a_snapshot.keyboardValid = true;' 'the keyboard half is always there'
Assert-Contains $hotkeys 'a_snapshot.gamepadValid = lastPadValid;' 'the pad half is only used when XInput answered'
Assert-Contains $hotkeys 'a_snapshot.activationHeld = !neutral;' 'the click that opened the recorder stays out of the chord'
Assert-Contains $hotkeys 'StoreCaptureLocked(result)' 'a finished chord is stored in one place'
Assert-Contains $hotkeys 'SyncCaptureLocked(result)' 'what the recorder holds is mirrored for the window'
Assert-Contains $hotkeys 'g_capture.begin(wanted == Dev::kGamepad ? fc::BindingCaptureDevice::gamepad : fc::BindingCaptureDevice::keyboard);' 'a recording starts in the waiting state'
$takeResult = Get-Block $hotkeys 'bool TakeCaptureResult(Slot& a_slot, Input& a_device' "`t`treturn true;"
Assert-Absent $takeResult 'g_capture.reset();' 'the window asking for a result never resets the running recorder'
$cancelCapture = Get-Block $hotkeys 'void CancelCapture()' 'void FeedInputEvent'
Assert-Contains $cancelCapture 'g_capture.reset();' 'the recorder is reset when a recording ends'
$pollCapture = Get-Block $hotkeys 'void PollCapture()' 'bool IsHeld('
Assert-Contains $pollCapture 'g_capturing.store(false, std::memory_order_relaxed);' 'a finished recording stops taking keys off the queue at once'
Assert-Contains $hotkeys 'case fc::BindingCaptureStatus::error:' 'a refused chord is explained instead of dropped'
Assert-Contains $hotkeys 'g_captureProblem.store(ProblemOf(result.error)' 'the reason of a refusal reaches the window'
Assert-Contains $hotkeys 'case fc::BindingCaptureStatus::cancelled:' 'Escape and Back end a recording through the recorder itself'

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


# ---- a chord is checked against the devices, not against the sink's memory ----
$downBlock = Get-Block $hotkeys 'bool Down(const Trigger& a_trigger)' 'bool AllDown(const Chord& a_chord)'
Assert-Contains $downBlock 'return PadDown(a_trigger);' 'a controller chord is checked against the pad itself'
Assert-Contains $downBlock 'vk = VkForScan(a_trigger.code);' 'a keyboard chord is checked against the keyboard itself'
Assert-Contains $downBlock '(::GetAsyncKeyState(vk) & 0x8000) != 0' 'a key only counts as down while the keyboard reports it'
Assert-Contains $downBlock 'if (vk != 0) {' 'the key table decides, the sink memory is only the last resort'
Assert-Contains $hotkeys 'int VkForScan(std::uint32_t a_code)' 'a scan code is turned into a virtual key through the key table'
$isHeldBlock = Get-Block $hotkeys 'bool IsHeld(const Trigger& a_trigger)' 'The key the player has actually bound to Shout/Power'
Assert-Contains $isHeldBlock 'return Down(a_trigger);' 'the polled path asks the devices as well'

# ---- the pad state is the structure XInput writes ----
$padState = Get-Block $hotkeys 'struct PadState' 'bool ReadPad('
Assert-Contains $padState 'std::uint16_t buttons;' 'the buttons are the WORD the engine writes'
Assert-Absent $padState 'std::uint32_t buttons;' 'reading the buttons as a DWORD moved both triggers onto a stick axis'
Assert-Contains $padState 'leftTrigger;' 'the left trigger is read as its own byte'
Assert-Contains $padState 'rightTrigger;' 'the right trigger is read as its own byte'
Assert-Contains $hotkeys 'static_assert(sizeof(PadState) == 16' 'the pad state has to be as large as the structure the engine writes'
Assert-Contains $hotkeys 'PadSnapshot PhysicalPad()' 'the pad is read once per chord check'
Assert-Contains $hotkeys 'if (ReadPad(pad)) {' 'the pad is read through XInput'
Assert-Absent $hotkeys 'return PadSnapshot{ pad, true };' 'a failed read does not pretend the pad is neutral'
Assert-Contains $hotkeys 'bool PadDown(const Trigger& a_trigger)' 'a pad button is checked against the pad itself'
Assert-Contains $hotkeys 'return pad.state.leftTrigger > kPadTriggerOn;' 'the left trigger is compared as an axis'
Assert-Contains $hotkeys 'return pad.state.rightTrigger > kPadTriggerOn;' 'the right trigger is compared as an axis'
Assert-Contains $hotkeys 'return (pad.state.buttons & a_trigger.code) != 0;' 'a real button is compared against the button mask'

# ---- a controller chord keeps the order its buttons went down in ----
Assert-Contains $hotkeys 'std::vector<unsigned> g_captureOrder;' 'the order of a pad chord is remembered'
Assert-Contains $hotkeys 'void RememberCaptureOrderLocked(const fc::BindingCaptureResult& a_result)' 'the order is filled from the recorder snapshot'
Assert-Contains $hotkeys 'RememberCaptureOrderLocked(result);' 'every snapshot feeds the order'
$triggersOf = Get-Block $hotkeys 'std::vector<Trigger> TriggersOf(' 'int ProblemOf('
Assert-Contains $triggersOf 'for (const auto index : g_captureOrder) {' 'the pad chord is listed in the order the buttons went down'
Assert-Contains $triggersOf 'std::find(g_captureOrder.begin(), g_captureOrder.end(), i) != g_captureOrder.end()' 'a button the order already knows is not listed twice'
if (([regex]::Matches($hotkeys, 'g_captureOrder\.clear\(\)')).Count -ne 4) {
    Write-Output 'FAIL the recorded order is dropped in every path that ends a recording'
    $script:failures++
}
Assert-Contains $takeResult 'g_captureOrder.clear();' 'the order does not survive into the next recording'

if ($failures -gt 0) {
    Write-Output "HOTKEY_CAPTURE_FAILED: $failures problem(s)"
    exit 1
}
Write-Output 'HOTKEY_CAPTURE_OK: firing comes from the input sink only, an open menu mutes every binding, one key table feeds parse/format/poll, the power key is read in the button-mask space and consumed, and the recorder is the FreeClimb state machine copied into input/FreeClimbCapture.h: the recorded keys are taken off the engine queue by two best-effort device poll hooks (CommonLib vtable constants, nothing else hooked, idle when no recording runs), nothing is taken until the device is neutral, and a chord lands only once every key of it is released, a chord is then checked against the devices themselves so an eaten key release cannot leave a key behind, the pad state is the structure XInput writes (LT and RT included), and a pad chord keeps the order its buttons went down in'
exit 0
