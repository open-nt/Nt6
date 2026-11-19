// MIT License - see LICENSE
//
// ntgentree is tested by running it: every species is grown into a temporary directory and read back with Nt glTF,
// and the GLB's own JSON is read for what Nt glTF does not expose (the wind attribute, sensor properties, extras).
#include <QtCore>
#include <QtGltf>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

QString g_tmp;

int run(const QStringList &args, QByteArray *out = nullptr, QByteArray *err = nullptr) {
    QProcess p;
    p.start(QString::fromUtf8(NT_GENTREE), args);
    if (!p.waitForFinished(120000))
        return -1;
    if (out)
        *out = p.readAllStandardOutput();
    if (err)
        *err = p.readAllStandardError();
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
}

QByteArray readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// The JSON chunk of a GLB.
QJsonObject glbJson(const QByteArray &glb) {
    if (glb.size() < 20 || !glb.startsWith("glTF"))
        return QJsonObject();
    quint32 length = 0;
    std::memcpy(&length, glb.constData() + 12, 4);
    return QJsonDocument::fromJson(glb.mid(20, static_cast<qsizetype>(length))).object();
}

QtGltf::Document load(const QString &path) {
    QtGltf::Document doc;
    doc.load(path);
    return doc;
}

QStringList names(const QtGltf::Document &doc, int node) {
    QStringList out;
    for (int c : doc.nodes().at(node).children)
        out << doc.nodes().at(c).name;
    return out;
}

const QtGltf::Mesh *meshByName(const QtGltf::Document &doc, const QString &name) {
    for (const QtGltf::Mesh &m : doc.meshes())
        if (m.name == name)
            return &m;
    return nullptr;
}

qsizetype triangles(const QtGltf::Mesh &m) {
    qsizetype n = 0;
    for (const QtGltf::Primitive &p : m.primitives)
        n += p.indices.size() / 3;
    return n;
}

// The extent of a mesh along x.
float widthOf(const QtGltf::Mesh &m) {
    float lo = 1e30f, hi = -1e30f;
    for (const QtGltf::Primitive &p : m.primitives) {
        lo = std::min(lo, p.boundsMin.x());
        hi = std::max(hi, p.boundsMax.x());
    }
    return hi - lo;
}

float topOf(const QtGltf::Mesh &m) {
    float hi = -1e30f;
    for (const QtGltf::Primitive &p : m.primitives)
        hi = std::max(hi, p.boundsMax.y());
    return hi;
}

bool allFinite(const QtGltf::Mesh &m) {
    for (const QtGltf::Primitive &p : m.primitives)
        for (const QVector3D &v : p.positions)
            if (!std::isfinite(v.x()) || !std::isfinite(v.y()) || !std::isfinite(v.z()))
                return false;
    return true;
}

QStringList speciesOf(const QString &kind) {
    QByteArray out;
    run({"--list"}, &out);
    QStringList ids;
    for (const QByteArray &line : out.split('\n')) {
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() >= 3 && parts.at(1) == kind.toUtf8())
            ids << QString::fromUtf8(parts.at(0));
    }
    return ids;
}

void testList() {
    QByteArray out;
    CHECK(run({"--version"}, &out) == 0);
    CHECK(out.startsWith("ntgentree"));
    const QStringList trees = speciesOf("tree"), grasses = speciesOf("grass"), weeds = speciesOf("weed");
    CHECK(speciesOf("shrub").size() >= 6);
    CHECK(speciesOf("hedge").size() >= 4);
    CHECK(trees.size() >= 8);
    CHECK(grasses.size() >= 5);
    CHECK(weeds.size() >= 5);
    // United Kingdom and Australian species.
    CHECK(trees.contains("quercus_robur"));
    CHECK(trees.contains("eucalyptus_camaldulensis"));
    CHECK(run({"--list"}, &out) == 0);
    CHECK(out.contains(" uk "));
    CHECK(out.contains(" au "));
    CHECK(run({"--list-params"}, &out) == 0);
    CHECK(out.contains("crown_radius"));
}

