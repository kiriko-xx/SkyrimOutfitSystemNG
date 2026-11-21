#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SOS::EspOutfits
{
	struct PluginSummary
	{
		std::string name;
		std::uint32_t order{};
		std::size_t armors{};
		std::size_t outfitArmors{};
	};

	struct PreviewPiece
	{
		std::string name;
		std::string editorId;
		std::uint32_t formId{};  // session-only; never persisted by the preview
		std::uint32_t mask{};
	};

	struct PreviewOutfit
	{
		std::string name;
		std::vector<std::size_t> pieces;
		bool existing{};
	};

	struct Preview
	{
		std::string plugin;
		std::vector<PreviewPiece> pieces;
		std::vector<PreviewOutfit> outfits;
		std::vector<std::size_t> unresolved;
		std::size_t notClothing{};
		bool truncated{};
	};

	struct Result
	{
		std::vector<std::string> names;
		std::size_t skipped{};
	};

	std::vector<PluginSummary> PluginsWithArmor();
	std::string SlotNumbers(std::uint32_t a_mask);
	std::string BaseNameFor(const std::string& a_plugin);
	std::string OutfitNameFor(const std::string& a_base, std::size_t a_index);
	// Planning does not write. Applying revalidates the exact previewed records
	// and never replaces an existing preset. All new presets use XXXO policy.
	Preview PreviewFromPlugin(const std::string& a_plugin);
	Result ApplyPreview(const Preview& a_preview, bool a_favorite);
}
