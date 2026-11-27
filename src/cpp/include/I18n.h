#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Runtime localisation for the menu and the quick picker.
//
// The user asked for a language switch (English / Chinese) in the settings
// menu.  Two very different problems hide behind that request:
//
//   1. the strings - solved here with a small static dictionary that is
//      applied inside the IG:: wrappers, so the panel code keeps writing plain
//      English literals;
//   2. the glyphs - the menu framework draws the text, so Chinese only shows
//      up if the font the framework loaded has CJK glyphs.  That font is the
//      framework's business: it reads [Fonts] in SKSEMenuFramework.ini (and
//      appends the extension itself), loads the file and hands it to ImGui.
//      So instead of guessing at file names the way 1.5.81 did - which
//      switched Chinese off for a font that was loaded and working, because
//      the ini value carried no extension - ChineseAvailable() asks the loaded
//      font for a CJK glyph through the framework's own igGetFont /
//      ImFont_FindGlyphNoFallback.  Until the menu has been drawn once the
//      answer is "not known yet" and nothing is switched off.
//
// Nothing here hooks or patches the framework: it only reads its ini and asks
// the framework which glyphs its font can draw, both of which are public.

namespace SOS::I18n
{
	// Resolves the language once Settings are loaded (kDataLoaded). Safe to
	// call again after a setting changed.
	void Init();
	void Refresh();

	// "auto" or the suffix of the active language file, as stored in the ini.
	const char* ModeName();

	// The language that is actually in use, e.g. "english".
	const char* CurrentCode();

	// Every language that has a file in Data/interface/translations, sorted.
	std::vector<std::string> Languages();

	// Display name of a language code: "english" -> "English", "chinese" ->
	// "中文", anything unknown gets its first letter capitalised.
	const char* LabelForCode(const char* a_code);

	// Switches the interface language. Pass "auto" to follow the language picked
	// on the SKSE Menu Framework settings page (the [Fonts] switches in its ini).
	// The value is written to Settings by the caller when it should persist.
	void SetLanguage(const char* a_code);

	// True when the effective language is Chinese and the glyphs are there.
	bool Chinese();

	// True when the menu framework can actually draw Chinese. Reads the font the
	// framework loaded, so it is only meaningful once the menu is open; before that
	// it answers "yes" rather than taking the language away.
	bool ChineseAvailable();

	// Re-asks the menu font whether it can draw Chinese and re-applies the language
	// when the answer changed. Call it while the menu is open (kEventMenuOpened, or
	// the first frame of the quick picker): igGetFont reads the host's ImGui context.
	void RefreshFontProbe();

	// Short status line for the log: "english (auto)", "chinese (forced)", ...
	const char* Status();

	// Translates a literal. Returns a_en unchanged when the active language has
	// no line for it, so call sites never need a fallback.
	const char* Tr(const char* a_en);

	// Same, for widget labels: the returned pointer may be "chinese###english"
	// so ImGui keeps using the English text as the widget id. That way widget
	// state survives a language switch and two different English labels can
	// never collide after translation.
	const char* TrId(const char* a_en);

	// Explicit pair, for messages with arguments the dictionary does not cover.
	// The active language file still wins over both.
	const char* T(const char* a_en, const char* a_zh);

	// ---- per-language translation files ---------------------------------
	//
	// Data/interface/translations/SkyrimOutfitSystemNG_<language>.txt
	//
	// One file per language, in the same style as the game's own translation
	// files ($English literal<TAB>translation, UTF-16LE). Adding a new
	// language is just another file: SkyrimOutfitSystemNG_german.txt shows up
	// in the language dropdown on the next start. The two files that ship with
	// the mod are written from the built-in dictionary the first time the
	// plugin runs, so a translator always starts from a complete list.
	const char* FilePath();

	// Re-reads every language file and re-resolves the language. Called from
	// Init() and from the "Reload the language files" button.
	void ReloadFile();

	// Number of lines the active language file contributed.
	std::size_t OverrideCount();

	// Short status line, e.g. "212 lines from SkyrimOutfitSystemNG_chinese.txt".
	const char* FileStatus();
}