void testTrees() {
    // Shrubs are grown like trees, from several stems.
    for (const QString &id : speciesOf("tree") + speciesOf("shrub")) {
        const QString path = g_tmp + "/" + id + ".glb";
        QByteArray out;
        CHECK(run({id, "--seed", "5", "--max-leaves", "3000", "-o", path}, &out) == 0);
        const QtGltf::Document doc = load(path);
        CHECK(doc.isValid());
        if (!doc.isValid()) {
            std::printf("  %s: %s\n", qPrintable(id), qPrintable(doc.errorString()));
            continue;
        }
        const QStringList kids = names(doc, doc.scenes().at(0).nodes.at(0));
        CHECK(kids.contains("LOD0") && kids.contains("LOD1") && kids.contains("LOD2") && kids.contains("LOD3"));
        CHECK(kids.contains("Felled") && kids.contains("Collision") && kids.contains("Trunk") &&
              kids.contains("Crown"));
        const QtGltf::Mesh *lod0 = meshByName(doc, "LOD0"), *lod1 = meshByName(doc, "LOD1");
        const QtGltf::Mesh *lod2 = meshByName(doc, "LOD2"), *lod3 = meshByName(doc, "LOD3");
        CHECK(lod0 && lod1 && lod2 && lod3);
        if (!(lod0 && lod1 && lod2 && lod3))
            continue;
        CHECK(triangles(*lod0) > triangles(*lod1));
        CHECK(triangles(*lod1) > triangles(*lod2));
        CHECK(triangles(*lod2) > triangles(*lod3));
        CHECK(triangles(*lod3) > 50);
        for (const QtGltf::Mesh &m : doc.meshes())
            CHECK(allFinite(m));

        // The tree stands on the ground and its height is the one in the extras.
        const QJsonObject nt = glbJson(readFile(path))
                                   .value("scenes")
                                   .toArray()
                                   .at(0)
                                   .toObject()
                                   .value("extras")
                                   .toObject()
                                   .value("nt")
                                   .toObject();
        CHECK(nt.value("kind").toString() == "tree" || nt.value("kind").toString() == "shrub");
        CHECK(nt.value("species").toString() == id);
        const double height = nt.value("height").toDouble();
        CHECK(height > 0.3);
        CHECK(topOf(*lod2) <= static_cast<float>(height) * 1.4f); // the leaves of a coarse level are large
        CHECK(topOf(*lod2) >= static_cast<float>(height) * 0.5f);
        CHECK(nt.value("lods").toArray().size() == 4);
        CHECK(nt.value("proxies").toObject().value("collision").toString() == "Collision");
    }
}

