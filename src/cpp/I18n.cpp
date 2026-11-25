#include "I18n.h"

#include "core/Settings.h"

#include <SimpleIni.h>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SOS::I18n
{
	namespace
	{
		struct Entry
		{
			const char* en;
			const char* zh;
		};

		// Every literal the UI can show has an entry here; the English side is
		// also the key used by the translation files. Strings that are missing
		// are shown verbatim, which is what dynamic content (outfit names,
		// actor names, versions) needs.
		constexpr Entry kDictionary[]{
			{ "Skyrim Outfit System", "天际幻化系统" },
			{ "General", "常规" },
			{ "Mod compatibility", "Mod 兼容" },
			{ "Hotkeys", "热键" },
			{ "Random outfit", "随机幻化" },
			{ "Diagnostics", "诊断" },
			{ "Language", "语言" },
			{ "Auto (game language)", "自动（跟随游戏）" },
			{ "English", "English" },
			{ "中文", "中文" },
			{ "Change outfit", "更换幻化" },
			{ "Wear", "穿上" },
			{ "Delete", "删除" },
			{ "Rename", "重命名" },
			{ "Create from worn", "用当前装备新建" },
			{ "Create empty", "新建空幻化" },
			{ "Copy", "复制" },
			{ "Paste", "粘贴" },
			{ "New outfit name...", "新幻化名称..." },
			{ "Favourites", "收藏" },
			{ "Favourites only", "仅收藏" },
			{ "Search...", "搜索..." },
			{ "Search by name...", "按名称搜索..." },
			{ "per page", "每页" },
			{ "page %d / %d", "第 %d / %d 页" },
			{ "Outfits: %zu", "幻化数：%zu" },
			{ "%zu outfits", "共 %zu 套" },
			{ "%zu armour pieces\nRight-click buttons below apply to other actors.", "共 %zu 件护甲\n下面的右键按钮作用于其他角色。" },
			{ "(double-click a name to apply it to the player)", "（双击名称即可应用到玩家）" },
			{ "No outfits yet - wear some armour and use 'Create from worn' below.", "还没有幻化 - 先穿上护甲再用下面的“用当前装备新建”。" },
			{ "That name is already taken.", "该名称已被占用。" },
			{ "Select an outfit on the left first.", "请先在左侧选中一套幻化。" },
			{ "Outfit copied to the clipboard.", "幻化已复制到剪贴板。" },
			{ "Could not copy the outfit to the clipboard.", "无法把幻化复制到剪贴板。" },
			{ "Pasted outfit '%s'.", "已粘贴幻化「%s」。" },
			{ "The clipboard does not contain any text.", "剪贴板里没有文本。" },
			{ "Another program is holding the clipboard; try again.", "其他程序正在占用剪贴板，请重试。" },
			{ "The clipboard does not contain valid JSON.", "剪贴板内容不是有效的 JSON。" },
			{ "The clipboard does not contain an outfit.", "剪贴板里不是幻化数据。" },
			{ "Could not create the pasted outfit.", "无法创建粘贴的幻化。" },
			{ "Outfit not found.", "找不到该幻化。" },
			{ "Outfit: ", "幻化：" },
			{ "Random", "随机" },
			{ "Close", "关闭" },
			{ "Actors", "角色" },
			{ "Add player", "添加玩家" },
			{ "Refresh all appearances", "刷新全部外观" },
			{ "No actors configured yet.", "还没有配置角色。" },
			{ "Outfit", "幻化" },
			{ "Location/state based switching", "按地点/状态自动切换" },
			{ "State outfits", "状态幻化" },
			{ "Refresh", "刷新" },
			{ "Remove", "移除" },
			{ "<inherit>", "<继承>" },
			{ "<random>", "<随机>" },
			{ "Delete outfit \"%s\"? This also removes it from every actor and state assignment.", "删除幻化“%s”？这也会从所有角色和状态指派中移除它。" },
			{ "<none>", "<无>" },
			{ "<not loaded>", "<未加载>" },
			{ "Enable outfit overrides", "启用幻化" },
			{ "Settings are stored in Data\\SKSE\\Plugins\\SkyrimOutfitSystemNG.ini", "设置保存在 Data\\SKSE\\Plugins\\SkyrimOutfitSystemNG.ini" },
			{ "Advanced Nudity Detection sees the outfit", "让 Advanced Nudity Detection 看到幻化" },
			{ "Advanced Nudity Detection: not installed.", "未安装 Advanced Nudity Detection。" },
			{ "Advanced Nudity Detection %s: nudity, topless and bottomless follow the outfit.", "Advanced Nudity Detection %s：裸体／上半身／下半身判定跟随幻化。" },
			{ "Advanced Nudity Detection %s: its nudity factions were not found, detection keeps the vanilla behaviour.", "Advanced Nudity Detection %s：找不到其裸体阵营，检测仍按原版。" },
			{ "An empty outfit counts as bare skin.", "空幻化视为裸体。" },
			{ "Procedural Armor Damage tears the outfit", "让 Procedural Armor Damage 撕损幻化" },
			{ "Procedural Armor Damage: not installed.", "未安装 Procedural Armor Damage。" },
			{ "Procedural Armor Damage %s: damage is stored on the real armour worn in the same slot.", "Procedural Armor Damage %s：损伤记录在同一槽位真实穿着的护甲上。" },
			{ "Pieces shown over a slot with no real armour underneath cannot be damaged.", "槽位上没有真实护甲打底的幻化件无法被撕损。" },
			{ "Log slot report", "记录槽位报告" },
			{ "Lists every slot: what the outfit shows, what is really worn.", "列出每个槽位：幻化显示什么、真实穿着什么。" },
			{ "Reset SMP physics when the outfit changes", "切换幻化时重置 SMP 物理效果" },
			{ "Only the actor whose outfit changed is reset.", "只重置换装的那个角色。" },
			{ "Faster HDT-SMP (DynamicHDT): not detected; this switch has no effect.", "Faster HDT-SMP（DynamicHDT）：未检测到；该开关不会生效。" },
			{ "Faster HDT-SMP (DynamicHDT): detected.", "Faster HDT-SMP（DynamicHDT）：已检测到。" },
			{ "Wear <current outfit>_sex during SexLab / OStim scenes", "进入 SexLab / OStim 场景时自动换用“<当前幻化>_sex”" },
			{ "If the current outfit already ends with _sex it is kept unchanged.", "当前幻化名称已以 _sex 结尾时保持不变。" },
			{ "The previous outfit is restored when the scene ends.", "场景结束后恢复之前的幻化。" },
			{ "Chinese needs [Fonts] EnableChinese = true and a CJK font (CN.ttf) in Data\\SKSE\\Plugins\\Fonts of the SKSE Menu Framework.", "中文需要 [Fonts] EnableChinese = true，并在 SKSE Menu Framework 的 Data\\SKSE\\Plugins\\Fonts 里放中文字体（CN.ttf）。" },
			{ "random outfit - example: ctrl+shift+O, alt+F5", "随机幻化 - 例如 ctrl+shift+O、alt+F5" },
			{ "quick picker window", "快速选择窗口" },
			{ "Never pick the outfit that is already worn", "不重复选择当前幻化" },
			{ "The \"Change Outfit\" power rolls a random outfit", "“更换幻化”能力随机抽取一套幻化" },
			{ "Off (default) opens the quick picker window instead.", "关闭（默认）时改为打开快速选择窗口。" },
			{ "Roll now", "立即随机" },
			{ "No outfits available.", "没有可用幻化。" },
			{ "Verbose logging (takes effect on the next start)", "详细日志（下次启动生效）" },
			{ "ImGui host: %s", "ImGui 宿主：%s" },
			{ "Quick picker", "快速选择" },
			{ "Skyrim Outfit System - Quick picker", "天际幻化系统 - 快速选择" },
			{ "No outfits. Create some in the Skyrim Outfit System menu (F1).", "没有幻化。请在 Skyrim Outfit System 菜单（F1）中创建。" },
			{ "Skyrim Outfit System: that outfit no longer exists.", "Skyrim Outfit System：该幻化已不存在。" },
			{ "World", "世界" },
			{ "World (snowy)", "世界（雪）" },
			{ "World (rainy)", "世界（雨）" },
			{ "Town", "城镇" },
			{ "Town (snowy)", "城镇（雪）" },
			{ "Town (rainy)", "城镇（雨）" },
			{ "City", "城市" },
			{ "City (snowy)", "城市（雪）" },
			{ "City (rainy)", "城市（雨）" },
			{ "Dungeon", "地下城" },
			{ "Dungeon (snowy)", "地下城（雪）" },
			{ "Dungeon (rainy)", "地下城（雨）" },
			{ "Combat", "战斗" },
			{ "Select an outfit on the left to edit its armour pieces and slot policies.", "在左侧选择一个幻化，编辑它的护甲件与槽位策略。" },
			{ "Add a nearby actor...", "添加附近角色..." },
			{ "No other actor in this cell.", "这个场景里没有其他角色。" },
			{ "Give the player the \"Change Outfit\" power", "给予玩家“更换幻化”能力" },
			{ "Editing: %s", "正在编辑：%s" },
			{ "%zu pieces", "共 %zu 件" },
			{ "Max %zu bytes per name.", "每个名称最多 %zu 字节。" },
			{ "Take off", "脱下" },
			{ "Stop managing %s? The player cannot be removed.", "不再管理 %s？玩家不能被移除。" },
			{ "Stop managing that actor?", "不再管理该角色？" },
			{ "Remove \"%s\" from the outfit?", "从幻化中移除“%s”？" },
			{ "Apply to", "应用到" },
			{ "Version %s", "版本 %s" },
			{ "Player", "玩家" },
			{ "Target: %s", "目标：%s" },
			{ "(double-click a name to apply it to %s)", "（双击名称即可应用到 %s）" },
			{ "Delete the outfit \"%s\"? This cannot be undone.", "删除幻化“%s”？此操作无法撤销。" },
			{ "Back to the list", "返回列表" },
			{ "Wear on the player", "穿到玩家身上" },
			{ "Favourite", "收藏" },
			{ "Add armour", "添加护甲" },
			{ "From the inventory", "从背包" },
			{ "From the worn armour", "从当前穿戴" },
			{ "By form ID", "按 FormID" },
			{ "From the full armour list", "从全部护甲列表" },
			{ "Filter by armour name or plugin...", "按护甲名或插件名过滤..." },
			{ "Playable only", "仅可玩" },
			{ "Add", "添加" },
			{ "%zu pieces match.", "匹配 %zu 件。" },
			{ "Showing the first %zu of %zu pieces - refine the filter.", "只显示前 %zu / 共 %zu 件 - 请缩小过滤范围。" },
			{ "No form with that ID is loaded.", "没有加载该 ID 的 Form。" },
			{ "That form is not armour.", "该 Form 不是护甲。" },
			{ "Hexadecimal, for example 00012EB7.", "十六进制，例如 00012EB7。" },
			{ "Enter a form ID such as 00012EB7.", "请输入 FormID，例如 00012EB7。" },
			{ "Body slots", "身体槽位" },
			{ "Slot policies", "槽位策略" },
			{ "Base policy", "基础策略" },
			{ "Use base policy", "使用基础策略" },
			{ "Unknown policy", "未知策略" },
			{ "Per-slot policy editor", "逐槽策略编辑器" },
			{ "Reset all policies to the defaults", "全部策略恢复默认" },
			{ "Slot policies reset to the defaults.", "槽位策略已重置为默认。" },
			{ "Policies decide what is drawn when the outfit and the real equipment disagree.", "策略决定幻化与真实装备冲突时显示什么。" },
			{ "%s uses a body slot that is already covered by this outfit. Replace the conflicting pieces?", "%s 的身体槽位已被本幻化占用。要替换冲突的护甲件吗？" },
			{ "Replace the conflicting pieces", "替换冲突的护甲件" },
			{ "Cancel", "取消" },
			{ "This outfit is empty.", "该幻化是空的。" },
			{ "The player carries no armour.", "玩家背包里没有护甲。" },
			{ "The player wears no armour.", "玩家没有穿护甲。" },
			{ "That outfit no longer exists.", "该幻化已不存在。" },
			{ "Plugin enabled: %s | Random hotkey: %s", "插件启用：%s | 随机热键：%s" },
			{ "%zu armour pieces\nWear applies it to the actor chosen in \"Apply to\".", "%zu 件护甲\n“穿上”会应用到“应用到”里选中的角色。" },
			{ "Outfits", "幻化" },
			{ "Settings", "设置" },
			{ "List width", "列表宽度" },
			{ "Page size", "每页数量" },
			{ "Remove outfit", "取消幻化" },
			{ "Removes the outfit from the player and shows the real armour again (X).", "移除玩家身上的幻化，恢复显示真实护甲（X）。" },
			{ "Translation file: %s (%zu lines)", "翻译文件：%s（%zu 行）" },
			{ "Reload the language file", "重新加载语言文件" },
			{ "Menu titles update after a game restart (the framework caches them).", "菜单标题要重启游戏才会更新（框架会缓存标题）。" },
			{ "Apply", "应用" },
			{ "No outfit matches the filter.", "没有符合过滤条件的幻化。" },
			{ "Outfit page size", "幻化列表每页数量" },
			{ "Quick picker page size", "快速选择每页数量" },
			{ "W/S: select   A/D: page   E: wear   R: random   X: remove   Esc: close", "W/S：选择   A/D：翻页   E：穿戴   R：随机   X：取消幻化   Esc：关闭" },
			{ "List width and page sizes are stored in the ini and shared by the quick picker.", "列表宽度与每页数量保存在 ini 中，快速选择窗口共用。" },
			{ "Translation file reloaded.", "语言文件已重新加载。" },
			{ "Outfit removed.", "已取消幻化。" },
			{ "Data\\SKSE\\Plugins\\OutfitSystemData.json - the same file the original mod reads and writes.", "Data\\SKSE\\Plugins\\OutfitSystemData.json —— 与原版读取和写入的是同一个文件。" },
			{ "Export outfits to OutfitSystemData.json", "导出幻化到 OutfitSystemData.json" },
			{ "Import outfits from OutfitSystemData.json", "从 OutfitSystemData.json 导入幻化" },
			{ "Outfit data", "幻化数据" },
			{ "Failed to open config for reading", "无法打开配置文件读取。" },
			{ "Failed to open config for writing", "无法打开配置文件写入。" },
			{ "Failed to read config data", "读取配置文件内容失败。" },
			{ "Failed to write config", "写入配置文件失败。" },
			{ "Failed to parse config data. Invalid syntax.", "无法解析配置数据，语法错误。" },
			{ "Wrote JSON config to ", "已导出 JSON 配置到 " },
			{ "Read JSON config from ", "已从以下文件导入 JSON 配置：" },
			{ "Interface", "界面" },
			{ "No outfits available to pick from.", "没有可供抽取的幻化。" },
			{ "Skyrim Outfit System: install SKSE Menu Framework to use the quick outfit picker.", "天际幻化系统：需要安装 SKSE Menu Framework 才能使用快速选择窗口。" },
			{ "Slot report written to OutfitSystemNG.log", "槽位报告已写入 OutfitSystemNG.log" },
			// 1.5.65: hotkey recording window
			{ "Not set", "未设置" },
			{ "Clear", "清除" },
			{ "Change Outfit power key", "切换服装能力键" },
			{ "click to record, e.g. ctrl+shift+O", "点击后按下按键录制，例如 ctrl+shift+O" },
			{ "click to record, e.g. ctrl+shift+L", "点击后按下按键录制，例如 ctrl+shift+L" },
			{ "the hotkey of the power itself; this one skips its cast animation", "这个能力本身的热键；按下它就不必等施法动作" },
			{ "Set a hotkey", "设置热键" },
			{ "Press the keys you want to use: up to four held together, keyboard or controller.", "请按下要用的按键：最多四个同时按住，键盘或手柄都可以。" },
			{ "Waiting for input...", "等待输入…" },
			{ "Only the first four keys are used.", "只取前四个按键。" },
			{ "Release every key when you are done; the chord is stored in the ini right away.", "录完后把所有键都松开；组合键会立刻写入 ini。" },
			{ "That outfit name is too long.", "幻化名称过长。" },
			{ "[Unnamed]", "[未命名]" },
			{ "Added ", "已添加 " },
			{ "Removed ", "已移除 " },
			{ "Replaced the conflicting pieces with ", "已用以下护甲替换冲突件：" },
			{ "Base policy set to ", "基础策略已设为 " },
			{ "Slot ", "槽位 " },
			{ " follows the base policy again.", " 已重新跟随基础策略。" },
			{ " uses ", " 使用 " },
			{ "Outfit applied to ", "已为以下对象穿上幻化：" },
			{ "Outfit removed from ", "已为以下对象取消幻化：" },
			{ "yes", "是" },
			{ "no", "否" },
			{ "Never show anything", "从不显示任何东西" },
			{ "If outfit and equipped, show equipped", "幻化和装备都有时显示装备" },
			{ "Require equipped (show the outfit only when the slot is worn)", "需要已装备（槽位有装备时才显示幻化）" },
			{ "If only outfit, show outfit", "只有幻化时显示幻化" },
			{ "If only outfit show outfit; if both show equipped", "只有幻化显示幻化；都有则显示装备" },
			{ "Always use outfit", "始终使用幻化" },
			{ "If only equipped, show equipped", "只有装备时显示装备" },
			{ "If equipped, show equipped", "有装备就显示装备" },
			{ "Passthrough", "穿透" },
			{ "If only equipped show equipped; if only outfit show outfit", "只有装备显示装备；只有幻化显示幻化" },
			{ "Both, but show equipped when both exist", "两者都显示，但都有时显示装备" },
			{ "Both, and show the outfit when both exist", "两者都显示，但都有时显示幻化" },
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
			{ "The menu framework does not expose the window functions; the quick picker cannot open.", "菜单框架没有提供窗口函数；快速选择窗口无法打开。" },
			{ "Reload the language files", "重新加载语言文件" },
			{ "Translation file: %s", "翻译文件：%s" },
			{ "Data\\interface\\translations\\SkyrimOutfitSystemNG_<language>.txt - one file per language.", "Data\\interface\\translations\\SkyrimOutfitSystemNG_<语言>.txt —— 每种语言一个文件。" },
			{ "Golden items", "金件" },
			{ "Generate outfits from a plugin", "从 ESP 生成套装" },
			{ "Every wearable piece of a plugin becomes an outfit. Pieces whose names start the same way count as one costume, and pieces that share a slot inside a costume are split into <name>_1, <name>_2 ... Every slot policy is set to \"require equipped\".", "把该 ESP 里每件可穿的装备变成幻化；名称开头相同的算同一套服装，同一套里同槽位有多件时拆成 <名称>_1、<名称>_2 等；槽位策略统一设为“需要已装备（槽位有装备时才显示幻化）”。" },
			{ "Choose a plugin...", "选择 ESP..." },
			{ "Mark generated outfits as favourites", "生成的套装自动收藏" },
			{ "Search plugins...", "搜索 ESP..." },
			{ "No plugin matches that search.", "没有匹配的 ESP。" },
			{ "Golden items: %zu piece(s) in %zu plugin group(s)", "金件：共 %zu 件，分布在 %zu 个 ESP 组" },
			{ "A golden item is drawn on top of the outfit. Its file is separate from the outfit data.", "金件会画在幻化之上。它的存档文件与幻化数据分开。" },
			{ "Editing", "编辑" },
			{ "Add every piece of a plugin...", "添加某个 ESP 的全部装备..." },
			{ "Add one piece from a plugin...", "按 ESP 添加单件..." },
			{ "Export to OutfitSystemGoldenItems.json", "导出到 OutfitSystemGoldenItems.json" },
			{ "Import from OutfitSystemGoldenItems.json", "从 OutfitSystemGoldenItems.json 导入" },
			{ "Add every slot armour of the chosen plugin.", "添加所选 ESP 里全部幻化槽位护甲。" },
			{ "Choose the plugin that holds the piece; its pieces are listed below.", "选择这件装备所属的 ESP，它的全部装备会列在下面。" },
			{ "Enter the local form id of the piece in the editor on the right.", "在右侧编辑器里输入该单件的本地 FormID。" },
			{ "No golden items yet - add a plugin above, or pick a single piece out of a plugin.", "还没有金件 —— 先在上面添加一个 ESP，或从某个 ESP 里挑一件。" },
			{ "Select a plugin group on the left to edit its pieces.", "在左侧选中一个 ESP 组以编辑其中的金件。" },
			{ "%zu golden piece(s); the plugin holds %zu slot armour record(s).", "金件 %zu 件；该 ESP 共有 %zu 条幻化槽位护甲。" },
			{ "Remove every piece of this plugin", "移除该 ESP 的全部金件" },
			{ "Pick a piece of this plugin below; the list holds only pieces of the selected plugin.", "在下面点一件该 ESP 的装备；列表只含选中 ESP 的装备。" },
			{ "Add piece", "添加单件" },
			{ "That is not a hexadecimal form id.", "这不是十六进制的 FormID。" },
			{ "This group only takes pieces of %s.", "该组只接受 %s 里的金件。" },
			{ "[slot %s]", "[槽位 %s]" },

			{ "Pick a piece of that plugin below.", "在下面点一件该 ESP 的装备。" },
			{ "Close the piece list", "关闭装备列表" },
			{ "Search pieces...", "搜索装备..." },
			{ "%zu slot armour record(s) in this plugin; click a piece to add it.", "该 ESP 共有 %zu 条槽位护甲记录；点一件即可加入。" },
			{ "No piece of this plugin matches that search.", "该 ESP 里没有匹配的装备。" },
			{ "[in the list] %s  %s", "[已加入] %s  %s" },
			// 1.5.56: bulk delete, adding a single piece from a plugin, golden items
			// and the ESP preview reasons were missing from the dictionary, so those
			// screens stayed English in a Chinese interface.
			{ "From a plugin (ESP)", "从 ESP 添加单个部件" },
			{ "Choose a plugin, then click one of its pieces. One piece per body slot: a piece that collides with a slot this outfit already covers is marked and can only be added by replacing the other piece.", "先选一个 ESP，再点它的某件装备。一个槽位只能放一件：与这套幻化已有槽位冲突的装备会带标记，只能用替换的方式加入。" },
			{ "(no plugin chosen)", "（未选择 ESP）" },
			{ "%s  (%zu pieces, %zu wearable)", "%s （共 %zu 件，%zu 件可穿）" },
			{ "Only free slots", "只显示槽位空闲的" },
			{ "Showing the first %zu of %zu pieces - refine the search or tick 「only free slots」.", "只显示前 %zu 件（共 %zu 件），请细化搜索或勾选「只显示槽位空闲的」。" },
			{ "[slot taken] ", "[槽位已占用] " },
			{ "Bulk delete", "批量删除" },
			{ "%zu preset(s) ticked", "已勾选 %zu 套幻化" },
			{ "Tick this page", "本页全选" },
			{ "Untick this page", "本页取消" },
			{ "Tick every match", "全部筛选结果" },
			{ "Untick all", "清空勾选" },
			{ "Tick presets in the list, then confirm the delete.", "先在列表里勾选幻化，再确认删除。" },
			{ "Delete the ticked presets", "删除已勾选的幻化" },
			{ "Delete %zu preset(s)? This also removes them from every actor and state assignment.", "删除 %zu 套幻化？这也会把它们从所有角色与状态指派里移除。" },
			{ "Yes, delete them", "确认删除" },
			{ "Deleted %zu preset(s).", "已删除 %zu 套幻化。" },
			{ "Select a plugin to preview its outfits. Uncertain matches are not created.", "选一个 ESP 预览它能生成的幻化。判断不出来的组合不会生成。" },
			{ "Only ordinary clothing is taken: FX-only records, records with no readable name, and records with no model are left out.", "只收普通服装：只有 FX 槽、名称读不出来、没有模型的记录都会被排除。" },
			{ "%s  (%zu piece(s), %zu skipped as not clothing)", "%s （%zu 件，%zu 件非服装已跳过）" },
			{ "%s  (%zu piece(s))", "%s （%zu 件）" },
			{ "%s: %zu proposed, %zu pieces pending, %zu non-clothing records filtered.", "%s：可生成 %zu 套，%zu 件待判断，过滤掉 %zu 条非服装记录。" },
			{ "The preset limit was reached; some groups were left pending.", "已达幻化数量上限，有款式只剩部分收录。" },
			{ "no shared style name", "没有同款式的名称前缀" },
			{ "shares a slot with another piece", "与其他件占用同一槽位" },
			{ "several pieces on one slot look the same", "同槽多件无法区分" },
			{ "the variant marks do not line up", "同款各槽的差目标号对不上" },
			{ "every piece covers the same slot", "所有件都占同一槽位" },
			{ "not enough pieces for a full outfit", "凑不成成套件" },
			{ "this style can never fill its slots", "这套的槽位凑不齐" },
			{ "another option of a slot already filled", "同一槽位已有别的件，这件未采用" },
			{ "too many variants on one slot", "同一槽位的变体过多" },
			{ "preset limit reached", "已达幻化数量上限" },
			{ "a preset with that name already exists", "已有同名幻化" },
			{ "the name cannot be used for a preset", "名称不能用作幻化名" },
			{ "not enough evidence", "证据不足" },
			{ "%s (slots %s) [%08X]", "%s（槽位 %s）[%08X]" },
			{ "in '%s' (%s)", "在「%s」（%s）中" },
			{ " (already exists; will skip)", "（已存在，将跳过）" },
			{ "Pending pieces (will not be created)", "待判断的件（不会生成）" },
			{ "Create previewed outfits", "生成预览的幻化" },
			{ "Created %zu outfits; skipped %zu. Existing presets were not replaced.", "已生成 %zu 套幻化，跳过 %zu 套；已存在的幻化没有被覆盖。" },
			{ "Close outfit preview", "关闭幻化预览" },
			{ "Outfit generation", "幻化生成" },
			{ "Mark outfits generated from a plugin as favourites", "生成的幻化自动收藏" },
			{ "Generated outfits take ordinary clothing only: FX-only records, records with no readable name, and records with no model are skipped. Golden items are not filtered.", "生成的幻化只收普通服装：只有 FX 槽、名称读不出来、没有模型的记录会被跳过；金件不做过滤。" },
			{ "Added %s.", "已加入 %s。" },
			{ "Golden items take any record: FX effects, unnamed records and records without a model are allowed here.", "金件接受任何记录：FX 效果、没有名称的记录、没有模型的记录都可以加进来。" },
			{ "[not saved] The changes live in memory only; press Export to write OutfitSystemGoldenItems.json.", "[未保存] 改动只在内存里；按「导出」才会写入 OutfitSystemGoldenItems.json。" },
			{ "Changes reach the file only when you press Export.", "只有按「导出」才会写进文件。" },
			{ "Exported %zu piece(s) to %s", "已导出 %zu 件到 %s" },
			{ "The golden item file could not be written; see the log.", "金件文件写入失败，详情见日志。" },
			{ "Imported %zu piece(s) from %s", "已导入 %zu 件（来自 %s）" },
			{ "The golden item file could not be read; see the log.", "金件文件读取失败，详情见日志。" },
			{ "%zu piece(s) of %s added.", "已加入 %zu 件（来自 %s）。" },
			{ "Removed the %s group.", "已移除 %s 分组。" },
			{ "<not in this load order>", "<不在当前加载顺序里>" },
			{ "Removed %s:%s.", "已移除 %s:%s。" },
		};

		constexpr std::string_view kFilePrefix = "SkyrimOutfitSystemNG_";
		constexpr const char*      kFileExtension = ".txt";

		using Strings = std::unordered_map<std::string, std::string>;

		struct State
		{
			std::string              mode{ "auto" };
			std::string              code{ "english" };
			bool                     chinese{ false };
			bool                     available{ false };
			std::string              status{ "english (default)" };
			std::string              fileStatus{ "not read yet" };
			std::size_t              overrides{ 0 };
			std::vector<std::string> languages{ std::string("english") };
			Strings                  strings;
		};

		State& Get()
		{
			static State s_state;
			return s_state;
		}

		// code -> every line of that language file
		std::unordered_map<std::string, Strings>& Files()
		{
			static std::unordered_map<std::string, Strings> s_files;
			return s_files;
		}

		std::filesystem::path RuntimeDir()
		{
			std::error_code ec;
			const auto      path = std::filesystem::current_path(ec);
			if (ec) {
				return {};
			}
			return path;
		}

		// DataSKSEPlugins of the running installation, i.e. where the framework,
		// the fonts and our ini live.
		std::filesystem::path PluginsDir()
		{
			const auto root = RuntimeDir();
			if (root.empty()) {
				return {};
			}
			return root / "Data" / "SKSE" / "Plugins";
		}

		// Datainterface	ranslations - next to the game's own translation files.
		std::filesystem::path TranslationsDir()
		{
			const auto root = RuntimeDir();
			if (root.empty()) {
				return {};
			}
			return root / "Data" / "interface" / "translations";
		}

		std::string ToLower(std::string_view a_text)
		{
			std::string out(a_text);
			std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return out;
		}

		bool IsSpace(char a_char)
		{
			return a_char == ' ' || a_char == '\t' || a_char == '\r' || a_char == '\n';
		}

		std::string_view TrimView(std::string_view a_text)
		{
			while (!a_text.empty() && IsSpace(a_text.front())) {
				a_text.remove_prefix(1);
			}
			while (!a_text.empty() && IsSpace(a_text.back())) {
				a_text.remove_suffix(1);
			}
			return a_text;
		}
		// Keys may end with a space on purpose ("Outfit: " is followed by a name),
		// so a key is only trimmed on the left.
		std::string_view TrimLeft(std::string_view a_text)
		{
			while (!a_text.empty() && IsSpace(a_text.front())) {
				a_text.remove_prefix(1);
			}
			return a_text;
		}

		// "\n" -> newline, "\t" -> tab, "\\" -> backslash. Everything else is
		// copied verbatim so translations may contain any punctuation.
		std::string Unescape(std::string_view a_text)
		{
			std::string out;
			out.reserve(a_text.size());
			for (std::size_t i = 0; i < a_text.size(); ++i) {
				if (a_text[i] == '\\' && i + 1 < a_text.size()) {
					switch (a_text[i + 1]) {
					case 'n':
						out.push_back('\n');
						++i;
						continue;
					case 't':
						out.push_back('\t');
						++i;
						continue;
					case '\\':
						out.push_back('\\');
						++i;
						continue;
					default:
						break;
					}
				}
				out.push_back(a_text[i]);
			}
			return out;
		}

		std::string Escape(std::string_view a_text)
		{
			std::string out;
			out.reserve(a_text.size() + 8);
			for (const char c : a_text) {
				switch (c) {
				case '\n':
					out += "\\n";
					break;
				case '\t':
					out += "\\t";
					break;
				case '\\':
					out += "\\\\";
					break;
				case '\r':
					break;
				default:
					out.push_back(c);
					break;
				}
			}
			return out;
		}

		std::string Utf16ToUtf8(const char* a_data, std::size_t a_size, bool a_bigEndian)
		{
			std::wstring wide;
			wide.reserve(a_size / 2);
			for (std::size_t i = 0; i + 1 < a_size; i += 2) {
				const auto lo = static_cast<unsigned char>(a_data[i]);
				const auto hi = static_cast<unsigned char>(a_data[i + 1]);
				const auto unit = a_bigEndian ? static_cast<wchar_t>((lo << 8) | hi) : static_cast<wchar_t>((hi << 8) | lo);
				wide.push_back(static_cast<wchar_t>(unit));
			}
			if (wide.empty()) {
				return {};
			}
			const int needed = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
			if (needed <= 0) {
				return {};
			}
			std::string out(static_cast<std::size_t>(needed), '\0');
			::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), needed, nullptr, nullptr);
			return out;
		}

		bool WriteUtf16(const std::filesystem::path& a_path, std::string_view a_utf8)
		{
			if (a_utf8.empty()) {
				return false;
			}
			const int needed = ::MultiByteToWideChar(CP_UTF8, 0, a_utf8.data(), static_cast<int>(a_utf8.size()), nullptr, 0);
			if (needed <= 0) {
				return false;
			}
			std::wstring wide(static_cast<std::size_t>(needed), L'\0');
			::MultiByteToWideChar(CP_UTF8, 0, a_utf8.data(), static_cast<int>(a_utf8.size()), wide.data(), needed);

			std::ofstream out(a_path, std::ios::binary | std::ios::trunc);
			if (!out) {
				return false;
			}
			const unsigned char bom[2]{ 0xFF, 0xFE };
			out.write(reinterpret_cast<const char*>(bom), 2);
			for (const wchar_t unit : wide) {
				const unsigned char bytes[2]{ static_cast<unsigned char>(unit & 0xFF), static_cast<unsigned char>((unit >> 8) & 0xFF) };
				out.write(reinterpret_cast<const char*>(bytes), 2);
			}
			return out.good();
		}

		bool ReadWholeFile(const std::filesystem::path& a_path, std::string& a_out)
		{
			std::ifstream in(a_path, std::ios::binary);
			if (!in) {
				return false;
			}
			a_out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
			return true;
		}

		// One file per language, Skyrim style:
		//     $English literal<TAB>translation
		// A UTF-8 file is accepted as well (handy for quick edits), and plain
		// "key=value" lines are understood for convenience.
		void ParseTranslation(std::string_view a_text, Strings& a_out, std::size_t& a_count)
		{
			std::size_t pos = 0;
			if (a_text.size() >= 3 && static_cast<unsigned char>(a_text[0]) == 0xEF &&
				static_cast<unsigned char>(a_text[1]) == 0xBB && static_cast<unsigned char>(a_text[2]) == 0xBF) {
				pos = 3;
			}

			while (pos <= a_text.size()) {
				const auto end = a_text.find('\n', pos);
				auto       line = a_text.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
				pos = (end == std::string_view::npos) ? a_text.size() + 1 : end + 1;

				// Only the line ending is stripped: a key may end with a space
				// ("Outfit: "), and trimming it away made such a key impossible to
				// override from the file.
				while (!line.empty() && line.back() == '\r') {
					line.remove_suffix(1);
				}
				line = TrimLeft(line);
				if (line.empty() || line.front() == '#' || line.front() == ';') {
					continue;
				}
				std::string_view key;
				std::string_view value;
				if (line.front() == '$') {
					line.remove_prefix(1);
					// A tab is the canonical separator and both columns are taken
					// verbatim, because Escape() writes them verbatim and a few keys and
					// translations really do end with a space. '=' stays a fallback for
					// hand written lines, where trimming around the sign is convenient.
					const auto tab = line.find('\t');
					if (tab != std::string_view::npos) {
						key = line.substr(0, tab);
						value = line.substr(tab + 1);
					} else {
						const auto eq = line.find('=');
						if (eq == std::string_view::npos) {
							continue;
						}
						key = TrimView(line.substr(0, eq));
						value = TrimView(line.substr(eq + 1));
					}
				} else {
					const auto sep = line.find('=');
					if (sep == std::string_view::npos) {
						continue;
					}
					key = TrimView(line.substr(0, sep));
					value = TrimView(line.substr(sep + 1));
				}
				if (key.empty()) {
					continue;
				}
				a_out[Unescape(key)] = Unescape(value);
				++a_count;
			}
		}

		std::filesystem::path PathFor(const std::string& a_code)
		{
			const auto dir = TranslationsDir();
			if (dir.empty()) {
				return {};
			}
			return dir / (std::string(kFilePrefix) + a_code + kFileExtension);
		}

		// Writes a complete template so a translator starts from a full list
		// instead of a blank page.
		bool WriteLanguageFile(const std::filesystem::path& a_path, const char* a_title, bool a_chinese)
		{
			std::string text;
			text += "# Skyrim Outfit System NG - ";
			text += a_title;
			text += "\r\n";
			text += "#\r\n";
			text += "# One file per language: Data/interface/translations/SkyrimOutfitSystemNG_<language>.txt\r\n";
			text += "# Format: $English literal<TAB>translation, the same style as the game's own files.\r\n";
			text += "# The English literal is the key and also the fallback, so deleting a line is safe.\r\n";
			text += "# Use \\n for a line break. Lines starting with '#' or ';' are comments.\r\n";
			text += "#\r\n";
			text += "# Add another language by copying this file to SkyrimOutfitSystemNG_german.txt\r\n";
			text += "# (or any other suffix); it shows up in the language dropdown.\r\n";
			text += "\r\n";
			for (const auto& entry : kDictionary) {
				text += '$';
				text += Escape(entry.en);
				text += '\t';
				text += Escape(a_chinese ? entry.zh : entry.en);
				text += "\r\n";
			}
			return WriteUtf16(a_path, text);
		}

		void Discover(std::vector<std::string>& a_codes)
		{
			a_codes.clear();
			const auto dir = TranslationsDir();
			if (dir.empty()) {
				return;
			}
			std::error_code ec;
			if (!std::filesystem::exists(dir, ec) || ec) {
				return;
			}
			for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
				if (ec) {
					break;
				}
				if (!entry.is_regular_file(ec) || ec) {
					continue;
				}
				const auto name = entry.path().filename().string();
				if (name.size() <= kFilePrefix.size() + 4) {
					continue;
				}
				if (name.compare(0, kFilePrefix.size(), kFilePrefix) != 0) {
					continue;
				}
				if (entry.path().extension() != kFileExtension) {
					continue;
				}
				auto code = ToLower(std::string_view(name).substr(kFilePrefix.size(), name.size() - kFilePrefix.size() - 4));
				if (code.empty()) {
					continue;
				}
				a_codes.push_back(std::move(code));
			}
			std::sort(a_codes.begin(), a_codes.end());
			a_codes.erase(std::unique(a_codes.begin(), a_codes.end()), a_codes.end());
		}

		void LoadAll()
		{
			auto& files = Files();
			files.clear();

			auto&      state = Get();
			std::vector<std::string> codes;
			Discover(codes);

			if (codes.empty()) {
				const auto dir = TranslationsDir();
				std::error_code ec;
				if (!dir.empty()) {
					std::filesystem::create_directories(dir, ec);
				}
				const bool english = WriteLanguageFile(PathFor("english"), "English", false);
				const bool chinese = WriteLanguageFile(PathFor("chinese"), "Chinese", true);
				if (english && chinese) {
					SKSE::log::info("Language: wrote the editable translation files to {}.", dir.string());
					Discover(codes);
				} else {
					state.fileStatus = "translation files could not be written";
					SKSE::log::warn("Language: could not write the translation templates into {}.", dir.string());
				}
			}

			std::size_t total = 0;
			for (const auto& code : codes) {
				std::string raw;
				if (!ReadWholeFile(PathFor(code), raw)) {
					continue;
				}
				std::string text;
				if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE) {
					text = Utf16ToUtf8(raw.data() + 2, raw.size() - 2, false);
				} else if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFE && static_cast<unsigned char>(raw[1]) == 0xFF) {
					text = Utf16ToUtf8(raw.data() + 2, raw.size() - 2, true);
				} else {
					text = std::move(raw);
				}
				Strings     strings;
				std::size_t count = 0;
				ParseTranslation(text, strings, count);
				total += count;
				files[code] = std::move(strings);
			}

			if (codes.empty()) {
				codes.emplace_back("english");
			}
			state.languages = codes;
			SKSE::log::debug("Language: {} language file(s), {} lines in total, from {}.",
				state.languages.size(), total, TranslationsDir().string());
		}

		std::string GameLanguageCode()
		{
			auto* collection = RE::INISettingCollection::GetSingleton();
			if (!collection) {
				return "english";
			}
			const auto* setting = collection->GetSetting("sLanguage:General");
			if (!setting || setting->GetType() != RE::Setting::Type::kString || !setting->data.s) {
				return "english";
			}
			auto code = ToLower(setting->data.s);
			if (code.rfind("chin", 0) == 0) {
				return "chinese";
			}
			return code;
		}

		// Reads [Fonts] from the framework ini. Defaults mirror the framework:
		// EnableChinese = true, PrimaryFont = CN.ttf.
		bool FontConfig(bool& a_enableChinese, std::string& a_primaryFont)
		{
			a_enableChinese = true;
			a_primaryFont = "CN.ttf";

			CSimpleIniA ini;
			ini.SetUnicode();
			const auto path = PluginsDir() / "SKSEMenuFramework.ini";
			std::error_code ec;
			if (!std::filesystem::exists(path, ec) || ec) {
				return false;
			}
			if (ini.LoadFile(path.string().c_str()) < 0) {
				return false;
			}
			a_enableChinese = ini.GetBoolValue("Fonts", "EnableChinese", true);
			if (const char* font = ini.GetValue("Fonts", "PrimaryFont", "CN.ttf")) {
				a_primaryFont = font;
			}
			return true;
		}

		bool HasCjkFont()
		{
			bool        enableChinese = true;
			std::string primaryFont;
			const bool  hasIni = FontConfig(enableChinese, primaryFont);
			if (hasIni && !enableChinese) {
				return false;
			}
			if (primaryFont.empty()) {
				return false;
			}
			const auto font = PluginsDir() / "Fonts" / primaryFont;
			std::error_code ec;
			const bool exists = std::filesystem::exists(font, ec) && !ec;
			if (!exists && hasIni) {
				SKSE::log::warn("Language: the menu framework is configured for '{}' but {} was not found; "
								"the Chinese interface stays unavailable.",
					primaryFont, font.string());
			}
			return exists;
		}

		// A translated line is handed to printf as the format string, so its
		// placeholders must appear in the same order and of the same kind as the
		// English key. A line that reorders them makes printf read a size_t as a
		// char pointer and takes the game down (1.5.39: "已添加 %s 的 %zu 件。"),
		// so such a line is ignored and the English text is shown instead.
		std::string PlaceholderSignature(std::string_view a_text)
		{
			std::string out;
			for (std::size_t i = 0; i < a_text.size(); ++i) {
				if (a_text[i] != '%') {
					continue;
				}
				if (i + 1 < a_text.size() && a_text[i + 1] == '%') {
					++i;
					continue;
				}
				std::string token{ '%' };
				std::size_t j = i + 1;
				while (j < a_text.size() && std::strchr("-+ #0.*123456789", a_text[j]) != nullptr) {
					token.push_back(a_text[j]);
					++j;
				}
				while (j < a_text.size() && (a_text[j] == 'l' || a_text[j] == 'h' || a_text[j] == 'L' || a_text[j] == 'j' || a_text[j] == 'z' || a_text[j] == 't')) {
					token.push_back(a_text[j]);
					++j;
				}
				if (j < a_text.size()) {
					token.push_back(a_text[j]);
				}
				i = j;
				out += token;
				out.push_back(' ');
			}
			return out;
		}

		bool PlaceholdersMatch(const std::string& a_key, const std::string& a_value)
		{
			return PlaceholderSignature(a_key) == PlaceholderSignature(a_value);
		}

		void Apply()
		{
			auto& state = Get();
			state.mode = Settings::Get().language;
			state.available = HasCjkFont();

			auto requested = ToLower(state.mode);
			if (requested == "auto" || requested.empty()) {
				requested = GameLanguageCode();
			}
			if (requested == "zh" || requested == "cn") {
				requested = "chinese";
			}
			if (requested == "en") {
				requested = "english";
			}
			if (requested.rfind("chin", 0) == 0) {
				requested = "chinese";
			}

			const auto has = [&state](const std::string& a_code) {
				return std::find(state.languages.begin(), state.languages.end(), a_code) != state.languages.end();
			};

			auto code = requested;
			if (!has(code)) {
				code = has("english") ? "english" : state.languages.front();
			}
			if (code == "chinese" && !state.available) {
				if (requested == "chinese") {
					SKSE::log::warn("Language: Chinese was requested but the menu framework has no CJK font loaded; "
									"falling back to English. Install a font such as CN.ttf in "
									"Data/SKSE/Plugins/Fonts and set [Fonts] EnableChinese = true in SKSEMenuFramework.ini.");
				}
				code = has("english") ? "english" : code;
			}

			state.code = code;
			state.chinese = code == "chinese";

			Strings     strings;
			std::size_t rejected = 0;
			if (state.chinese) {
				strings.reserve(std::size(kDictionary) * 2);
				for (const auto& entry : kDictionary) {
					if (!PlaceholdersMatch(entry.en, entry.zh)) {
						if (rejected < 5) {
							SKSE::log::warn("Language: '{}' does not use the placeholders of '{}' in the same order; the English line is shown so a format string cannot take the game down.", entry.zh, entry.en);
						}
						++rejected;
						continue;
					}
					strings.emplace(entry.en, entry.zh);
				}
			}
			state.overrides = 0;
			if (const auto it = Files().find(code); it != Files().end()) {
				for (const auto& [key, value] : it->second) {
					if (!PlaceholdersMatch(key, value)) {
						if (rejected < 5) {
							SKSE::log::warn("Language: '{}' does not use the placeholders of '{}' in the same order; the English line is shown so a format string cannot take the game down.", value, key);
						}
						++rejected;
						continue;
					}
					strings[key] = value;
				}
				state.overrides = it->second.size();
			}
			if (rejected > 0) {
				SKSE::log::warn("Language: {} translated line(s) were ignored because their placeholders do not match the English text.", rejected);
			}
			state.strings = std::move(strings);

			state.fileStatus = std::to_string(state.overrides) + " lines from " + std::string(kFilePrefix) + code + kFileExtension;
			state.status = code + (state.mode == "auto" ? " (auto)" : " (forced)");
			SKSE::log::debug("Language: interface language '{}' (mode '{}'), {} translated lines.", code, state.mode, state.overrides);
		}
	}

	void Init()
	{
		LoadAll();
		Apply();
	}

	void Refresh()
	{
		Apply();
	}

	const char* ModeName() { return Get().mode.c_str(); }

	const char* CurrentCode() { return Get().code.c_str(); }

	std::vector<std::string> Languages() { return Get().languages; }

	const char* LabelForCode(const char* a_code)
	{
		static const std::unordered_map<std::string, std::string> s_names{
			{ "english", "English" },
			{ "chinese", "中文" },
			{ "german", "Deutsch" },
			{ "french", "Français" },
			{ "italian", "Italiano" },
			{ "spanish", "Español" },
			{ "russian", "Русский" },
			{ "polish", "Polski" },
			{ "japanese", "日本語" },
			{ "korean", "한국어" },
			{ "portuguese", "Português" },
			{ "turkish", "Türkçe" },
		};
		static thread_local std::string buffer;
		const std::string code = a_code ? ToLower(a_code) : std::string();
		if (const auto it = s_names.find(code); it != s_names.end()) {
			return it->second.c_str();
		}
		buffer = code;
		if (!buffer.empty()) {
			buffer[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(buffer[0])));
		}
		return buffer.c_str();
	}

	void SetLanguage(const char* a_code)
	{
		Settings::Get().language = (a_code && *a_code) ? ToLower(a_code) : "auto";
		Apply();
	}

	bool Chinese() { return Get().chinese; }

	bool ChineseAvailable() { return Get().available; }

	const char* Status() { return Get().status.c_str(); }

	const char* Tr(const char* a_en)
	{
		if (!a_en) {
			return a_en;
		}
		const auto& strings = Get().strings;
		const auto  it = strings.find(a_en);
		return it == strings.end() ? a_en : it->second.c_str();
	}

	const char* TrId(const char* a_en)
	{
		if (!a_en) {
			return a_en;
		}
		const char* translated = Tr(a_en);
		if (translated == a_en) {
			return a_en;
		}
		// "chinese###english": ImGui displays the part before ### and uses the
		// part after it as the widget id, so ids stay stable across languages.
		static thread_local std::string buffer;
		buffer.assign(translated);
		buffer.append("###");
		buffer.append(a_en);
		return buffer.c_str();
	}

	const char* T(const char* a_en, const char* a_zh)
	{
		if (a_en) {
			const auto& strings = Get().strings;
			const auto  it = strings.find(a_en);
			if (it != strings.end()) {
				return it->second.c_str();
			}
		}
		if (Get().chinese && a_zh) {
			return a_zh;
		}
		return a_en;
	}

	const char* FilePath()
	{
		static thread_local std::string path;
		path = PathFor(Get().code).string();
		return path.c_str();
	}

	void ReloadFile()
	{
		LoadAll();
		Apply();
	}

	std::size_t OverrideCount() { return Get().overrides; }

	const char* FileStatus() { return Get().fileStatus.c_str(); }
}
