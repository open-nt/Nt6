// MIT License - see LICENSE
//
// The species tables of ntgentree: the parameters of each tree, grass and weed that it can grow. The
// values are chosen to give the typical habit of a mature plant (height, crown width, branching angles,
// leaf size and leaf area index); they are not fitted to measurements. Every float member of TreeSpecies
// can be changed from the command line with --param name=value (see treeParameters()).
#pragma once

#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace gentree {

// A colour in linear RGB.
struct Color {
    float r = 1, g = 1, b = 1;
};

inline float srgbToLinear(int c) {
    const float f = static_cast<float>(c) / 255.0f;
    return f <= 0.04045f ? f / 12.92f : std::pow((f + 0.055f) / 1.055f, 2.4f);
}
inline Color rgb(int r, int g, int b) { return {srgbToLinear(r), srgbToLinear(g), srgbToLinear(b)}; }
inline float luminance(Color c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

enum class LeafShape { Broad, Lanceolate, Needle };

struct TreeSpecies {
    const char *id = "";
    const char *common = "";
    const char *latin = "";
    const char *region = "";   // "uk" or "au"
    const char *kind = "tree"; // "tree", "shrub" (scrub) or "hedge"
    const char *sensor = "";   // sensor material class, see opticsFor()
    const char *barkDesc = "";
    const char *leafDesc = "";

    float heightMin = 10, heightMax = 20; // metres
    float trunkRadiusRatio = 0.025f;      // trunk radius at breast height over the height
    float flare = 0.5f;                   // widening of the trunk at the ground
    float leaderEnd = 0.6f;               // where the trunk ends, as a fraction of the height; 1 is excurrent
    float crownBase = 0.25f;              // height of the first branches, as a fraction of the height
    float crownRadius = 0.3f;             // widest crown radius over the height
    bool conic = false;                   // a cone (conifers) instead of a rounded crown
    float profileA = 0.75f;               // crown profile exponent; lower is fuller towards the base
    float wobble = 5.0f;                  // random bend of the trunk in degrees per segment
    float lean = 4.0f;                    // tilt of the trunk in degrees
    int levels = 3;                       // orders of branches above the trunk, 1 to 3
    // Twigs fill the crown by space colonization (Runions et al. 2007): about this many attraction points for each
    // cubic metre of crown, denser near its surface, pull twigs towards them until the crown is full; 0 draws twigs
    // at random instead. asym is how lopsided the crown is.
    float fill = 1.5f;
    // Instead of branching at random, the whole crown can grow as a competition for space (Palubicki et al. 2009):
    // buds that find free space grow, and the vigor of a branch is shared between its main axis and its side
    // branch with weight apical for the axis (above 0.5 gives a tree with a leader, below it a spreading one);
    // side branches that gain no space for shed iterations die.
    bool competition = false;
    float apical = 0.5f;
    float shed = 12.0f;
    float asym = 0.2f;
    int stems = 1;          // stems from the ground; a shrub has several
    float stemLean = 15.0f; // how far the stems of a shrub spread, in degrees
    // A hedge is a clipped box: this long and wide (metres), as high as the height. Its stems stand in a row.
    float hedgeLength = 0.0f;
    float hedgeWidth = 0.0f;

    // Per order of branch: how many each parent carries, length relative to the parent, angle from the
    // parent in degrees, bend in degrees along the branch, tropism (positive grows up, negative hangs) and
    // radius relative to the parent where the branch leaves it.
    float count[3] = {20, 8, 6};
    float lengthRatio[3] = {1.0f, 0.55f, 0.5f};
    float downAngle[3] = {60, 55, 50};
    float curve[3] = {30, 35, 30};
    float tropism[3] = {0.05f, 0.0f, -0.05f};
    float radiusRatio[3] = {0.45f, 0.6f, 0.6f};

    LeafShape leafShape = LeafShape::Broad;
    float leafLength = 0.1f;  // metres
    float leafAspect = 0.55f; // width over length
    float leafDroop = 0.15f;  // 0 grows outwards, 1 hangs straight down
    float lai = 4.0f;         // leaf area index of the full crown
    bool deciduous = true;

    Color bark = rgb(90, 75, 62);
    Color leaf = rgb(60, 95, 35);
};

struct GrassSpecies {
    const char *id = "";
    const char *common = "";
    const char *latin = "";
    const char *region = "";
    const char *bladeDesc = "";

    float heightMin = 0.3f, heightMax = 0.6f; // metres
    float radius = 0.15f;                     // radius of the clump at the ground
    int blades = 160;
    float bladeWidth = 0.006f;
    float lean = 0.35f; // mean angle of a blade from vertical, radians
    float leanVar = 0.2f;
    float curl = 1.2f;      // how far the tip of a blade bends over, radians
    bool seedHeads = false; // stalks that rise above the leaves and carry seeds
    int seedHeadCount = 12;
    float seedStalkRatio = 1.5f; // stalk height over leaf height
    bool hummock = false;        // stiff radial blades in a dome, like spinifex
    Color blade = rgb(95, 130, 55);
    Color seed = rgb(190, 160, 95);
    Color baseTint = rgb(150, 170, 110); // brightness at the base of a blade relative to its tip
};

enum class WeedForm { Rosette, Stemmed, Frond, Cane };

struct WeedSpecies {
    const char *id = "";
    const char *common = "";
    const char *latin = "";
    const char *region = "";
    const char *leafDesc = "";

    WeedForm form = WeedForm::Rosette;
    int leaves = 14;
    float leafLength = 0.2f; // metres
    float leafAspect = 0.3f;
    float petiole = 0.0f;   // fraction of the leaf length that is a bare stalk
    float elevation = 0.5f; // angle of the leaves above the ground, radians
    float curl = 0.8f;
    float stemHeight = 0.0f; // flowering stem or main stem
    int stems = 1;
    int stemLeaves = 0;        // leaves up the flowering stem, smaller towards the top
    float flowerRadius = 0.0f; // 0 for no flowers
    int petals = 24;
    bool flowerSpike = false; // many small flowers along the stem instead of one head
    bool flowerBall = false;  // petals radiate in all directions, like a thistle
    Color leaf = rgb(70, 110, 45);
    Color stem = rgb(90, 120, 60);
    Color flower = rgb(240, 200, 30);
};

// Sensor properties for lidar and thermal simulation. The values are typical, not measured for a species:
// reflectance of vegetation in the near infrared and its drop at 1550 nm (water absorption), emissivity in the
// long-wave infrared (8-14 um) and absorptivity of sunlight.
struct Optics {
    float lidar905 = 0.45f;
    float lidar1550 = 0.2f;
    float emissivity = 0.97f;
    float solarAbsorptivity = 0.5f;
};

inline Optics opticsFor(const std::string &sensor) {
    Optics o;
    if (sensor == "vegetation.conifer" || sensor == "vegetation.needleleaf") {
        o.lidar905 = 0.32f;
        o.lidar1550 = 0.17f;
        o.solarAbsorptivity = 0.55f;
    } else if (sensor == "vegetation.sclerophyll") {
        o.lidar905 = 0.42f;
        o.lidar1550 = 0.22f;
    } else if (sensor == "vegetation.grass") {
        o.lidar905 = 0.5f;
        o.lidar1550 = 0.25f;
        o.emissivity = 0.98f;
    } else if (sensor == "vegetation.forb") {
        o.lidar905 = 0.5f;
        o.lidar1550 = 0.2f;
        o.emissivity = 0.98f;
    }
    return o;
}

// Bark is lighter and brighter in the near infrared the paler it is.
inline Optics barkOptics(Color bark) {
    const float l = luminance(bark);
    Optics o;
    o.lidar905 = 0.12f + 0.55f * std::sqrt(l);
    o.lidar1550 = 0.08f + 0.45f * std::sqrt(l);
    o.emissivity = 0.95f;
    o.solarAbsorptivity = 0.95f - 0.7f * std::sqrt(l);
    return o;
}

// Shrubs and scrub: several stems from the ground and a low, rounded crown.
inline void addShrubs(std::vector<TreeSpecies> &v) {
    auto shrub = [](const char *id, const char *common, const char *latin, const char *region, const char *sensor,
                    const char *barkDesc, const char *leafDesc, float hMin, float hMax) {
        TreeSpecies t;
        t.id = id;
        t.common = common;
        t.latin = latin;
        t.region = region;
        t.kind = "shrub";
        t.sensor = sensor;
        t.barkDesc = barkDesc;
        t.leafDesc = leafDesc;
        t.heightMin = hMin;
        t.heightMax = hMax;
        t.trunkRadiusRatio = 0.012f;
        t.flare = 0.15f;
        t.leaderEnd = 0.9f;
        t.crownBase = 0.1f;
        t.crownRadius = 0.4f;
        t.profileA = 0.6f;
        t.wobble = 8;
        t.lean = 6;
        t.stems = 6;
        t.stemLean = 18;
        t.fill = 8.0f;
        t.count[0] = 9;
        t.count[1] = 7;
        t.count[2] = 6;
        t.downAngle[0] = 50;
        return t;
    };
    {
        TreeSpecies t = shrub("ulex_europaeus", "Gorse", "Ulex europaeus", "uk", "vegetation.needleleaf",
                              "green ridged spiny stems", "short stiff green spine of gorse", 1.2f, 2.2f);
        t.stems = 7;
        t.leafShape = LeafShape::Needle;
        t.leafLength = 0.03f;
        t.leafAspect = 0.15f;
        t.lai = 5.0f;
        t.deciduous = false;
        t.bark = rgb(70, 85, 45);
        t.leaf = rgb(70, 100, 45);
        v.push_back(t);
    }
    {
        TreeSpecies t =
            shrub("corylus_avellana", "Hazel", "Corylus avellana", "uk", "vegetation.broadleaf",
                  "smooth brown bark with pale horizontal lenticels", "rounded toothed hazel leaf", 3.5f, 5.5f);
        t.crownRadius = 0.45f;
        t.leafLength = 0.09f;
        t.leafAspect = 0.85f;
        t.lai = 4.5f;
        t.bark = rgb(120, 95, 75);
        t.leaf = rgb(75, 115, 45);
        v.push_back(t);
    }
    {
        TreeSpecies t = shrub("calluna_vulgaris", "Heather", "Calluna vulgaris", "uk", "vegetation.needleleaf",
                              "thin wiry dark brown stems", "tiny scale-like heather leaves along a stem", 0.35f, 0.6f);
        t.stems = 9;
        t.stemLean = 25;
        t.crownRadius = 0.55f;
        t.leafShape = LeafShape::Needle;
        t.leafLength = 0.01f;
        t.leafAspect = 0.4f;
        t.lai = 4.0f;
        t.deciduous = false;
        t.bark = rgb(80, 60, 50);
        t.leaf = rgb(85, 110, 60);
        v.push_back(t);
    }
    {
        TreeSpecies t =
            shrub("prunus_spinosa", "Blackthorn", "Prunus spinosa", "uk", "vegetation.broadleaf",
                  "dark almost black bark with sharp thorns", "small oval toothed blackthorn leaf", 2.5f, 4.0f);
        t.stems = 5;
        t.crownRadius = 0.45f;
        t.leafLength = 0.04f;
        t.leafAspect = 0.6f;
        t.lai = 3.5f;
        t.bark = rgb(45, 40, 40);
        t.leaf = rgb(65, 100, 45);
        v.push_back(t);
    }
    {
        TreeSpecies t = shrub("leptospermum_laevigatum", "Coast tea-tree", "Leptospermum laevigatum", "au",
                              "vegetation.sclerophyll", "grey-brown flaky bark on twisted stems",
                              "small narrow grey-green tea-tree leaf", 3.0f, 5.0f);
        t.stems = 3;
        t.stemLean = 25;
        t.wobble = 12;
        t.crownRadius = 0.45f;
        t.leafShape = LeafShape::Lanceolate;
        t.leafLength = 0.025f;
        t.leafAspect = 0.35f;
        t.leafDroop = 0.2f;
        t.lai = 3.0f;
        t.deciduous = false;
        t.bark = rgb(130, 115, 100);
        t.leaf = rgb(100, 125, 80);
        v.push_back(t);
    }
    {
        TreeSpecies t =
            shrub("atriplex_nummularia", "Old man saltbush", "Atriplex nummularia", "au", "vegetation.sclerophyll",
                  "pale grey-brown thin stems", "rounded silvery grey-green saltbush leaf", 1.5f, 2.5f);
        t.crownRadius = 0.5f;
        t.leafLength = 0.035f;
        t.leafAspect = 0.8f;
        t.lai = 3.0f;
        t.deciduous = false;
        t.bark = rgb(140, 130, 110);
        t.leaf = rgb(140, 160, 130);
        v.push_back(t);
    }
    {
        TreeSpecies t =
            shrub("melaleuca_uncinata", "Broom honey-myrtle", "Melaleuca uncinata", "au", "vegetation.needleleaf",
                  "papery grey-brown bark", "fine needle-like honey-myrtle foliage", 2.5f, 4.0f);
        t.stems = 5;
        t.stemLean = 20;
        t.crownRadius = 0.4f;
        t.leafShape = LeafShape::Needle;
        t.leafLength = 0.05f;
        t.leafAspect = 0.08f;
        t.leafDroop = 0.3f;
        t.lai = 4.0f;
        t.deciduous = false;
        t.bark = rgb(120, 100, 85);
        t.leaf = rgb(90, 120, 80);
        v.push_back(t);
    }
}

// Hedges: a clipped segment that is placed end to end along a line.
inline void addHedges(std::vector<TreeSpecies> &v) {
    auto hedge = [](const char *id, const char *common, const char *latin, const char *region, const char *sensor,
                    const char *barkDesc, const char *leafDesc, float hMin, float hMax, float width) {
        TreeSpecies t;
        t.id = id;
        t.common = common;
        t.latin = latin;
        t.region = region;
        t.kind = "hedge";
        t.sensor = sensor;
        t.barkDesc = barkDesc;
        t.leafDesc = leafDesc;
        t.heightMin = hMin;
        t.heightMax = hMax;
        t.hedgeLength = 4.0f;
        t.hedgeWidth = width;
        t.fill = 60.0f;
        t.asym = 0.0f;
        t.trunkRadiusRatio = 0.006f;
        t.flare = 0.1f;
        t.leaderEnd = 0.9f;
        t.crownBase = 0.08f;
        t.wobble = 8;
        t.lean = 4;
        t.stemLean = 8;
        t.count[0] = 8;
        t.count[1] = 6;
        t.count[2] = 6;
        t.downAngle[0] = 40;
        t.downAngle[1] = 45;
        t.downAngle[2] = 45;
        t.lai = 6.0f;
        return t;
    };
    {
        TreeSpecies t =
            hedge("crataegus_monogyna_hedge", "Hawthorn hedge", "Crataegus monogyna", "uk", "vegetation.broadleaf",
                  "grey-brown twiggy stems with thorns", "deeply lobed hawthorn leaf", 1.8f, 2.4f, 1.2f);
        t.leafLength = 0.04f;
        t.leafAspect = 0.9f;
        t.bark = rgb(92, 74, 64);
        t.leaf = rgb(58, 98, 42);
        v.push_back(t);
    }
    {
        TreeSpecies t = hedge("taxus_baccata_hedge", "Yew hedge", "Taxus baccata", "uk", "vegetation.conifer",
                              "reddish-brown thin scaly bark", "dark green flat yew needle", 1.8f, 2.6f, 1.0f);
        t.leafShape = LeafShape::Needle;
        t.leafLength = 0.03f;
        t.leafAspect = 0.12f;
        t.lai = 7.0f;
        t.deciduous = false;
        t.bark = rgb(120, 75, 60);
        t.leaf = rgb(30, 65, 40);
        v.push_back(t);
    }
    {
        TreeSpecies t = hedge("fagus_sylvatica_hedge", "Beech hedge", "Fagus sylvatica", "uk", "vegetation.broadleaf",
                              "smooth grey bark", "ovate beech leaf with parallel veins", 1.8f, 2.4f, 0.8f);
        t.leafLength = 0.06f;
        t.leafAspect = 0.6f;
        t.bark = rgb(140, 132, 122);
        t.leaf = rgb(70, 110, 40);
        v.push_back(t);
    }
    {
        TreeSpecies t = hedge("westringia_fruticosa_hedge", "Coastal rosemary hedge", "Westringia fruticosa", "au",
                              "vegetation.sclerophyll", "thin grey-brown stems",
                              "narrow grey-green whorled rosemary-like leaf", 1.0f, 1.6f, 1.0f);
        t.leafShape = LeafShape::Lanceolate;
        t.leafLength = 0.025f;
        t.leafAspect = 0.18f;
        t.deciduous = false;
        t.bark = rgb(120, 105, 90);
        t.leaf = rgb(110, 135, 105);
        v.push_back(t);
    }
    {
        TreeSpecies t =
            hedge("syzygium_smithii_hedge", "Lilly pilly hedge", "Syzygium smithii", "au", "vegetation.sclerophyll",
                  "smooth pale brown stems", "glossy lanceolate lilly pilly leaf", 2.0f, 3.0f, 1.2f);
        t.leafShape = LeafShape::Lanceolate;
        t.leafLength = 0.07f;
        t.leafAspect = 0.35f;
        t.deciduous = false;
        t.bark = rgb(140, 115, 95);
        t.leaf = rgb(45, 105, 50);
        v.push_back(t);
    }
}

inline std::vector<TreeSpecies> makeTreeSpecies() {
    std::vector<TreeSpecies> v;
    {
        TreeSpecies t;
        t.id = "quercus_robur";
        t.competition = true;
        t.apical = 0.47f;
        // Tallo, British Isles (n=57): height 50th-90th percentile, trunk ratio 75th and crown ratio 90th percentile.
        t.common = "English oak";
        t.latin = "Quercus robur";
        t.region = "uk";
        t.sensor = "vegetation.broadleaf";
        t.barkDesc = "deeply fissured grey-brown ridges";
        t.leafDesc = "lobed oak leaf";
        t.heightMin = 22.0f;
        t.heightMax = 27.0f;
        t.trunkRadiusRatio = 0.012f;
        t.leaderEnd = 0.55f;
        t.crownBase = 0.22f;
        t.crownRadius = 0.303f;
        t.profileA = 0.75f;
        t.wobble = 6;
        t.leafLength = 0.10f;
        t.leafAspect = 0.55f;
        t.lai = 4.0f;
        t.bark = rgb(88, 74, 62);
        t.leaf = rgb(62, 98, 36);
        v.push_back(t);
    }
    {
        TreeSpecies t;
        t.id = "fagus_sylvatica";
        t.competition = true;
        t.apical = 0.55f;
        // Tallo, Europe (n=6546): height 50th-90th percentile, trunk ratio 75th and crown ratio 90th percentile.
        t.common = "European beech";
        t.latin = "Fagus sylvatica";
        t.region = "uk";
        t.sensor = "vegetation.broadleaf";
        t.barkDesc = "smooth silver-grey";
        t.leafDesc = "ovate beech leaf with parallel veins";
        t.heightMin = 21.0f;
        t.heightMax = 29.0f;
        t.trunkRadiusRatio = 0.0086f;
        t.leaderEnd = 0.7f;
        t.crownBase = 0.25f;
        t.crownRadius = 0.273f;
        t.profileA = 0.7f;
        t.count[0] = 30;
        t.count[1] = 8;
        t.count[2] = 6;
        t.leafLength = 0.08f;
        t.leafAspect = 0.6f;
        t.lai = 5.0f;
        t.bark = rgb(140, 132, 122);
        t.leaf = rgb(70, 110, 40);
        v.push_back(t);
    }
    {
        TreeSpecies t;
        t.id = "betula_pendula";
        t.competition = true;
        t.apical = 0.55f;
        // Tallo, Europe (n=656): height 50th-90th percentile, trunk ratio 75th and crown ratio 90th percentile.
        t.common = "Silver birch";
        t.latin = "Betula pendula";
        t.region = "uk";
        t.sensor = "vegetation.broadleaf";
        t.barkDesc = "white papery bark with black horizontal lenticels";
        t.leafDesc = "small triangular-ovate serrated birch leaf";
        t.heightMin = 22.0f;
        t.heightMax = 33.0f;
        t.trunkRadiusRatio = 0.0057f;
        t.leaderEnd = 0.85f;
        t.crownBase = 0.3f;
        t.crownRadius = 0.162f;
        t.profileA = 0.9f;
        t.count[0] = 22;
        t.count[1] = 8;
        t.count[2] = 8;
        t.downAngle[0] = 45;
        t.tropism[1] = -0.25f;
        t.tropism[2] = -0.55f;
        t.leafLength = 0.05f;
        t.leafAspect = 0.8f;
        t.leafDroop = 0.6f;
        t.lai = 2.5f;
        t.bark = rgb(225, 220, 210);
        t.leaf = rgb(95, 140, 50);
        v.push_back(t);
    }
    {
        TreeSpecies t;
        t.id = "pinus_sylvestris";
        t.competition = true;
        t.apical = 0.64f;
        // Tallo, Europe (n=13088): height 50th-90th percentile, trunk ratio 75th and crown ratio 90th percentile.
        t.common = "Scots pine";
        t.latin = "Pinus sylvestris";
        t.region = "uk";
        t.sensor = "vegetation.conifer";
        t.barkDesc = "orange-red flaking upper bark over grey-brown plates";
        t.leafDesc = "pair of blue-green pine needles";
        t.heightMin = 17.0f;
        t.heightMax = 22.0f;
        t.trunkRadiusRatio = 0.0103f;
        t.leaderEnd = 1.0f;
        t.crownBase = 0.55f;
        t.crownRadius = 0.222f;
        t.conic = true;
        t.profileA = 0.8f;
        t.levels = 2;
        t.count[0] = 44;
        t.count[1] = 12;
        t.lengthRatio[1] = 0.5f;
        t.downAngle[0] = 75;
        t.downAngle[1] = 60;
        t.curve[0] = 25;
        t.curve[1] = 20;
        t.tropism[0] = -0.05f;
        t.tropism[1] = 0.1f;
        t.leafShape = LeafShape::Needle;
        t.leafLength = 0.12f;
        t.leafAspect = 0.12f;
        t.leafDroop = 0.2f;
        t.lai = 3.5f;
        t.deciduous = false;
        t.bark = rgb(140, 88, 58);
        t.leaf = rgb(50, 85, 55);
        v.push_back(t);
    }
    {
        TreeSpecies t;
        t.id = "crataegus_monogyna";
        t.competition = true;
        t.apical = 0.45f;
        // Tallo, world (n=241): height 50th-90th percentile, trunk ratio 75th and crown ratio 90th percentile.
        t.common = "Common hawthorn";
        t.latin = "Crataegus monogyna";
        t.region = "uk";
        t.sensor = "vegetation.broadleaf";
        t.barkDesc = "grey-brown bark with shallow vertical cracks";
        t.leafDesc = "deeply lobed hawthorn leaf";
        t.heightMin = 4.5f;
        t.heightMax = 6.0f;
        t.trunkRadiusRatio = 0.0183f;
        t.leaderEnd = 0.5f;
        t.crownBase = 0.15f;
        t.crownRadius = 0.5f;
        t.count[0] = 16;
        t.leafLength = 0.04f;
        t.leafAspect = 0.9f;
        t.lai = 4.0f;
        t.bark = rgb(92, 74, 64);
        t.leaf = rgb(58, 98, 42);
        v.push_back(t);
    }
    {
        TreeSpecies t;
        t.id = "eucalyptus_camaldulensis";
        t.competition = true;
        t.apical = 0.45f;
        t.common = "River red gum";
        t.latin = "Eucalyptus camaldulensis";
        t.region = "au";
        t.sensor = "vegetation.sclerophyll";
        t.barkDesc = "mottled smooth bark in cream, grey and tan with shedding patches, rough stocking at the base";
        t.leafDesc = "long narrow lanceolate eucalyptus leaf";
        t.heightMin = 20;
        t.heightMax = 32;
        t.trunkRadiusRatio = 0.04f;
        t.flare = 0.7f;
        t.leaderEnd = 0.5f;
        t.crownBase = 0.35f;
        t.crownRadius = 0.35f;
        t.wobble = 7;
        t.count[0] = 16;
        t.count[1] = 7;
        t.count[2] = 7;
        t.downAngle[0] = 45;
        t.downAngle[1] = 50;
        t.downAngle[2] = 45;
        t.curve[0] = 45;
        t.curve[1] = 40;
        t.tropism[0] = 0.15f;
        t.tropism[1] = 0.0f;
        t.tropism[2] = -0.3f;
        t.radiusRatio[0] = 0.5f;
        t.leafShape = LeafShape::Lanceolate;
        t.leafLength = 0.14f;
        t.leafAspect = 0.14f;
        t.leafDroop = 0.85f;
        t.lai = 1.6f;
        t.deciduous = false;
        t.bark = rgb(150, 140, 125);
        t.leaf = rgb(95, 115, 70);
        v.push_back(t);
    }
    {
        TreeSpecies t;
        t.id = "corymbia_maculata";
        t.competition = true;
        t.apical = 0.58f;
        t.common = "Spotted gum";
        t.latin = "Corymbia maculata";
        t.region = "au";
        t.sensor = "vegetation.sclerophyll";
        t.barkDesc = "smooth pale grey and cream bark with dimpled spots where patches have shed";
        t.leafDesc = "long narrow lanceolate gum leaf";
        t.heightMin = 25;
        t.heightMax = 40;
        t.trunkRadiusRatio = 0.02f;
        t.leaderEnd = 0.8f;
        t.crownBase = 0.55f;
        t.crownRadius = 0.18f;
        t.count[0] = 14;
        t.count[1] = 7;
        t.count[2] = 6;
        t.downAngle[0] = 40;
        t.downAngle[1] = 45;
        t.downAngle[2] = 40;
        t.tropism[0] = 0.2f;
        t.tropism[2] = -0.2f;
        t.leafShape = LeafShape::Lanceolate;
        t.leafLength = 0.16f;
        t.leafAspect = 0.14f;
        t.leafDroop = 0.8f;
        t.lai = 1.8f;
        t.deciduous = false;
        t.bark = rgb(200, 190, 170);
        t.leaf = rgb(80, 110, 60);
        v.push_back(t);
    }
    {
        TreeSpecies t;
        t.id = "corymbia_aparrerinja";
        t.competition = true;
        t.apical = 0.4f;
        t.common = "Ghost gum";
        t.latin = "Corymbia aparrerinja";
        t.region = "au";
        t.sensor = "vegetation.sclerophyll";
        t.barkDesc = "smooth chalky white bark with faint pale grey streaks";
        t.leafDesc = "narrow lanceolate gum leaf";
        t.heightMin = 10;
        t.heightMax = 16;
        t.trunkRadiusRatio = 0.04f;
        t.leaderEnd = 0.4f;
        t.crownBase = 0.3f;
        t.crownRadius = 0.45f;
        t.count[0] = 12;
        t.count[1] = 7;
        t.count[2] = 6;
        t.downAngle[0] = 50;
        t.tropism[2] = -0.3f;
        t.leafShape = LeafShape::Lanceolate;
        t.leafLength = 0.12f;
        t.leafAspect = 0.13f;
        t.leafDroop = 0.8f;
        t.lai = 1.3f;
        t.deciduous = false;
        t.bark = rgb(235, 235, 225);
        t.leaf = rgb(110, 130, 75);
        v.push_back(t);
    }
    {
        TreeSpecies t;
        t.id = "acacia_pycnantha";
        t.competition = true;
        t.apical = 0.45f;
        t.common = "Golden wattle";
        t.latin = "Acacia pycnantha";
        t.region = "au";
        t.sensor = "vegetation.sclerophyll";
        t.barkDesc = "dark brown rough bark";
        t.leafDesc = "sickle-shaped acacia phyllode";
        t.heightMin = 4;
        t.heightMax = 8;
        t.trunkRadiusRatio = 0.035f;
        t.leaderEnd = 0.5f;
        t.crownBase = 0.12f;
        t.crownRadius = 0.5f;
        t.count[0] = 14;
        t.downAngle[0] = 50;
        t.leafShape = LeafShape::Lanceolate;
        t.leafLength = 0.12f;
        t.leafAspect = 0.2f;
        t.leafDroop = 0.3f;
        t.lai = 2.8f;
        t.deciduous = false;
        t.bark = rgb(80, 60, 45);
        t.leaf = rgb(70, 115, 60);
        v.push_back(t);
    }
    {
        TreeSpecies t;
        t.id = "casuarina_cunninghamiana";
        t.competition = true;
        t.apical = 0.6f;
        t.common = "River she-oak";
        t.latin = "Casuarina cunninghamiana";
        t.region = "au";
        t.sensor = "vegetation.needleleaf";
        t.barkDesc = "dark grey-brown rough fissured bark";
        t.leafDesc = "drooping fine green jointed branchlets";
        t.heightMin = 18;
        t.heightMax = 28;
        t.trunkRadiusRatio = 0.025f;
        t.leaderEnd = 0.9f;
        t.crownBase = 0.25f;
        t.crownRadius = 0.2f;
        t.count[0] = 22;
        t.count[1] = 8;
        t.count[2] = 10;
        t.downAngle[0] = 55;
        t.tropism[1] = -0.2f;
        t.tropism[2] = -0.5f;
        t.leafShape = LeafShape::Needle;
        t.leafLength = 0.18f;
        t.leafAspect = 0.05f;
        t.leafDroop = 0.9f;
        t.lai = 3.0f;
        t.deciduous = false;
        t.bark = rgb(95, 75, 60);
        t.leaf = rgb(75, 105, 65);
        v.push_back(t);
    }
    addShrubs(v);
    addHedges(v);
    return v;
}

inline std::vector<GrassSpecies> makeGrassSpecies() {
    std::vector<GrassSpecies> v;
    {
        GrassSpecies g;
        g.id = "dactylis_glomerata";
        g.common = "Cocksfoot";
        g.latin = "Dactylis glomerata";
        g.region = "uk";
        g.bladeDesc = "broad flat blue-green blade with a pointed tip";
        g.heightMin = 0.4f;
        g.heightMax = 0.7f;
        g.radius = 0.12f;
        g.blades = 120;
        g.bladeWidth = 0.009f;
        g.seedHeads = true;
        g.seedHeadCount = 10;
        g.seedStalkRatio = 1.7f;
        g.blade = rgb(85, 125, 70);
        g.seed = rgb(150, 150, 95);
        v.push_back(g);
    }
    {
        GrassSpecies g;
        g.id = "lolium_perenne";
        g.common = "Perennial ryegrass";
        g.latin = "Lolium perenne";
        g.region = "uk";
        g.bladeDesc = "narrow glossy dark green blade with a pointed tip";
        g.heightMin = 0.15f;
        g.heightMax = 0.3f;
        g.radius = 0.07f;
        g.blades = 80;
        g.bladeWidth = 0.005f;
        g.curl = 0.9f;
        g.blade = rgb(70, 125, 50);
        v.push_back(g);
    }
    {
        GrassSpecies g;
        g.id = "deschampsia_cespitosa";
        g.common = "Tufted hair-grass";
        g.latin = "Deschampsia cespitosa";
        g.region = "uk";
        g.bladeDesc = "narrow ridged stiff blade";
        g.heightMin = 0.5f;
        g.heightMax = 0.9f;
        g.radius = 0.2f;
        g.blades = 240;
        g.bladeWidth = 0.004f;
        g.lean = 0.5f;
        g.seedHeads = true;
        g.seedHeadCount = 14;
        g.seedStalkRatio = 1.8f;
        g.blade = rgb(90, 125, 60);
        g.seed = rgb(170, 150, 110);
        v.push_back(g);
    }
    {
        GrassSpecies g;
        g.id = "themeda_triandra";
        g.common = "Kangaroo grass";
        g.latin = "Themeda triandra";
        g.region = "au";
        g.bladeDesc = "narrow reddish-green blade with a pointed tip";
        g.heightMin = 0.5f;
        g.heightMax = 0.9f;
        g.radius = 0.12f;
        g.blades = 110;
        g.bladeWidth = 0.004f;
        g.lean = 0.3f;
        g.seedHeads = true;
        g.seedHeadCount = 12;
        g.seedStalkRatio = 1.7f;
        g.blade = rgb(130, 135, 70);
        g.seed = rgb(150, 85, 40);
        v.push_back(g);
    }
    {
        GrassSpecies g;
        g.id = "poa_labillardierei";
        g.common = "Common tussock-grass";
        g.latin = "Poa labillardierei";
        g.region = "au";
        g.bladeDesc = "fine blue-green arching blade";
        g.heightMin = 0.6f;
        g.heightMax = 1.0f;
        g.radius = 0.25f;
        g.blades = 300;
        g.bladeWidth = 0.003f;
        g.lean = 0.55f;
        g.curl = 1.6f;
        g.blade = rgb(110, 140, 95);
        v.push_back(g);
    }
    {
        GrassSpecies g;
        g.id = "triodia_basedowii";
        g.common = "Lobed spinifex";
        g.latin = "Triodia basedowii";
        g.region = "au";
        g.bladeDesc = "rigid needle-like spiny grey-green blade";
        g.heightMin = 0.5f;
        g.heightMax = 0.8f;
        g.radius = 0.1f;
        g.blades = 520;
        g.bladeWidth = 0.0025f;
        g.lean = 1.0f;
        g.leanVar = 0.35f;
        g.curl = 0.15f;
        g.hummock = true;
        g.blade = rgb(150, 150, 95);
        g.baseTint = rgb(120, 110, 80);
        v.push_back(g);
    }
    {
        GrassSpecies g;
        g.id = "microlaena_stipoides";
        g.common = "Weeping grass";
        g.latin = "Microlaena stipoides";
        g.region = "au";
        g.bladeDesc = "soft narrow light green drooping blade";
        g.heightMin = 0.2f;
        g.heightMax = 0.45f;
        g.radius = 0.1f;
        g.blades = 90;
        g.bladeWidth = 0.004f;
        g.lean = 0.6f;
        g.curl = 1.5f;
        g.blade = rgb(120, 155, 80);
        v.push_back(g);
    }
    return v;
}

inline std::vector<WeedSpecies> makeWeedSpecies() {
    std::vector<WeedSpecies> v;
    {
        WeedSpecies w;
        w.id = "taraxacum_officinale";
        w.common = "Dandelion";
        w.latin = "Taraxacum officinale";
        w.region = "uk";
        w.leafDesc = "deeply toothed lobed dandelion leaf";
        w.leaves = 12;
        w.leafLength = 0.2f;
        w.leafAspect = 0.22f;
        w.elevation = 0.35f;
        w.stemHeight = 0.25f;
        w.stems = 3;
        w.flowerRadius = 0.025f;
        w.petals = 40;
        w.flower = rgb(250, 205, 20);
        v.push_back(w);
    }
    {
        WeedSpecies w;
        w.id = "rumex_obtusifolius";
        w.common = "Broad-leaved dock";
        w.latin = "Rumex obtusifolius";
        w.region = "uk";
        w.leafDesc = "large broad oval dock leaf with a wavy edge";
        w.leaves = 9;
        w.leafLength = 0.35f;
        w.leafAspect = 0.45f;
        w.petiole = 0.25f;
        w.elevation = 0.7f;
        w.stemHeight = 1.1f;
        w.stems = 1;
        w.stemLeaves = 7;
        w.flowerRadius = 0.006f;
        w.petals = 5;
        w.flowerSpike = true;
        w.stem = rgb(130, 90, 60);
        w.flower = rgb(140, 70, 40);
        v.push_back(w);
    }
    {
        WeedSpecies w;
        w.id = "cirsium_vulgare";
        w.common = "Spear thistle";
        w.latin = "Cirsium vulgare";
        w.region = "uk";
        w.leafDesc = "spiny deeply lobed grey-green thistle leaf";
        w.leaves = 10;
        w.leafLength = 0.3f;
        w.leafAspect = 0.25f;
        w.elevation = 0.5f;
        w.stemHeight = 0.9f;
        w.stems = 2;
        w.stemLeaves = 9;
        w.flowerRadius = 0.04f;
        w.petals = 28;
        w.flowerBall = true;
        w.leaf = rgb(85, 115, 70);
        w.flower = rgb(150, 70, 150);
        v.push_back(w);
    }
    {
        WeedSpecies w;
        w.id = "urtica_dioica";
        w.common = "Stinging nettle";
        w.latin = "Urtica dioica";
        w.region = "uk";
        w.leafDesc = "heart-shaped serrated nettle leaf with pointed tip";
        w.form = WeedForm::Stemmed;
        w.leaves = 14;
        w.leafLength = 0.09f;
        w.leafAspect = 0.7f;
        w.petiole = 0.3f;
        w.stemHeight = 1.0f;
        w.stems = 4;
        w.leaf = rgb(55, 105, 45);
        v.push_back(w);
    }
    {
        WeedSpecies w;
        w.id = "pteridium_aquilinum";
        w.common = "Bracken";
        w.latin = "Pteridium aquilinum";
        w.region = "uk";
        w.leafDesc = "triangular bracken frond with many pinnae";
        w.form = WeedForm::Frond;
        w.leaves = 8;
        w.leafLength = 0.9f;
        w.leafAspect = 0.9f;
        w.petiole = 0.45f;
        w.elevation = 1.15f;
        w.curl = 0.9f;
        w.leaf = rgb(70, 115, 45);
        w.stem = rgb(120, 130, 70);
        v.push_back(w);
    }
    {
        WeedSpecies w;
        w.id = "rubus_fruticosus";
        w.common = "Blackberry";
        w.latin = "Rubus fruticosus";
        w.region = "uk";
        w.leafDesc = "serrated ovate bramble leaflet";
        w.form = WeedForm::Cane;
        w.leaves = 10;
        w.leafLength = 0.08f;
        w.leafAspect = 0.65f;
        w.stemHeight = 1.5f;
        w.stems = 4;
        w.flowerRadius = 0.02f;
        w.petals = 5;
        w.leaf = rgb(55, 95, 40);
        w.stem = rgb(110, 60, 70);
        w.flower = rgb(250, 235, 235);
        v.push_back(w);
    }
    {
        WeedSpecies w;
        w.id = "echium_plantagineum";
        w.common = "Paterson's curse";
        w.latin = "Echium plantagineum";
        w.region = "au";
        w.leafDesc = "bristly lanceolate grey-green leaf";
        w.leaves = 12;
        w.leafLength = 0.22f;
        w.leafAspect = 0.18f;
        w.elevation = 0.45f;
        w.stemHeight = 0.7f;
        w.stems = 3;
        w.stemLeaves = 12;
        w.flowerRadius = 0.012f;
        w.petals = 5;
        w.flowerSpike = true;
        w.leaf = rgb(95, 120, 80);
        w.flower = rgb(110, 70, 170);
        v.push_back(w);
    }
    return v;
}

// Names the float members of TreeSpecies that --param can change.
inline const std::map<std::string, float TreeSpecies::*> &treeParameters() {
    static const std::map<std::string, float TreeSpecies::*> m = {
        {"height_min", &TreeSpecies::heightMin},
        {"height_max", &TreeSpecies::heightMax},
        {"trunk_radius_ratio", &TreeSpecies::trunkRadiusRatio},
        {"flare", &TreeSpecies::flare},
        {"leader_end", &TreeSpecies::leaderEnd},
        {"crown_base", &TreeSpecies::crownBase},
        {"crown_radius", &TreeSpecies::crownRadius},
        {"profile", &TreeSpecies::profileA},
        {"wobble", &TreeSpecies::wobble},
        {"lean", &TreeSpecies::lean},
        {"fill", &TreeSpecies::fill},
        {"apical", &TreeSpecies::apical},
        {"shed", &TreeSpecies::shed},
        {"asym", &TreeSpecies::asym},
        {"stem_lean", &TreeSpecies::stemLean},
        {"hedge_length", &TreeSpecies::hedgeLength},
        {"hedge_width", &TreeSpecies::hedgeWidth},
        {"leaf_length", &TreeSpecies::leafLength},
        {"leaf_aspect", &TreeSpecies::leafAspect},
        {"leaf_droop", &TreeSpecies::leafDroop},
        {"lai", &TreeSpecies::lai},
    };
    return m;
}

} // namespace gentree