void testHedges() {
    const QStringList hedges = speciesOf("hedge");
    for (const QString &id : hedges) {
        const QString path = g_tmp + "/" + id + ".glb";
        CHECK(run({id, "--seed", "4", "--max-leaves", "15000", "-o", path}) == 0);
        const QtGltf::Document doc = load(path);
        CHECK(doc.isValid());
        if (!doc.isValid())
            continue;
        const QStringList kids = names(doc, doc.scenes().at(0).nodes.at(0));
        CHECK(kids == QStringList({"LOD0", "LOD1", "LOD2", "LOD3", "Collision", "Crown"}));
        const QJsonObject nt = glbJson(readFile(path))
                                   .value("scenes")
                                   .toArray()
                                   .at(0)
                                   .toObject()
                                   .value("extras")
                                   .toObject()
                                   .value("nt")
                                   .toObject();
        CHECK(nt.value("kind").toString() == "hedge");
        CHECK(nt.value("proxies").toObject().value("ballistic").toString() == "Collision");
        CHECK(!nt.value("variants").toObject().contains("felled"));
        const double height = nt.value("height").toDouble();
        const double length = nt.value("hedge").toObject().value("length").toDouble();
        const double width = nt.value("hedge").toObject().value("width").toDouble();
        const QtGltf::Mesh *lod0 = meshByName(doc, "LOD0");
        CHECK(lod0 != nullptr);
        if (!lod0)
            continue;
        // A clipped box: the branches stop at its faces, and only leaves reach a little beyond.
        CHECK(topOf(*lod0) <= static_cast<float>(height) * 1.1f);
        CHECK(widthOf(*lod0) <= static_cast<float>(length) + 0.6f);
        CHECK(widthOf(*lod0) >= static_cast<float>(length) * 0.8f);
        float lo = 1e30f, hi = -1e30f;
        for (const QtGltf::Primitive &p : lod0->primitives) {
            lo = std::min(lo, p.boundsMin.z());
            hi = std::max(hi, p.boundsMax.z());
        }
        CHECK(hi - lo <= static_cast<float>(width) + 0.5f);
        CHECK(hi - lo >= static_cast<float>(width) * 0.6f);
        CHECK(triangles(*meshByName(doc, "LOD3")) == 12);
        CHECK(triangles(*lod0) > triangles(*meshByName(doc, "LOD1")));
    }
    // The length is a parameter, and the number of stems follows it.
    const QString shorter = g_tmp + "/short.glb", longer = g_tmp + "/long.glb";
    CHECK(run({"crataegus_monogyna_hedge", "--max-leaves", "500", "--param", "hedge_length=2", "-o", shorter}) == 0);
    CHECK(run({"crataegus_monogyna_hedge", "--max-leaves", "500", "--param", "hedge_length=8", "-o", longer}) == 0);
    CHECK(widthOf(*meshByName(load(longer), "LOD0")) > 2.5f * widthOf(*meshByName(load(shorter), "LOD0")));
}

void testShrubs() {
    // A shrub has several stems: more of them make a broader base than one.
    const QString one = g_tmp + "/one.glb", many = g_tmp + "/many.glb";
    CHECK(run({"corylus_avellana", "--seed", "3", "--max-leaves", "300", "--param", "stems=1", "-o", one}) == 0);
    CHECK(run({"corylus_avellana", "--seed", "3", "--max-leaves", "300", "--param", "stems=9", "-o", many}) == 0);
    const QtGltf::Document d1 = load(one), d9 = load(many);
    CHECK(d1.isValid() && d9.isValid());
    CHECK(triangles(*meshByName(d9, "LOD0")) > triangles(*meshByName(d1, "LOD0")));
    // The collision proxy of a shrub is wider than any one stem.
    const QtGltf::Mesh *collision = meshByName(d9, "Collision");
    CHECK(collision && widthOf(*collision) > 0.5f);
}

void testAttributesAndSensors() {
    const QString path = g_tmp + "/oak.glb";
    CHECK(run({"quercus_robur", "--max-leaves", "800", "-o", path}) == 0);
    const QJsonObject json = glbJson(readFile(path));
    // The wind weights, the tint and the texture coordinates are on every primitive.
    bool allHaveWind = true;
    const QJsonArray meshes = json.value("meshes").toArray();
    for (const QJsonValue &mv : meshes)
        for (const QJsonValue &pv : mv.toObject().value("primitives").toArray()) {
            const QJsonObject attrs = pv.toObject().value("attributes").toObject();
            allHaveWind =
                allHaveWind && attrs.contains("_WIND") && attrs.contains("COLOR_0") && attrs.contains("TEXCOORD_0");
        }
    CHECK(allHaveWind);
    // Lidar and thermal properties on the materials.
    const QJsonArray materials = json.value("materials").toArray();
    CHECK(materials.size() >= 3);
    for (const QJsonValue &mv : materials) {
        const QJsonObject m = mv.toObject();
        const QString name = m.value("name").toString();
        if (name != "bark" && name != "leaf")
            continue;
        const QJsonObject sensor = m.value("extras").toObject().value("nt").toObject().value("sensor").toObject();
        CHECK(sensor.value("class").toString() == "vegetation.broadleaf");
        const double r905 = sensor.value("lidar").toObject().value("reflectance905").toDouble();
        const double r1550 = sensor.value("lidar").toObject().value("reflectance1550").toDouble();
        CHECK(r905 > 0.1 && r905 < 0.9);
        CHECK(r1550 < r905); // water absorbs at 1550 nm
        const double e = sensor.value("thermal").toObject().value("emissivityLwir").toDouble();
        CHECK(e > 0.9 && e <= 1.0);
    }
    // Leaves are double sided, bark is not.
    for (const QJsonValue &mv : materials) {
        const QJsonObject m = mv.toObject();
        if (m.value("name").toString() == "leaf")
            CHECK(m.value("doubleSided").toBool());
        if (m.value("name").toString() == "bark")
            CHECK(!m.value("doubleSided").toBool());
    }
}

