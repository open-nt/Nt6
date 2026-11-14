// MIT License - see LICENSE
//
// Overview page about Nt glTF for the NDoc documentation. This file holds only a documentation
// comment and is not part of the library.

/*!
    \page nt-gltf.html
    \title Using Nt glTF
    \brief How to load glTF 2.0 scenes and hand their meshes, materials and instances to a renderer.

    Nt glTF reads glTF 2.0, the Khronos format for 3D scenes, from \c .gltf files (JSON with
    external or embedded buffers) and \c .glb files (one binary container). It decodes the geometry
    into float arrays that copy straight into vertex and index buffers, so a Vulkan renderer or ray
    tracer needs no glTF code of its own. Qt has no such module; Qt Quick 3D loads glTF only into its
    own scene graph. The module needs Nt Core and Nt GUI:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(viewer main.cpp)
    target_link_libraries(viewer PRIVATE Nt::Gltf)
    \endcode

    \section1 Loading a Scene

    QtGltf::Document::load() reads a file and the buffers it names. The lists of the document then
    follow the glTF specification, with the same indices: a node refers to a mesh by its index in
    meshes(), a primitive to a material by its index in materials(), and so on.

    \snippet gltf/gltfinfo.cpp load

    Loading checks every index, byte range and value, and either succeeds completely or leaves the
    document empty, with errorString() naming the object at fault (\c{"node 3: mesh is not a valid
    index"}). Code that uses a loaded document needs no checks of its own.

    \section1 Geometry

    Each QtGltf::Primitive holds positions, and optionally normals, tangents, two sets of texture
    coordinates and vertex colours, all with one element per vertex. Whatever the file stores,
    whether floats, normalized bytes or shorts from \c KHR_mesh_quantization, or sparse accessors,
    the lists hold floats. Indices are always present: they are generated for geometry without
    them, and triangle strips, triangle fans and line loops are converted to plain lists, so a
    primitive is just points, lines or triangles. Triangles are counter-clockwise when seen from
    the front.

    \section1 Scenes and Instances

    A scene is a tree of nodes, each with a transform relative to its parent. instances() flattens
    a scene into each node and its transform to world space. A ray tracer builds one bottom-level
    acceleration structure per mesh and adds one top-level instance for each node that draws a
    mesh:

    \snippet gltf/gltfinfo.cpp instances

    worldTransform() gives the same matrix for a single node, and parentNode() walks up the tree.
    Cameras and lights are placed by their nodes the same way; both look down the node's negative
    Z axis.

    \section1 Materials and Textures

    QtGltf::Material holds the metallic-roughness parameters of glTF: base colour, metalness,
    roughness, normal, occlusion and emission, each a factor and an optional texture.
    \c KHR_materials_emissive_strength and \c KHR_materials_unlit are read as well, and for glass
    \c KHR_materials_transmission, \c KHR_materials_ior and \c KHR_materials_volume. Textures pair
    an image with a sampler. An image stored in the file keeps its encoded bytes; an external image
    keeps only its path and is read when QtGltf::Image::encodedData() or
    QtGltf::Image::toImage() asks for it, so a scene with gigabytes of textures loads quickly.

    \section1 Test Scenes

    The Khronos glTF Sample Assets repository has small scenes for testing: \e DamagedHelmet (one
    mesh with every PBR texture) and \e Sponza (an atrium with many materials). NVIDIA's Open
    Research Content Archive has \e{Amazon Lumberyard Bistro}, a scene of about three million
    triangles that is commonly used to test ray tracing and upscaling. Check the license of each
    model before you ship it. \c{examples/gltf/gltfinfo.cpp} prints what a file holds:

    \code
    gltfinfo DamagedHelmet.glb
    gltfinfo Sponza/glTF/Sponza.gltf --instances
    \endcode

    \section1 Limits

    Animations, skins and morph targets are not read, and \c KHR_draco_mesh_compression,
    \c EXT_meshopt_compression and \c KHR_texture_transform are not supported. A file that lists an
    unsupported extension in \c extensionsRequired fails to load with
    QtGltf::Document::ExtensionError; one that only uses it loads without it.

    \sa {Nt glTF C++ Classes}, {Using Nt Vulkan}
*/
