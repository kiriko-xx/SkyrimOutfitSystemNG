#include "MenuOutfitEditor.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include "I18n.h"
#include "ImGuiApi.h"
#include "MenuUI.h"
#include "NGFeatures.h"
#include "Utility.h"
#include "bindings.h"
#include "core/Outfit.h"
#include "core/OutfitService.h"
#include "core/PluginScan.h"
#include "core/Policy.h"
#include "core/SlotRules.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace SOS::Menu::Editor
{
	namespace
	{
		// BOD2 editor indices 30..61, i.e. the same order the native slot
		// listing uses (RE::BIPED_OBJECTS::kEditorHead..kEditorTotal - 1).
		struct SlotLabel
		{
			const char* en;
			const char* zh;
		};

		constexpr SlotLabel kSlotLabels[32]{
			{ "Head", "头部" },
			{ "Hair, Short", "头发, 短" },
			{ "Body", "身体" },
			{ "Hands", "手" },
			{ "Forearms", "前臂" },
			{ "Amulet", "项链" },
			{ "Ring", "戒指" },
			{ "Feet", "脚" },
			{ "Calves", "小腿" },
			{ "Shield", "盾牌" },
			{ "Tail", "尾巴" },
			{ "Hair, Long", "头发, 长" },
			{ "Circlet", "头环" },
			{ "Ears", "耳朵" },
			{ "Unnamed (probably Face)", "未知 (一般来说是 面部)" },
			{ "Unnamed (probably Neck)", "未知 (一般来说是 颈部)" },
			{ "Unnamed (probably Chest)", "未知 (一般来说是 胸部)" },
			{ "Unnamed (probably Back)", "未知 (一般来说是 背部)" },
			{ "Unnamed (probably Other)", "未知 (一般来说是 其他)" },
			{ "Unnamed (probably Pelvis outside)", "未知 (一般来说是 骨盆外侧)" },
			{ "Decapitate-Head", "Decapitate-Head" },
			{ "Decapitate", "Decapitate" },
			{ "Unnamed (probably Pelvis below)", "未知 (一般来说是 骨盆下方)" },
			{ "Unnamed (probably Right Leg)", "未知 (一般来说是 右腿)" },
			{ "Unnamed (probably Left Leg)", "未知 (一般来说是 左腿)" },
			{ "Unnamed (probably Face 2)", "未知 (一般来说是 面部 2)" },
			{ "Unnamed (probably Chest 2)", "未知 (一般来说是 胸部 2)" },
			{ "Unnamed (probably Shoulder)", "未知 (一般来说是 肩膀)" },
			{ "Unnamed (probably Left Arm)", "未知 (一般来说是 左臂)" },
			{ "Unnamed (probably Right Arm)", "未知 (一般来说是 右臂)" },
			{ "Unnamed (probably Other 2)", "未知 (一般来说是 其他 2)" },
			{ "FX", "FX" },
		};

		struct PolicyLabel
		{
			const char* code;
			const char* en;
			const char* zh;
		};

		// Mirrors $SkyOutSys_Desc_EasyPolicyName_* of the original translation
		// file, but as plain text so the ImGui menu can show it directly.
		constexpr PolicyLabel kPolicyLabels[]{
			{ "XXXX", "Never show anything", "从不显示任何东西" },
			{ "XXXE", "If outfit and equipped, show equipped", "幻化和装备都有时显示装备" },
			{ "XXXO", "Require equipped (show the outfit only when the slot is worn)", "需要已装备（槽位有装备时才显示幻化）" },
			{ "XXOX", "If only outfit, show outfit", "只有幻化时显示幻化" },
			{ "XXOE", "If only outfit show outfit; if both show equipped", "只有幻化显示幻化；都有则显示装备" },
			{ "XXOO", "Always use outfit", "始终使用幻化" },
			{ "XEXX", "If only equipped, show equipped", "只有装备时显示装备" },
			{ "XEXE", "If equipped, show equipped", "有装备就显示装备" },
			{ "XEXO", "Passthrough", "穿透" },
			{ "XEOX", "If only equipped show equipped; if only outfit show outfit", "只有装备显示装备；只有幻化显示幻化" },
			{ "XEOE", "Both, but show equipped when both exist", "两者都显示，但都有时显示装备" },
			{ "XEOO", "Both, and show the outfit when both exist", "两者都显示，但都有时显示幻化" },
		};

		// The slot number is appended to the visible text: the editor lists all
		// 32 BOD2 slots, several of them have no official name ("Unnamed ...")
		// and the numbers shown to the user are the ones Creation Kit and xEdit
		// use, i.e. the zero based BOD2 index plus 30 (head = 30, shield = 39,
		// FX = 61).
		constexpr std::uint32_t kDisplaySlotOffset = 30;

		std::string SlotLabelFor(std::uint32_t a_index)
		{
			if (a_index >= 32) {
				return "?";
			}
			std::string label(I18n::T(kSlotLabels[a_index].en, kSlotLabels[a_index].zh));
			char        suffix[32]{};
			std::snprintf(suffix, sizeof(suffix), I18n::Chinese() ? "（槽位 %u）" : " (slot %u)", a_index + kDisplaySlotOffset);
			return label + suffix;
		}

		const char* PolicyLabelFor(const std::string& a_code)
		{
			// Old saves and imported JSON files can hold a localized label
			// instead of the code; normalise before looking the label up.
			const auto code = NormalizePolicyCode(a_code);
			for (const auto& label : kPolicyLabels) {
				if (code == label.code) {
					return I18n::T(label.en, label.zh);
				}
			}
			if (a_code.empty()) {
				return I18n::T("Use base policy", "使用基础策略");
			}
			return I18n::T("Unknown policy", "未知策略");
		}

		std::string ArmorName(const RE::TESObjectARMO* a_armor)
		{
			if (!a_armor) {
				return I18n::T("[Unnamed]", "[未命名]");
			}
			if (const auto* fullName = skyrim_cast<const RE::TESFullName*>(a_armor)) {
				if (fullName->fullName.data() && *fullName->fullName.data()) {
					return fullName->fullName.data();
				}
			}
			return I18n::T("[Unnamed]", "[未命名]");
		}

		std::string LowerCopy(std::string a_text)
		{
			std::transform(a_text.begin(), a_text.end(), a_text.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return a_text;
		}

		// Local form id of an armour, the six digit number the plugin list shows.
		std::string HexLocalIdOf(std::uint32_t a_localId)
		{
			char text[16]{};
			std::snprintf(text, sizeof(text), "%06X", a_localId & 0x00FFFFFFu);
			return text;
		}

		// "32, 37" - the numbers Creation Kit and xEdit show, i.e. the zero based
		// BOD2 index plus 30.
		std::string SlotNumbersText(const RE::TESObjectARMO* a_armor)
		{
			const auto  mask = a_armor ? a_armor->GetSlotMask().underlying() : 0;
			std::string text;
			for (std::uint32_t bit = 0; bit < 32; ++bit) {
				if ((mask & (1u << bit)) == 0) {
					continue;
				}
				char number[8]{};
				std::snprintf(number, sizeof(number), text.empty() ? "%u" : ", %u", bit + kDisplaySlotOffset);
				text += number;
			}
			return text.empty() ? std::string("?") : text;
		}

		// ---- editor state ---------------------------------------------------
		char                      g_search[96]{};
		bool                      g_playableOnly{ true };
		std::string               g_searchSignature;
		std::vector<const RE::TESObjectARMO*> g_results;
		// Which outfits have the per-slot policy editor expanded. This used to
		// be one bool for the whole panel, so ticking the box on one outfit
		// left it open on every other outfit as well. The state is keyed by
		// outfit name; the menu holds it for the session only (it is a view
		// preference, not part of the outfit data), so the exported JSON is
		// untouched.
		std::unordered_set<std::string> g_slotPolicyOpen;
		char                      g_formId[32]{};
		std::string               g_status;
		const RE::TESObjectARMO*  g_pending{ nullptr };
		std::string               g_pendingOutfit;

		// The old MCM asked for confirmation before taking a piece out of an
		// outfit; keep that so a stray click cannot lose the entry.  The prompt
		// is drawn next to the clicked row, not at the top of the panel, because
		// the body-slot list is usually scrolled out of view.
		const RE::TESObjectARMO*  g_pendingRemove{ nullptr };
		std::uint32_t             g_pendingRemoveSlot{ 0 };
		std::string               g_pendingRemoveOutfit;

		// "From a plugin (ESP)": pick a plugin first, then click one of its
		// pieces, the same way the golden items page picks a piece out of a
		// plugin. The piece goes through AddArmor(), so the one-piece-per-slot
		// rule applies exactly as it does for every other way of adding.
		std::string               g_piecePlugin;
		char                      g_piecePluginFilter[96]{};
		char                      g_pieceFilter[96]{};
		bool                      g_piecePickerOpen{ false };
		bool                      g_pieceFreeOnly{ false };

		// Scanning every armour record and building lower-case copies on every
		// keystroke is the most expensive thing this panel does on a large load
		// order. The form array only changes when plugins are loaded, so cache the
		// filtered data and the sorted order for the lifetime of the process.
		struct ArmorCacheEntry
		{
			const RE::TESObjectARMO* armor{ nullptr };
			std::string               name;
			std::string               nameLower;
			std::string               pluginLower;
			bool                      playable{ true };
		};

		std::vector<ArmorCacheEntry> g_armorCache;
		const void*                  g_armorCacheHandler{ nullptr };
		const void*                  g_armorCacheData{ nullptr };
		std::size_t                  g_armorCacheCount{ 0 };
		bool                         g_armorCacheBuilt{ false };

		void RefreshArmorCache()
		{
			auto* handler = RE::TESDataHandler::GetSingleton();
			if (!handler) {
				return;
			}
			auto& list = handler->GetFormArray(RE::FormType::Armor);
			if (g_armorCacheBuilt && handler == g_armorCacheHandler && list.size() == g_armorCacheCount && list.data() == g_armorCacheData) {
				return;
			}

			g_armorCache.clear();
			g_armorCache.reserve(list.size());
			for (std::uint32_t i = 0; i < list.size(); ++i) {
				auto* form = list[i];
				if (!form) {
					continue;
				}
				auto* armor = skyrim_cast<RE::TESObjectARMO*>(form);
				if (!armor || armor->templateArmor) {
					continue;
				}
				const auto name = ArmorName(armor);
				if (name.empty() || name == I18n::T("[Unnamed]", "[未命名]")) {
					continue;
				}
				ArmorCacheEntry entry;
				entry.armor = armor;
				entry.name = name;
				entry.nameLower = LowerCopy(name);
				entry.playable = (armor->formFlags & RE::TESObjectARMO::RecordFlags::kNonPlayable) == 0;
				if (const auto* file = armor->GetFile()) {
					const auto filename = file->GetFilename();
					if (!filename.empty()) {
						entry.pluginLower = LowerCopy(std::string(filename));
					}
				}
				g_armorCache.push_back(std::move(entry));
			}

			std::sort(g_armorCache.begin(), g_armorCache.end(), [](const ArmorCacheEntry& a_lhs, const ArmorCacheEntry& a_rhs) {
				return a_lhs.nameLower < a_rhs.nameLower;
			});
			g_armorCacheHandler = handler;
			g_armorCacheData = list.data();
			g_armorCacheCount = list.size();
			g_armorCacheBuilt = true;
			// The filtered result is cached by search text only; force it to be
			// rebuilt against the new form array after a load or plugin change.
			g_searchSignature.clear();
		}

		void SetStatus(std::string a_text)
		{
			g_status = std::move(a_text);
			SKSE::log::debug("Outfit editor: {}", g_status);
		}

		void RefreshSearch()
		{
			std::string signature = std::string(g_search) + "|" + (g_playableOnly ? "1" : "0");
			if (signature == g_searchSignature) {
				return;
			}

			g_results.clear();
			RefreshArmorCache();
			// RefreshArmorCache may clear the signature when the form array changed,
			// so assign it only after the cache is up to date.
			g_searchSignature = std::move(signature);

			const std::string needle = LowerCopy(g_search);
			for (const auto& entry : g_armorCache) {
				if (g_playableOnly && !entry.playable) {
					continue;
				}
				if (!needle.empty() && entry.nameLower.find(needle) == std::string::npos &&
					entry.pluginLower.find(needle) == std::string::npos) {
					continue;
				}
				g_results.push_back(entry.armor);
			}
		}

		void NotifyChanged(const std::string& a_outfit)
		{
			// InvalidateBrowser also resets the editor state, including the status
			// line that the caller just set. Keep that one message across the reset.
			const std::string status = g_status;
			Menu::InvalidateBrowser();
			g_status = status;
			CreateRefreshAllArmorsTask();
			SKSE::log::debug("Outfit editor: '{}' now has {} pieces.", a_outfit, [&] {
				auto service = outfit_service_get_singleton_ptr();
				const auto* outfit = service->inner().get_outfit_ptr(a_outfit.c_str());
				return outfit ? outfit->armors().size() : 0u;
			}());
		}

		void AddArmor(const std::string& a_outfit, const RE::TESObjectARMO* a_armor)
		{
			if (!a_armor) {
				return;
			}
			auto  service = outfit_service_get_mut_singleton_ptr();
			auto* outfit = service->inner().get_mut_outfit_ptr(a_outfit.c_str());
			if (!outfit) {
				return;
			}
			if (outfit->conflicts_with(a_armor)) {
				g_pending = a_armor;
				g_pendingOutfit = a_outfit;
				return;
			}
			outfit->insert_armor(a_armor);
			SetStatus(std::string(I18n::T("Added ", "已添加 ")) + ArmorName(a_armor) + ".");
			NotifyChanged(a_outfit);
		}

		void RemoveArmor(const std::string& a_outfit, const RE::TESObjectARMO* a_armor)
		{
			auto  service = outfit_service_get_mut_singleton_ptr();
			auto* outfit = service->inner().get_mut_outfit_ptr(a_outfit.c_str());
			if (!outfit || !a_armor) {
				return;
			}
			outfit->erase_armor(a_armor);
			SKSE::log::info("Outfit editor: removed '{}' from '{}'.", ArmorName(a_armor), a_outfit);
			SetStatus(std::string(I18n::T("Removed ", "已移除 ")) + ArmorName(a_armor) + ".");
			NotifyChanged(a_outfit);
		}

		void RenderPendingConflict()
		{
			if (!g_pending) {
				return;
			}
			IG::TextWrapped("%s uses a body slot that is already covered by this outfit. Replace the conflicting pieces?",
				ArmorName(g_pending).c_str());
			if (IG::Button("Replace the conflicting pieces")) {
				auto  service = outfit_service_get_mut_singleton_ptr();
				auto* outfit = service->inner().get_mut_outfit_ptr(g_pendingOutfit.c_str());
				if (outfit) {
					const auto mask = g_pending->GetSlotMask().underlying();
					std::vector<const RE::TESObjectARMO*> conflicting;
					for (const auto* existing : outfit->armors()) {
						if (existing && (existing->GetSlotMask().underlying() & mask) != 0) {
							conflicting.push_back(existing);
						}
					}
					for (const auto* existing : conflicting) {
						outfit->erase_armor(existing);
					}
					outfit->insert_armor(g_pending);
					SetStatus(std::string(I18n::T("Replaced the conflicting pieces with ", "已用以下护甲替换冲突件：")) + ArmorName(g_pending) + ".");
					NotifyChanged(g_pendingOutfit);
				}
				g_pending = nullptr;
			}
			IG::SameLine();
			if (IG::Button("Cancel")) {
				g_pending = nullptr;
			}
		}

		struct CachedArmorList
		{
			std::vector<const RE::TESObjectARMO*> armors;
			std::vector<std::string>             names;
			std::chrono::steady_clock::time_point refreshed{};
			bool                                  valid{ false };
		};

		CachedArmorList g_carriedList;
		CachedArmorList g_wornList;

		const CachedArmorList& GetArmorList(bool a_worn)
		{
			auto& cache = a_worn ? g_wornList : g_carriedList;
			const auto now = std::chrono::steady_clock::now();
			if (!cache.valid || now - cache.refreshed > std::chrono::milliseconds(500)) {
				auto* player = RE::PlayerCharacter::GetSingleton();
				cache.armors = a_worn ? Features::WornArmors(player) : Features::CarriedArmors(player);
				cache.names.clear();
				cache.names.reserve(cache.armors.size());
				for (const auto* armor : cache.armors) {
					cache.names.push_back(ArmorName(armor));
				}
				cache.refreshed = now;
				cache.valid = true;
			}
			return cache;
		}

		void RenderAddSection(const std::string& a_outfit)
		{
			IG::SeparatorText("Add armour");

			if (IG::CollapsingHeader("From the inventory")) {
				const auto& carried = GetArmorList(false);
				if (carried.armors.empty()) {
					IG::TextDisabled("The player carries no armour.");
				} else {
					// BeginChild has to be paired with EndChild even when it returns
					// false (collapsed/clipped). With the empty list the old code
					// called EndChild without a matching BeginChild and corrupted the
					// window stack of the whole menu.
					if (IG::BeginChild("##carried", ImVec2(0.0f, 150.0f), true)) {
						for (std::size_t i = 0; i < carried.armors.size(); ++i) {
							IG::PushID(static_cast<int>(i));
							if (IG::Selectable(carried.names[i].c_str())) {
								AddArmor(a_outfit, carried.armors[i]);
							}
							IG::PopID();
						}
					}
					IG::EndChild();
				}
			}

			if (IG::CollapsingHeader("From the worn armour")) {
				const auto& worn = GetArmorList(true);
				if (worn.armors.empty()) {
					IG::TextDisabled("The player wears no armour.");
				} else {
					if (IG::BeginChild("##worn", ImVec2(0.0f, 150.0f), true)) {
						for (std::size_t i = 0; i < worn.armors.size(); ++i) {
							IG::PushID(static_cast<int>(i));
							if (IG::Selectable(worn.names[i].c_str())) {
								AddArmor(a_outfit, worn.armors[i]);
							}
							IG::PopID();
						}
					}
					IG::EndChild();
				}
			}

			if (IG::CollapsingHeader("By form ID")) {
				IG::TextDisabled("Hexadecimal, for example 00012EB7.");
				IG::SetNextItemWidth(160.0f);
				IG::InputText("##formid", g_formId, sizeof(g_formId));
				IG::SameLine();
				if (IG::Button("Add")) {
					const auto value = std::strtoul(g_formId, nullptr, 16);
					if (value == 0) {
						SetStatus(I18n::T("Enter a form ID such as 00012EB7.", "请输入 FormID，例如 00012EB7。"));
					} else {
						auto* form = RE::TESForm::LookupByID(static_cast<RE::FormID>(value));
						auto* armor = form ? skyrim_cast<RE::TESObjectARMO*>(form) : nullptr;
						if (!form) {
							SetStatus(I18n::T("No form with that ID is loaded.", "没有加载该 ID 的 Form。"));
						} else if (!armor) {
							SetStatus(I18n::T("That form is not armour.", "该 Form 不是护甲。"));
						} else {
							AddArmor(a_outfit, armor);
						}
					}
				}
			}

			if (IG::CollapsingHeader("From the full armour list")) {
				IG::SetNextItemWidth(280.0f);
				IG::InputTextWithHint("##armorsearch", "Filter by armour name or plugin...", g_search, sizeof(g_search));
				IG::SameLine();
				if (IG::Checkbox("Playable only", &g_playableOnly)) {
					g_searchSignature.clear();
				}
				RefreshSearch();
				IG::TextDisabled("%zu pieces match.", g_results.size());
				if (IG::BeginChild("##armorresults", ImVec2(0.0f, 220.0f), true)) {
					constexpr std::size_t kMaxRows = 400;
					const auto            count = std::min(g_results.size(), kMaxRows);
					for (std::size_t i = 0; i < count; ++i) {
						IG::PushID(static_cast<int>(i));
						if (IG::Selectable(ArmorName(g_results[i]).c_str())) {
							AddArmor(a_outfit, g_results[i]);
						}
						IG::PopID();
					}
					if (g_results.size() > count) {
						IG::TextDisabled("Showing the first %zu of %zu pieces - refine the filter.", count, g_results.size());
					}
				}
				IG::EndChild();
			}

			// 1.5.55: pick a single piece out of a plugin instead of hunting for
			// its form id. Same flow as the golden items page: choose the plugin,
			// then click the piece. Pieces whose body slot is already taken by this
			// outfit are marked, and AddArmor() asks before replacing anything.
			if (IG::CollapsingHeader(I18n::T("From a plugin (ESP)", "从 ESP 添加单个部件"))) {
				IG::TextDisabled(I18n::T("Choose a plugin, then click one of its pieces. One piece per body slot: a piece that collides with a slot this outfit already covers is marked and can only be added by replacing the other piece.",
					"先选 ESP，再点其中一件。一个槽位只能放一件：与当前幻化已占槽位冲突的装备会被标出，加入时必须先替换占用该槽位的那件。"));
				IG::SetNextItemWidth(220.0f);
				IG::InputTextWithHint("##piecepluginsearch", I18n::T("Search plugins...", "搜索 ESP..."), g_piecePluginFilter, sizeof(g_piecePluginFilter));
				IG::SameLine();
				if (IG::Button(I18n::T("Choose a plugin...", "选择 ESP..."))) {
					g_piecePickerOpen = !g_piecePickerOpen;
				}
				IG::SameLine();
				IG::TextDisabled("%s", g_piecePlugin.empty() ? I18n::T("(no plugin chosen)", "（未选择 ESP）") : g_piecePlugin.c_str());

				if (g_piecePickerOpen) {
					const auto plugins = PluginScan::PluginsWithArmor();
					const auto needle = LowerCopy(std::string(g_piecePluginFilter));
					if (IG::BeginChild("##pieceplugins", ImVec2(0.0f, 140.0f), true)) {
						std::size_t row = 0;
						// Newest plugin first: the one just installed is the one the
						// user is looking for.
						for (auto it = plugins.rbegin(); it != plugins.rend(); ++it) {
							if (!needle.empty() && LowerCopy(it->name).find(needle) == std::string::npos) {
								continue;
							}
							IG::PushID(static_cast<int>(row++));
							char label[400]{};
							std::snprintf(label, sizeof(label), I18n::T("%s  (%zu pieces, %zu wearable)", "%s  （%zu 件，可幻化 %zu 件）"),
								it->name.c_str(), it->armors, it->outfitArmors);
							if (IG::Selectable(label, g_piecePlugin == it->name)) {
								g_piecePlugin = it->name;
								g_piecePickerOpen = false;
								g_pieceFilter[0] = 0;
							}
							IG::PopID();
						}
					}
					IG::EndChild();
				}

				if (!g_piecePlugin.empty()) {
					const auto armors = PluginScan::ArmorsOf(g_piecePlugin);
					// Slot masks this outfit already covers, read once per frame so the
					// list can mark pieces that would collide instead of letting the
					// click fail without an explanation.
					std::vector<std::uint32_t> takenMasks;
					{
						auto service = outfit_service_get_singleton_ptr();
						if (const auto* outfit = service->inner().get_outfit_ptr(a_outfit.c_str())) {
							takenMasks.reserve(outfit->armors().size());
							for (const auto* existing : outfit->armors()) {
								if (existing) {
									takenMasks.push_back(existing->GetSlotMask().underlying());
								}
							}
						}
					}
					IG::SetNextItemWidth(240.0f);
					IG::InputTextWithHint("##piecefilter", I18n::T("Search pieces...", "搜索装备..."), g_pieceFilter, sizeof(g_pieceFilter));
					IG::SameLine();
					IG::Checkbox(I18n::T("Only free slots", "只显示槽位空闲的"), &g_pieceFreeOnly);
					IG::SameLine();
					IG::TextDisabled(I18n::T("%zu slot armour record(s) in this plugin; click a piece to add it.", "该 ESP 共有 %zu 条槽位护甲记录；点一件即可加入。"), armors.size());
					if (IG::BeginChild("##piecepieces", ImVec2(0.0f, 200.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
						const auto        needle = LowerCopy(std::string(g_pieceFilter));
						constexpr std::size_t kMaxPieceRows = 400;
						std::size_t       shown = 0;
						std::size_t       row = 0;
						for (const auto* armor : armors) {
							if (!armor) {
								continue;
							}
							const char* full = armor->GetFullName();
							const char* editor = armor->GetFormEditorID();
							const char* name = (full && full[0] != 0) ? full : ((editor && editor[0] != 0) ? editor : "");
							const auto  mask = armor->GetSlotMask().underlying();
							bool        occupied = false;
							for (const auto taken : takenMasks) {
								if (SlotsCollide(taken, mask)) {
									occupied = true;
									break;
								}
							}
							if (g_pieceFreeOnly && occupied) {
								continue;
							}
							const auto idText = HexLocalIdOf(PluginScan::LocalIdFor(g_piecePlugin, armor->GetFormID()));
							if (!needle.empty() && LowerCopy(std::string(name)).find(needle) == std::string::npos && LowerCopy(idText).find(needle) == std::string::npos) {
								continue;
							}
							if (shown >= kMaxPieceRows) {
								IG::TextDisabled(I18n::T("Showing the first %zu of %zu pieces - refine the search or tick 「only free slots」.", "只显示前 %zu 件（共 %zu 件）——请缩小搜索或勾选「只显示槽位空闲的」。"), shown, armors.size());
								break;
							}
							++shown;
							char label[640]{};
							std::snprintf(label, sizeof(label), "%s%s  %s",
								occupied ? I18n::T("[slot taken] ", "[槽位已占用] ") : "", name, idText.c_str());
							IG::PushID(static_cast<int>(row++));
							if (IG::Selectable(label)) {
								AddArmor(a_outfit, armor);
							}
							IG::SameLine();
							IG::TextDisabled(I18n::T("[slot %s]", "[槽位 %s]"), SlotNumbersText(armor).c_str());
							IG::PopID();
						}
						if (shown == 0) {
							IG::TextDisabled(I18n::T("No piece of this plugin matches that search.", "该 ESP 里没有匹配的装备。"));
						}
					}
					IG::EndChild();
				}
			}
		}


		struct OutfitView
	{
		std::string                                      name;
		bool                                             favorite{ false };
		std::vector<const RE::TESObjectARMO*>            armors;
		std::string                                      blanket;
		std::array<std::optional<std::string>, 32>       slotPolicies;
	};

	bool SnapshotOutfit(const std::string& a_outfit, OutfitView& a_view)
	{
		auto service = outfit_service_get_singleton_ptr();
		const auto* outfit = service->inner().get_outfit_ptr(a_outfit.c_str());
		if (!outfit) {
			return false;
		}
		a_view.name = std::string(outfit->name_c());
		a_view.favorite = outfit->favorite_c();
		a_view.armors.assign(outfit->armors().begin(), outfit->armors().end());
		a_view.blanket = std::string(outfit->blanket_policy());
		for (std::uint32_t slot = 0; slot < 32; ++slot) {
			a_view.slotPolicies[slot] = outfit->slot_policy(slot);
		}
		return true;
	}

	void RenderSlotList(const std::string& a_outfit, const std::vector<const RE::TESObjectARMO*>& a_armors)
	{
		IG::SeparatorText("Body slots");
		if (!g_pendingRemoveOutfit.empty() && g_pendingRemoveOutfit != a_outfit) {
			g_pendingRemove = nullptr;
			g_pendingRemoveSlot = 0;
			g_pendingRemoveOutfit.clear();
		}
		std::vector<std::pair<std::uint32_t, const RE::TESObjectARMO*>> rows;
		for (const auto* armor : a_armors) {
			if (!armor) {
				continue;
			}
			const auto mask = armor->GetSlotMask().underlying();
			for (std::uint32_t slot = 0; slot < 32; ++slot) {
				if ((mask & (1u << slot)) != 0) {
					rows.emplace_back(slot, armor);
				}
			}
		}
		if (rows.empty()) {
			IG::TextDisabled("This outfit is empty.");
			return;
		}
		if (IG::BeginChild("##slotlist", ImVec2(0.0f, 180.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
			bool removedThisFrame = false;
			for (std::size_t i = 0; i < rows.size(); ++i) {
				IG::PushID(static_cast<int>(i));
				const std::string rowText = SlotLabelFor(rows[i].first) + ": " + ArmorName(rows[i].second);
				const bool confirming = g_pendingRemove == rows[i].second && g_pendingRemoveSlot == rows[i].first;
				if (confirming) {
					IG::TextDisabled("Remove \"%s\" from the outfit?", ArmorName(rows[i].second).c_str());
				}
				IG::Text("%s", rowText.c_str());
				// Keep the buttons inside the visible child even when the armour name is long.
				const float rowWidth = IG::ContentRegionAvail().x;
				const float buttonWidth = IG::CalcTextSize(I18n::T("Remove", "移除")).x + 16.0f;
				const float wantedX = std::max(320.0f, IG::CalcTextSize(rowText.c_str()).x + 16.0f);
				const float buttonX = rowWidth > buttonWidth ? std::max(0.0f, std::min(wantedX, rowWidth - buttonWidth - 8.0f)) : 0.0f;
				IG::SameLine(buttonX);
				if (confirming) {
					if (IG::SmallButton("Remove")) {
						const auto* armor = rows[i].second;
						const std::string outfit = a_outfit;
						g_pendingRemove = nullptr;
						g_pendingRemoveSlot = 0;
						g_pendingRemoveOutfit.clear();
						RemoveArmor(outfit, armor);
						removedThisFrame = true;
					}
					IG::SameLine();
					if (IG::SmallButton("Cancel")) {
						g_pendingRemove = nullptr;
						g_pendingRemoveSlot = 0;
						g_pendingRemoveOutfit.clear();
					}
				} else if (IG::SmallButton("Remove")) {
					g_pendingRemove = rows[i].second;
					g_pendingRemoveSlot = rows[i].first;
					g_pendingRemoveOutfit = a_outfit;
				}
				IG::PopID();
				if (removedThisFrame) {
					break;
				}
			}
		}
		IG::EndChild();
	}

	void RenderPolicies(const std::string& a_outfit, const OutfitView& a_view)
	{
		IG::SeparatorText("Slot policies");

		const std::string base = a_view.blanket;
		IG::SetNextItemWidth(520.0f);
		if (IG::BeginCombo("Base policy", PolicyLabelFor(base))) {
			for (const auto& label : kPolicyLabels) {
				const bool selected = base == label.code;
				if (IG::Selectable(PolicyLabelFor(label.code), selected)) {
					const std::string code = label.code;
					{
						auto service = outfit_service_get_mut_singleton_ptr();
						auto* outfit = service->inner().get_mut_outfit_ptr(a_outfit.c_str());
						if (outfit) {
							outfit->set_blanket_slot_policy_c(code.c_str());
						}
					}
					SetStatus(std::string(I18n::T("Base policy set to ", "基础策略已设为 ")) + PolicyLabelFor(code) + ".");
					NotifyChanged(a_outfit);
				}
			}
			IG::EndCombo();
		}
		IG::TextDisabled("Policies decide what is drawn when the outfit and the real equipment disagree.");

		bool showSlotPolicies = g_slotPolicyOpen.find(a_outfit) != g_slotPolicyOpen.end();
		if (IG::Checkbox("Per-slot policy editor", &showSlotPolicies)) {
			// Remember the choice per outfit: the menu shows one outfit at a
			// time, and switching to another outfit must not inherit it.
			if (showSlotPolicies) {
				g_slotPolicyOpen.insert(a_outfit);
			} else {
				g_slotPolicyOpen.erase(a_outfit);
			}
		}
		if (showSlotPolicies) {
			if (IG::BeginChild("##slotpolicies", ImVec2(0.0f, 240.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
				for (std::uint32_t slot = 0; slot < 32; ++slot) {
					IG::PushID(static_cast<int>(slot));
					const auto& current = a_view.slotPolicies[slot];
					const char* preview = current ? PolicyLabelFor(*current) : I18n::T("Use base policy", "使用基础策略");
					IG::SetNextItemWidth(520.0f);
					if (IG::BeginCombo(SlotLabelFor(slot).c_str(), preview)) {
						if (IG::Selectable(I18n::T("Use base policy", "使用基础策略"), !current)) {
							{
								auto service = outfit_service_get_mut_singleton_ptr();
								auto* outfit = service->inner().get_mut_outfit_ptr(a_outfit.c_str());
								if (outfit) {
									outfit->set_slot_policy_c(slot, nullptr);
								}
							}
							SetStatus(std::string(I18n::T("Slot ", "槽位 ")) + SlotLabelFor(slot) + I18n::T(" follows the base policy again.", " 已重新跟随基础策略。"));
							NotifyChanged(a_outfit);
						}
						for (const auto& label : kPolicyLabels) {
							if (IG::Selectable(PolicyLabelFor(label.code), current && *current == label.code)) {
								const std::string code = label.code;
								{
									auto service = outfit_service_get_mut_singleton_ptr();
									auto* outfit = service->inner().get_mut_outfit_ptr(a_outfit.c_str());
									if (outfit) {
										outfit->set_slot_policy_c(slot, code.c_str());
									}
								}
								SetStatus(std::string(I18n::T("Slot ", "槽位 ")) + SlotLabelFor(slot) + I18n::T(" uses ", " 使用 ") + PolicyLabelFor(code) + ".");
								NotifyChanged(a_outfit);
							}
						}
						IG::EndCombo();
					}
					IG::PopID();
				}
			}
			IG::EndChild();
		}

		if (IG::Button("Reset all policies to the defaults")) {
			{
				auto service = outfit_service_get_mut_singleton_ptr();
				auto* outfit = service->inner().get_mut_outfit_ptr(a_outfit.c_str());
				if (outfit) {
					outfit->reset_to_default_slot_policy();
				}
			}
			SetStatus(I18n::T("Slot policies reset to the defaults.", "槽位策略已重置为默认。"));
			NotifyChanged(a_outfit);
		}
	}

	} // namespace
	void Render(const std::string& a_outfit, RE::FormID a_target, void (*a_onClose)())
	{
		OutfitView view;
		if (!SnapshotOutfit(a_outfit, view)) {
			IG::TextDisabled("That outfit no longer exists.");
			return;
		}

		IG::Text("Editing: %s", view.name.c_str());
		IG::SameLine();
		IG::TextDisabled("%zu pieces", view.armors.size());

		if (IG::Button("Back to the list")) {
			if (a_onClose) {
				a_onClose();
			}
			return;
		}
		auto* target = a_target != 0 ? RE::TESForm::LookupByID<RE::Actor>(a_target) : RE::PlayerCharacter::GetSingleton();
		const char* targetName = target ? target->GetDisplayFullName() : nullptr;
		if (!targetName) {
			targetName = I18n::T("<not loaded>", "<未加载>");
		}
		IG::SameLine();
		IG::TextDisabled("Target: %s", targetName);
		if (IG::Button("Wear")) {
			if (target) {
				Features::AssignOutfit(target, a_outfit.c_str());
				SetStatus(std::string(I18n::T("Outfit applied to ", "已为以下对象穿上幻化：")) + targetName + ".");
			}
		}
		IG::SameLine();
		if (IG::Button("Take off")) {
			if (target) {
				Features::AssignOutfit(target, "");
				SetStatus(std::string(I18n::T("Outfit removed from ", "已为以下对象取消幻化：")) + targetName + ".");
			}
		}
		IG::SameLine();
		bool favorite = view.favorite;
		if (IG::Checkbox("Favourite", &favorite)) {
			{
				auto service = outfit_service_get_mut_singleton_ptr();
				service->inner().set_favorite(a_outfit.c_str(), favorite);
			}
			Menu::InvalidateBrowser();
		}

		if (!g_status.empty()) {
			IG::TextDisabled("%s", g_status.c_str());
		}

		RenderPendingConflict();
		RenderAddSection(a_outfit);
		RenderSlotList(a_outfit, view.armors);
		RenderPolicies(a_outfit, view);
	}

	void Invalidate()
	{
		g_searchSignature.clear();
		g_results.clear();
		g_pending = nullptr;
		g_pendingRemove = nullptr;
		g_pendingRemoveSlot = 0;
		g_pendingRemoveOutfit.clear();
		g_status.clear();
	}
}