void testDeterminism() {
    const QString a = g_tmp + "/a.glb", b = g_tmp + "/b.glb", c = g_tmp + "/c.glb";
    CHECK(run({"fagus_sylvatica", "--seed", "7", "--max-leaves", "500", "-o", a}) == 0);
    CHECK(run({"fagus_sylvatica", "--seed", "7", "--max-leaves", "500", "-o", b}) == 0);
    CHECK(run({"fagus_sylvatica", "--seed", "8", "--max-leaves", "500", "-o", c}) == 0);
    CHECK(!readFile(a).isEmpty());
    CHECK(readFile(a) == readFile(b));
    CHECK(readFile(a) != readFile(c));
}

void testOptions() {
    const QString full = g_tmp + "/full.glb", sparse = g_tmp + "/sparse.glb", bare = g_tmp + "/bare.glb";
    CHECK(run({"betula_pendula", "--seed", "2", "--height", "15", "--max-leaves", "100000", "-o", full}) == 0);
    CHECK(run({"betula_pendula", "--seed", "2", "--height", "15", "--max-leaves", "100000", "--density", "0.4", "-o",
               sparse}) == 0);
    CHECK(run({"betula_pendula", "--seed", "2", "--height", "15", "--leaf-off", "-o", bare}) == 0);
    const QtGltf::Document f = load(full), s = load(sparse), b = load(bare);
    CHECK(f.isValid() && s.isValid() && b.isValid());
    if (!(f.isValid() && s.isValid() && b.isValid()))
        return;
    const qsizetype tf = triangles(*meshByName(f, "LOD0")), ts = triangles(*meshByName(s, "LOD0")),
                    tb = triangles(*meshByName(b, "LOD0"));
    // The skeleton is the same, so the difference is the leaves, and the leaf area follows the density.
    CHECK(tb < ts);
    CHECK(ts < tf);
    const double leavesFull = static_cast<double>(tf - tb), leavesSparse = static_cast<double>(ts - tb);
    CHECK(leavesSparse / leavesFull > 0.3 && leavesSparse / leavesFull < 0.5);
    CHECK(topOf(*meshByName(f, "LOD2")) <= 15.0f * 1.25f);

    // The leaf budget caps the count and the leaves grow to keep the area.
    const QString capped = g_tmp + "/capped.glb";
    CHECK(run({"betula_pendula", "--seed", "2", "--height", "15", "--max-leaves", "2000", "-o", capped}) == 0);
    const QtGltf::Document cd = load(capped);
    CHECK(triangles(*meshByName(cd, "LOD0")) - tb < (tf - tb) / 4); // the leaves, without the wood

    // A narrower crown from a parameter.
    const QString wide = g_tmp + "/wide.glb", narrow = g_tmp + "/narrow.glb";
    CHECK(run({"quercus_robur", "--seed", "4", "--height", "18", "--max-leaves", "300", "-o", wide}) == 0);
    CHECK(run({"quercus_robur", "--seed", "4", "--height", "18", "--max-leaves", "300", "--param", "crown_radius=0.15",
               "-o", narrow}) == 0);
    CHECK(widthOf(*meshByName(load(wide), "LOD2")) > 1.5f * widthOf(*meshByName(load(narrow), "LOD2")));
}

