#include "AssetPath.h"

#include <filesystem>

// used to go from macro to literal
#define VALUE(string) #string
#define TO_LITERAL(string) VALUE(string)


AssetPath::AssetPath()
{
#if defined(ASSET_DIR)
	_assetPath = std::filesystem::absolute(std::string(TO_LITERAL(ASSET_DIR))).string();
#endif

	if (_assetPath.empty())
	{
		_assetPath = std::filesystem::current_path().string();
	}

	spdlog::info("AssetPath initialized to: {}", _assetPath);
}


std::string AssetPath::get(std::string const& address) const
{
	return get(address.c_str());
}

std::string AssetPath::get(char const *address) const
{
    return std::filesystem::path(_assetPath).append(address).string();
}
