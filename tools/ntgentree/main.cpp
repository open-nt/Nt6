// MIT License - see LICENSE
//
// ntgentree: grows trees, grasses and weeds from rules and a seed and writes them as binary glTF with levels of
// detail, wind weights, proxies and sensor properties for lidar and thermal simulation. Run it with --help.

#include "plants.h"
#include "tree.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

#ifndef NT_VERSION_STRING
#define NT_VERSION_STRING "unknown"
#endif

using namespace gentree;

namespace {

const char *kUsage =
    "Usage: ntgentree [options] <species> -o <file.glb>\n"
    "\n"
    "Grows a tree, grass or weed from rules and a seed. The same species, seed and options give the same file.\n"
    "\n"
    "  -o, --output <file>     the glb to write\n"
    "  --seed <n>              the seed (default 1)\n"
    "  --height <m>            the height, instead of one drawn from the species range\n"
    "  --density <x>           multiplies the leaf area of a tree: 0.5 is a sparse crown, 1.3 a dense one\n"
    "  --leaf-off              a tree without leaves, as in winter or for lidar of the branch structure\n"
    "  --max-leaves <n>        leaves in the finest level of detail (default 40000); fewer, larger leaves keep the "
    "area\n"
    "  --blades <n>            blades in a grass tuft\n"
    "  --param <name>=<value>  changes one tree parameter, see --list-params; may be repeated\n"
    "  --textures <dir>        embeds <species>_<slot>.png (slots bark, leaf, blade) from the directory\n"
    "  --asset <file>          writes the asset configuration entry for ntmapgen\n"
    "  --asset-path <path>     the path of the glb inside that entry (default assets/vegetation/<file>)\n"
    "  --list                  lists the species\n"
    "  --list-params           lists the parameters that --param accepts\n"
    "  --textures-plan         prints the textures each species needs, with prompts, as JSON\n"
    "  --version, --help\n";

struct Entry {
    std::string id, kind, region, common, latin;
};

std::vector<Entry> allSpecies() {
    std::vector<Entry> v;
    for (const TreeSpecies &t : makeTreeSpecies())
        v.push_back({t.id, t.kind, t.region, t.common, t.latin});
    for (const GrassSpecies &g : makeGrassSpecies())
        v.push_back({g.id, "grass", g.region, g.common, g.latin});
    for (const WeedSpecies &w : makeWeedSpecies())
        v.push_back({w.id, "weed", w.region, w.common, w.latin});
    return v;
}

struct TexturePlan {
    std::string id, slot, prompt, size;
    bool alpha;
};

std::vector<TexturePlan> texturePlan() {
    std::vector<TexturePlan> v;
    const std::string tail = ", even diffuse lighting, no shadows, no text, no watermark";
    for (const TreeSpecies &t : makeTreeSpecies()) {
        const std::string name = std::string(t.common) + " (" + t.latin + ")";
        v.push_back({t.id, "bark",
                     "Seamless tileable photographic texture of the bark of a " + name +
                         (std::string(t.kind) == "tree" ? " tree, " : " shrub, ") + t.barkDesc +
                         ", flat front-on view filling the whole frame, no sky, no leaves, no moss" + tail,
                     "square_hd", false});
        // Tiny leaves vanish when the background is removed, so those are shown as a dense sprig that fills the frame.
        const std::string subject =
            t.leafLength < 0.04f ? "A dense leafy sprig of " + name + " with many " + t.leafDesc +
                                       ", filling the frame, pointing straight up with its base at the bottom centre"
            : t.leafShape == LeafShape::Needle ? "A single tuft of " + std::string(t.leafDesc) + " of " + name +
                                                     " fanning upwards from one point at the bottom centre"
                                               : "A single " + std::string(t.leafDesc) + " of " + name +
                                                     " pointing straight up with its base at the bottom centre";
        v.push_back({t.id, "leaf", subject + ", isolated on a plain pure white background, flat botanical scan" + tail,
                     "portrait_4_3", true});
    }
    for (const GrassSpecies &g : makeGrassSpecies())
        v.push_back({g.id, "blade",
                     (g.bladeWidth < 0.005f
                          ? "A thick bundle of ten " + std::string(g.bladeDesc) + " blades of " + g.common + " (" +
                                g.latin + "), fanning upwards from one point at the bottom centre"
                          : "A single " + std::string(g.bladeDesc) + " blade of " + g.common + " (" + g.latin +
                                "), long and narrow, standing straight up with its base at the bottom centre") +
                         ", isolated on a plain pure white background, flat macro photograph" + tail,
                     "portrait_16_9", true});
    for (const WeedSpecies &w : makeWeedSpecies())
        v.push_back({w.id, "leaf",
                     "A single " + std::string(w.leafDesc) + " of " + w.common + " (" + w.latin +
                         ") pointing straight up with its base at the bottom centre, isolated on a plain pure white "
                         "background, flat botanical scan" +
                         tail,
                     "portrait_4_3", true});
    return v;
}

bool applyParam(TreeSpecies &sp, const std::string &key, float value) {
    const auto &named = treeParameters();
    const auto it = named.find(key);
    if (it != named.end()) {
        sp.*(it->second) = value;
        return true;
    }
    if (key == "competition") {
        sp.competition = value != 0;
        return true;
    }
    if (key == "stems") {
        sp.stems = std::max(1, static_cast<int>(value));
        return true;
    }
    // Per order of branch: count1, angle1, curve1, tropism1, radius1 and length2 (orders 1 to 3).
    if (key.size() < 2 || key[key.size() - 1] < '1' || key[key.size() - 1] > '3')
        return false;
    const std::size_t o = static_cast<std::size_t>(key[key.size() - 1] - '1');
    const std::string name = key.substr(0, key.size() - 1);
    if (name == "count")
        sp.count[o] = value;
    else if (name == "angle")
        sp.downAngle[o] = value;
    else if (name == "curve")
        sp.curve[o] = value;
    else if (name == "tropism")
        sp.tropism[o] = value;
    else if (name == "radius")
        sp.radiusRatio[o] = value;
    else if (name == "length" && o > 0)
        sp.lengthRatio[o] = value;
    else
        return false;
    return true;
}

void listParams() {
    for (const auto &p : treeParameters())
        std::printf("%s\n", p.first.c_str());
    std::printf("count1..3 angle1..3 curve1..3 tropism1..3 radius1..3 length2..3\n");
}

std::string jsonList(const std::vector<TexturePlan> &plan) {
    std::string o = "[";
    for (std::size_t i = 0; i < plan.size(); ++i) {
        const TexturePlan &t = plan[i];
        o += std::string(i ? ",\n " : "\n ") + "{\"id\":" + jsonEscape(t.id) + ",\"slot\":" + jsonEscape(t.slot) +
             ",\"prompt\":" + jsonEscape(t.prompt) + ",\"image_size\":" + jsonEscape(t.size) +
             ",\"alpha\":" + (t.alpha ? "true" : "false") + "}";
    }
    return o + "\n]\n";
}

std::string baseName(const std::string &path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string assetEntry(const Result &r, const std::string &path) {
    std::string o = "{\n  \"" + r.kind + "." + r.id + "\": {\n    \"gltf\": " + jsonEscape(path) +
                    ",\n    \"kind\": " + jsonEscape(r.kind) + ",\n    \"height\": " + jsonNumber(r.height);
    if (r.hasProxies)
        o += std::string(",\n    \"variants\": { \"intact\": \"LOD0\"") + (r.felled ? ", \"felled\": \"Felled\"" : "") +
             " },\n    \"proxies\": { \"collision\": \"Collision\", " +
             "\"los\": \"Crown\", \"ballistic\": " + jsonEscape(r.ballistic) + ", \"sensor\": \"Crown\" }";
    o += ",\n    \"sensorMaterial\": " + jsonEscape(r.sensor) + "\n  }\n}\n";
    return o;
}

float parseFloat(const std::string &s, const char *what) {
    char *end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (s.empty() || *end != '\0')
        throw std::runtime_error(std::string("invalid ") + what + ": " + s);
    return v;
}

int run(int argc, char **argv) {
    Options opt;
    std::string species, output, assetFile, assetPath;
    std::vector<std::pair<std::string, float>> params;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc)
                throw std::runtime_error("missing value for " + a);
            return argv[++i];
        };
        if (a == "-h" || a == "--help") {
            std::fputs(kUsage, stdout);
            return 0;
        } else if (a == "--version") {
            std::printf("ntgentree %s\n", NT_VERSION_STRING);
            return 0;
        } else if (a == "--list") {
            for (const Entry &e : allSpecies())
                std::printf("%-26s %-5s %-2s %s (%s)\n", e.id.c_str(), e.kind.c_str(), e.region.c_str(),
                            e.common.c_str(), e.latin.c_str());
            return 0;
        } else if (a == "--list-params") {
            listParams();
            return 0;
        } else if (a == "--textures-plan") {
            std::fputs(jsonList(texturePlan()).c_str(), stdout);
            return 0;
        } else if (a == "-o" || a == "--output") {
            output = value();
        } else if (a == "--seed") {
            opt.seed = static_cast<std::uint64_t>(std::strtoull(value().c_str(), nullptr, 10));
        } else if (a == "--height") {
            opt.height = parseFloat(value(), "height");
        } else if (a == "--density") {
            opt.density = parseFloat(value(), "density");
        } else if (a == "--leaf-off") {
            opt.leafOff = true;
        } else if (a == "--max-leaves") {
            opt.maxLeaves = std::max(1, static_cast<int>(parseFloat(value(), "max-leaves")));
        } else if (a == "--blades") {
            opt.blades = std::max(1, static_cast<int>(parseFloat(value(), "blades")));
        } else if (a == "--textures") {
            opt.textures = value();
        } else if (a == "--asset") {
            assetFile = value();
        } else if (a == "--asset-path") {
            assetPath = value();
        } else if (a == "--param") {
            const std::string kv = value();
            const std::size_t eq = kv.find('=');
            if (eq == std::string::npos)
                throw std::runtime_error("--param needs name=value: " + kv);
            params.emplace_back(kv.substr(0, eq), parseFloat(kv.substr(eq + 1), "parameter value"));
        } else if (!a.empty() && a[0] == '-') {
            throw std::runtime_error("unknown option " + a);
        } else if (species.empty()) {
            species = a;
        } else {
            throw std::runtime_error("more than one species: " + species + " and " + a);
        }
    }
    if (species.empty() || output.empty()) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    if (opt.density <= 0)
        throw std::runtime_error("density must be positive");

    Result result;
    bool found = false;
    for (TreeSpecies t : makeTreeSpecies()) {
        if (species != t.id)
            continue;
        for (const auto &p : params)
            if (!applyParam(t, p.first, p.second))
                throw std::runtime_error("unknown parameter " + p.first + " (see --list-params)");
        result = TreeBuilder(t, opt).build();
        found = true;
    }
    if (!found && !params.empty())
        for (const Entry &e : allSpecies())
            if (e.id == species && e.kind != "tree")
                throw std::runtime_error("--param applies to trees only");
    for (const GrassSpecies &g : makeGrassSpecies())
        if (!found && species == g.id) {
            result = buildGrass(g, opt);
            found = true;
        }
    for (const WeedSpecies &w : makeWeedSpecies())
        if (!found && species == w.id) {
            result = buildWeed(w, opt);
            found = true;
        }
    if (!found)
        throw std::runtime_error("unknown species " + species + " (see --list)");

    const std::vector<std::uint8_t> glb = writeGlb(result.doc);
    std::ofstream out(output, std::ios::binary);
    out.write(reinterpret_cast<const char *>(glb.data()), static_cast<std::streamsize>(glb.size()));
    if (!out)
        throw std::runtime_error("cannot write " + output);
    out.close();
    if (!assetFile.empty()) {
        std::ofstream a(assetFile);
        a << assetEntry(result, assetPath.empty() ? "assets/vegetation/" + baseName(output) : assetPath);
        if (!a)
            throw std::runtime_error("cannot write " + assetFile);
    }
    std::printf("%s: %s %s, height %.1f m, %zu bytes\n", output.c_str(), result.kind.c_str(), result.id.c_str(),
                static_cast<double>(result.height), glb.size());
    for (const LodInfo &l : result.lods)
        std::printf("  %s %zu triangles\n", l.node.c_str(), l.triangles);
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "ntgentree: %s\n", e.what());
        return 1;
    }
}