void testErrors() {
    QByteArray err;
    CHECK(run({"not_a_species", "-o", g_tmp + "/x.glb"}, nullptr, &err) == 1);
    CHECK(err.contains("unknown species"));
    CHECK(run({"quercus_robur", "--param", "nonsense=1", "-o", g_tmp + "/x.glb"}, nullptr, &err) == 1);
    CHECK(err.contains("unknown parameter"));
    CHECK(run({"themeda_triandra", "--param", "lai=1", "-o", g_tmp + "/x.glb"}, nullptr, &err) == 1);
    CHECK(run({"quercus_robur", "--density", "0", "-o", g_tmp + "/x.glb"}, nullptr, &err) == 1);
    CHECK(run({"quercus_robur"}, nullptr, &err) == 2);
    CHECK(run({"quercus_robur", "--bogus", "-o", g_tmp + "/x.glb"}, nullptr, &err) == 1);
}

void testTextures() {
    const QString dir = g_tmp + "/textures";
    QDir().mkpath(dir);
    // A 1x1 PNG.
    const QByteArray png = QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==");
    CHECK(!png.isEmpty());
    for (const char *name : {"quercus_robur_bark.png", "quercus_robur_leaf.png", "leaf.png"}) {
        QFile f(dir + "/" + name);
        CHECK(f.open(QIODevice::WriteOnly));
        f.write(png);
    }
    const QString path = g_tmp + "/textured.glb";
    CHECK(run({"quercus_robur", "--max-leaves", "500", "--textures", dir, "-o", path}) == 0);
    const QtGltf::Document doc = load(path);
    CHECK(doc.isValid());
    CHECK(doc.images().size() == 2);
    CHECK(doc.textures().size() == 2);
    int cutouts = 0, textured = 0;
    for (const QtGltf::Material &m : doc.materials()) {
        if (m.baseColorTexture.isValid())
            ++textured;
        if (m.alphaMode == QtGltf::Material::Mask)
            ++cutouts;
    }
    CHECK(textured == 2);
    CHECK(cutouts == 1); // only the leaves are cut out
    CHECK(!doc.images().isEmpty() && doc.images().at(0).toImage().width() == 1);

    // A weed picks up the generic leaf.png.
    const QString weed = g_tmp + "/weed_textured.glb";
    CHECK(run({"taraxacum_officinale", "--textures", dir, "-o", weed}) == 0);
    CHECK(load(weed).images().size() == 1);

    // Something that is not an image is refused.
    QFile bad(dir + "/fagus_sylvatica_bark.png");
    CHECK(bad.open(QIODevice::WriteOnly));
    bad.write("not a png");
    bad.close();
    QByteArray err;
    CHECK(run({"fagus_sylvatica", "--textures", dir, "-o", g_tmp + "/y.glb"}, nullptr, &err) == 1);
    CHECK(err.contains("neither a PNG nor a JPEG"));
}

void testGrassAndWeeds() {
    for (const QString &kind : {QString("grass"), QString("weed")}) {
        const QStringList ids = speciesOf(kind);
        for (const QString &id : ids) {
            const QString path = g_tmp + "/" + id + ".glb";
            CHECK(run({id, "--seed", "3", "-o", path}) == 0);
            const QtGltf::Document doc = load(path);
            CHECK(doc.isValid());
            if (!doc.isValid())
                continue;
            const QStringList kids = names(doc, doc.scenes().at(0).nodes.at(0));
            CHECK(kids == QStringList({"LOD0", "LOD1", "LOD2"}));
            const QtGltf::Mesh *l0 = meshByName(doc, "LOD0"), *l1 = meshByName(doc, "LOD1"),
                               *l2 = meshByName(doc, "LOD2");
            CHECK(l0 && l1 && l2);
            if (!(l0 && l1 && l2))
                continue;
            CHECK(triangles(*l0) > 100);
            CHECK(triangles(*l0) > triangles(*l1));
            CHECK(triangles(*l1) > triangles(*l2));
            for (const QtGltf::Mesh &m : doc.meshes())
                CHECK(allFinite(m));
            const QJsonObject nt = glbJson(readFile(path))
                                       .value("scenes")
                                       .toArray()
                                       .at(0)
                                       .toObject()
                                       .value("extras")
                                       .toObject()
                                       .value("nt")
                                       .toObject();
            CHECK(nt.value("kind").toString() == kind);
            CHECK(nt.value("sensorMaterial").toString() == (kind == "grass" ? "vegetation.grass" : "vegetation.forb"));
            // Plants stay close to the ground.
            CHECK(topOf(*l0) < 3.0f);
        }
    }
    // More blades, more triangles.
    CHECK(run({"poa_labillardierei", "--blades", "100", "-o", g_tmp + "/few.glb"}) == 0);
    CHECK(run({"poa_labillardierei", "--blades", "400", "-o", g_tmp + "/many.glb"}) == 0);
    CHECK(triangles(*meshByName(load(g_tmp + "/many.glb"), "LOD0")) >
          2 * triangles(*meshByName(load(g_tmp + "/few.glb"), "LOD0")));
}

