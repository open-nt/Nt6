// MIT License - see LICENSE
#pragma once

#include "glb.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gentree {

struct Options {
    std::uint64_t seed = 1;
    float height = 0;      // metres; 0 picks a height from the species range with the seed
    float density = 1.0f;  // multiplies the leaf area, so 0.5 is a sparse crown
    bool leafOff = false;  // no leaves, as in winter
    int maxLeaves = 40000; // leaves in the finest level of detail; their size grows to keep the leaf area
    int blades = 0;        // blades in a grass tuft; 0 uses the species default
    std::string textures;  // directory of textures
    std::map<std::string, float> params;
};

struct LodInfo {
    std::string node;
    std::size_t triangles = 0;
    float maxDistance = 0; // metres; 0 means no limit
};

struct Result {
    Document doc;
    std::vector<LodInfo> lods;
    std::string kind; // "tree", "grass" or "weed"
    std::string id;
    std::string sensor; // sensor material class
    bool hasProxies = false;
    bool felled = false;
    std::string ballistic = "Trunk";
    float height = 0;
};

} // namespace gentree
