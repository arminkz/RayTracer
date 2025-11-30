#pragma once

#include "stdafx.h"
#include "utils/Singleton.h"

class AssetPath : public Singleton<AssetPath> 
{
friend class Singleton<AssetPath>;

public:

    AssetPath();

    std::string get(std::string const& address) const; // used for std::string inputs
    std::string get(char const * address) const; // used for char* inputs

private:

    std::string _assetPath {}; // Root asset path (gonna be set in constructor from CMake)
};