// The highest point of the leaves of a plant, which is where the leaf material is used.
float leafTop(const QtGltf::Document &doc, const QtGltf::Mesh &m) {
    float top = 0;
    for (const QtGltf::Primitive &p : m.primitives)
        if (p.material >= 0 && doc.materials().at(p.material).name == "leaf")
            top = std::max(top, p.boundsMax.y());
    return top;
}

void testStemLeaves() {
    // Dock, thistle and Paterson's curse have leaves up the flowering stem; a dandelion has none, only a rosette.
    for (const QString &id :
         {QString("rumex_obtusifolius"), QString("cirsium_vulgare"), QString("echium_plantagineum")}) {
        const QString path = g_tmp + "/" + id + "_stem.glb";
        CHECK(run({id, "--seed", "3", "-o", path}) == 0);
        const QtGltf::Document doc = load(path);
        const QJsonObject nt = glbJson(readFile(path))
                                   .value("scenes")
                                   .toArray()
                                   .at(0)
                                   .toObject()
                                   .value("extras")
                                   .toObject()
                                   .value("nt")
                                   .toObject();
        const float height = static_cast<float>(nt.value("height").toDouble());
        CHECK(leafTop(doc, *meshByName(doc, "LOD0")) > 0.3f * height);
        CHECK(leafTop(doc, *meshByName(doc, "LOD2")) > 0.2f * height);
    }
    const QString path = g_tmp + "/dandelion_stem.glb";
    CHECK(run({"taraxacum_officinale", "--seed", "3", "-o", path}) == 0);
    const QtGltf::Document doc = load(path);
    CHECK(leafTop(doc, *meshByName(doc, "LOD0")) < 0.3f);
}

float trunkTop(const QString &path) {
    const QtGltf::Document doc = load(path);
    const QtGltf::Mesh *trunk = meshByName(doc, "Trunk");
    return trunk ? topOf(*trunk) : -1.0f;
}

void testCompetition() {
    // The trees compete for space (Palubicki et al. 2009). Where the vigor of a fork goes decides the form: with
    // apical control below one half the main axis loses to its side branches, and the trunk ends in the crown as
    // in an oak; above it the trunk carries on to the top.
    for (int seed = 4; seed <= 6; ++seed) {
        const QString spreading = g_tmp + "/spreading.glb", leader = g_tmp + "/leader.glb";
        const QString s = QString::number(seed);
        CHECK(run({"quercus_robur", "--seed", s, "--height", "22", "--max-leaves", "300", "--param", "apical=0.2", "-o",
                   spreading}) == 0);
        CHECK(run({"quercus_robur", "--seed", s, "--height", "22", "--max-leaves", "300", "--param", "apical=0.85",
                   "-o", leader}) == 0);
        CHECK(trunkTop(spreading) < 0.55f * 22.0f);
        CHECK(trunkTop(leader) > 0.7f * 22.0f);
    }
    // Shedding: side branches that find no space die, so a tree that sheds quickly has fewer branches.
    const QString keeps = g_tmp + "/keeps.glb", sheds = g_tmp + "/sheds.glb";
    CHECK(run({"fagus_sylvatica", "--seed", "3", "--max-leaves", "300", "--param", "shed=60", "-o", keeps}) == 0);
    CHECK(run({"fagus_sylvatica", "--seed", "3", "--max-leaves", "300", "--param", "shed=2", "-o", sheds}) == 0);
    auto branches = [](const QString &path) {
        return glbJson(readFile(path))
            .value("scenes")
            .toArray()
            .at(0)
            .toObject()
            .value("extras")
            .toObject()
            .value("nt")
            .toObject()
            .value("branches")
            .toInt();
    };
    CHECK(branches(sheds) < branches(keeps));
    // The older model, with twigs filling the crown by space colonization or drawn at random, is still there.
    const QString comp = g_tmp + "/comp.glb", fill = g_tmp + "/fill.glb", random = g_tmp + "/random.glb";
    CHECK(run({"quercus_robur", "--seed", "3", "--max-leaves", "300", "-o", comp}) == 0);
    CHECK(run({"quercus_robur", "--seed", "3", "--max-leaves", "300", "--param", "competition=0", "-o", fill}) == 0);
    CHECK(run({"quercus_robur", "--seed", "3", "--max-leaves", "300", "--param", "competition=0", "--param", "fill=0",
               "-o", random}) == 0);
    CHECK(load(comp).isValid() && load(fill).isValid() && load(random).isValid());
    CHECK(readFile(comp) != readFile(fill));
    CHECK(readFile(fill) != readFile(random));
}

void testAssetEntry() {
    const QString asset = g_tmp + "/asset.json";
    CHECK(run({"pinus_sylvestris", "--max-leaves", "300", "--asset", asset, "--asset-path",
               "assets/vegetation/pine.glb", "-o", g_tmp + "/pine.glb"}) == 0);
    const QJsonObject root = QJsonDocument::fromJson(readFile(asset)).object();
    const QJsonObject entry = root.value("tree.pinus_sylvestris").toObject();
    CHECK(entry.value("gltf").toString() == "assets/vegetation/pine.glb");
    CHECK(entry.value("kind").toString() == "tree");
    CHECK(entry.value("sensorMaterial").toString() == "vegetation.conifer");
    CHECK(entry.value("variants").toObject().value("felled").toString() == "Felled");
    CHECK(entry.value("proxies").toObject().contains("ballistic"));
}

void testTexturePlan() {
    QByteArray out;
    CHECK(run({"--textures-plan"}, &out) == 0);
    const QJsonArray plan = QJsonDocument::fromJson(out).array();
    const int expected =
        static_cast<int>(2 * (speciesOf("tree").size() + speciesOf("shrub").size() + speciesOf("hedge").size()) +
                         speciesOf("grass").size() + speciesOf("weed").size());
    CHECK(plan.size() == expected);
    bool barkOpaque = true, leafAlpha = true, prompts = true;
    for (const QJsonValue &v : plan) {
        const QJsonObject o = v.toObject();
        prompts = prompts && o.value("prompt").toString().size() > 40 && !o.value("id").toString().isEmpty();
        if (o.value("slot").toString() == "bark")
            barkOpaque = barkOpaque && !o.value("alpha").toBool();
        else
            leafAlpha = leafAlpha && o.value("alpha").toBool();
    }
    CHECK(barkOpaque && leafAlpha && prompts);
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const char *tmp = std::getenv("TMPDIR");
    const QString base = tmp && *tmp ? QDir::cleanPath(QString::fromUtf8(tmp)) : QString::fromUtf8("/tmp");
    g_tmp = base + "/nt_gentree_test_" + QString::number(static_cast<qlonglong>(::getpid()));
    if (!QDir().mkpath(g_tmp)) {
        std::printf("cannot create %s\n", qPrintable(g_tmp));
        return 1;
    }
    testList();
    testTrees();
    testHedges();
    testShrubs();
    testAttributesAndSensors();
    testDeterminism();
    testOptions();
    testErrors();
    testTextures();
    testGrassAndWeeds();
    testStemLeaves();
    testCompetition();
    testAssetEntry();
    testTexturePlan();
    QDir(g_tmp).removeRecursively();
    if (g_failures == 0)
        std::printf("All gentree tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
