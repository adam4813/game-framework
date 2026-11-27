/// Unit tests for the upright-object geometry builder (ticket 03). Pure CPU geometry — no ECS, no
/// GPU. Given a WorldObject with a known footprint and height, BuildUprightObjectMesh must produce
/// upright quad vertices at exact, independently computed world coordinates.
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "farm/object_kind_registry.hpp"
#include "farm/upright_object_mesh_builder.hpp"
#include "farm/world_data.hpp"

using namespace game::farm;
using engine::core::Rect;

namespace {

// A resolver that never finds a sprite -> quads fall back to full-texture UVs (0..1).
const SpriteRectResolver kNoSprite = [](std::string_view) -> std::optional<Rect> { return std::nullopt; };

WorldObject MakeObject(const std::vector<glm::ivec2>& footprint, const std::string& kind, const float height) {
	WorldObject obj;
	obj.footprint = footprint;
	obj.kind = kind;
	obj.height = height;
	obj.blocksMovement = true;
	return obj;
}

} // namespace

// ── Single upright quad geometry ─────────────────────────────────────────────────

TEST(UprightMeshBuilderTest, SingleTileObjectProducesOneUprightQuad) {
	const auto obj = MakeObject({glm::ivec2{3, 4}}, "monument", 2.0F);
	const UprightMeshConfig cfg{.tileSize = 0.5F, .quadWidthTiles = 1.0F, .atlasWidthPx = 1.0F, .atlasHeightPx = 1.0F};

	const auto mesh = BuildUprightObjectMesh({obj}, ObjectKindRegistry{}, cfg, kNoSprite);

	ASSERT_EQ(mesh.vertices.size(), 4U);
	ASSERT_EQ(mesh.indices.size(), 6U);
	ASSERT_EQ(mesh.colors.size(), 4U);
	ASSERT_EQ(mesh.uvs.size(), 4U);

	// Ground centre of tile (3,4): ((3+0.5)*0.5, (4+0.5)*0.5) = (1.75, 2.25); half-width 0.25.
	constexpr float cx = 1.75F;
	constexpr float cz = 2.25F;
	constexpr float hw = 0.25F;
	constexpr float h = 2.0F * 0.5F; // height 2.0 tiles × tileSize 0.5 = 1.0 world units

	// Vertex order: BL, BR, TR, TL.
	EXPECT_EQ(mesh.vertices[0], (glm::vec3{cx - hw, 0.0F, cz})); // BL
	EXPECT_EQ(mesh.vertices[1], (glm::vec3{cx + hw, 0.0F, cz})); // BR
	EXPECT_EQ(mesh.vertices[2], (glm::vec3{cx + hw, h, cz}));    // TR
	EXPECT_EQ(mesh.vertices[3], (glm::vec3{cx - hw, h, cz}));    // TL

	// Two triangles with reversed winding (CCW from the oblique camera at Pz-10 looking in +Z).
	const std::vector<uint32_t> expectedIndices{0, 2, 1, 0, 3, 2};
	EXPECT_EQ(mesh.indices, expectedIndices);
}

TEST(UprightMeshBuilderTest, FrontFacePointsTowardPositiveZ) {
	const auto obj = MakeObject({glm::ivec2{0, 0}}, "monument", 1.5F);
	const UprightMeshConfig cfg{};
	const auto mesh = BuildUprightObjectMesh({obj}, ObjectKindRegistry{}, cfg, kNoSprite);

	ASSERT_EQ(mesh.vertices.size(), 4U);
	// Normal of the first triangle (v0,v1,v2) via the right-hand rule must point toward -Z so the
	// fixed oblique camera (sitting at Pz-10, looking in +Z) sees the front face in its CCW
	// convention and the quads are not back-face culled.
	const glm::vec3 e1 = mesh.vertices[mesh.indices[1]] - mesh.vertices[mesh.indices[0]];
	const glm::vec3 e2 = mesh.vertices[mesh.indices[2]] - mesh.vertices[mesh.indices[0]];
	const glm::vec3 normal = glm::cross(e1, e2);
	EXPECT_LT(normal.z, 0.0F);
	EXPECT_FLOAT_EQ(normal.x, 0.0F);
	EXPECT_FLOAT_EQ(normal.y, 0.0F);
}

TEST(UprightMeshBuilderTest, HeightComesFromObjectNotFootprint) {
	const auto obj = MakeObject({glm::ivec2{1, 1}}, "tree", 3.5F);
	const auto mesh = BuildUprightObjectMesh({obj}, ObjectKindRegistry{}, UprightMeshConfig{}, kNoSprite);

	ASSERT_EQ(mesh.vertices.size(), 4U);
	// Base vertices sit on the ground; top vertices rise to height × tileSize world units.
	// UprightMeshConfig{} has tileSize = 0.5F, so 3.5 tiles × 0.5 = 1.75 world units.
	EXPECT_FLOAT_EQ(mesh.vertices[0].y, 0.0F);
	EXPECT_FLOAT_EQ(mesh.vertices[1].y, 0.0F);
	EXPECT_FLOAT_EQ(mesh.vertices[2].y, 3.5F * 0.5F);
	EXPECT_FLOAT_EQ(mesh.vertices[3].y, 3.5F * 0.5F);
}

TEST(UprightMeshBuilderTest, ZeroOrNegativeHeightObjectIsSkipped) {
	const auto flush = MakeObject({glm::ivec2{2, 2}}, "flat", 0.0F);
	const auto negative = MakeObject({glm::ivec2{5, 5}}, "flat", -1.0F);
	const auto mesh = BuildUprightObjectMesh({flush, negative}, ObjectKindRegistry{}, UprightMeshConfig{}, kNoSprite);
	EXPECT_TRUE(mesh.vertices.empty());
	EXPECT_TRUE(mesh.indices.empty());
}

TEST(UprightMeshBuilderTest, QuadWidthTilesScalesHorizontalSpan) {
	const auto obj = MakeObject({glm::ivec2{0, 0}}, "wide", 1.0F);
	const UprightMeshConfig cfg{.tileSize = 0.5F, .quadWidthTiles = 2.0F};
	const auto mesh = BuildUprightObjectMesh({obj}, ObjectKindRegistry{}, cfg, kNoSprite);

	ASSERT_EQ(mesh.vertices.size(), 4U);
	// Tile (0,0) centre at 0.25; half-width = (2 * 0.5) / 2 = 0.5 -> spans [-0.25, 0.75].
	EXPECT_FLOAT_EQ(mesh.vertices[0].x, -0.25F);
	EXPECT_FLOAT_EQ(mesh.vertices[1].x, 0.75F);
}

// ── Multi-tile footprints and multiple objects ──────────────────────────────────

TEST(UprightMeshBuilderTest, MultiTileFootprintEmitsOneQuadPerTile) {
	const auto building = MakeObject({glm::ivec2{1, 1}, glm::ivec2{2, 1}}, "house", 2.0F);
	const UprightMeshConfig cfg{.tileSize = 0.5F};
	const auto mesh = BuildUprightObjectMesh({building}, ObjectKindRegistry{}, cfg, kNoSprite);

	ASSERT_EQ(mesh.vertices.size(), 8U);
	ASSERT_EQ(mesh.indices.size(), 12U);

	// First quad anchored to tile (1,1): centre X = (1+0.5)*0.5 = 0.75.
	EXPECT_FLOAT_EQ(mesh.vertices[0].x, 0.75F - 0.25F);
	EXPECT_FLOAT_EQ(mesh.vertices[0].z, 0.75F);
	// Second quad anchored to tile (2,1): centre X = (2+0.5)*0.5 = 1.25; indices offset by 4.
	EXPECT_FLOAT_EQ(mesh.vertices[4].x, 1.25F - 0.25F);
	EXPECT_FLOAT_EQ(mesh.vertices[4].z, 0.75F);

	const std::vector<uint32_t> expectedIndices{0, 2, 1, 0, 3, 2, 4, 6, 5, 4, 7, 6};
	EXPECT_EQ(mesh.indices, expectedIndices);
}

TEST(UprightMeshBuilderTest, MultipleObjectsConcatenateInFootprintOrder) {
	const auto a = MakeObject({glm::ivec2{0, 0}}, "a", 1.0F);
	const auto b = MakeObject({glm::ivec2{4, 4}}, "b", 1.0F);
	const auto mesh = BuildUprightObjectMesh({a, b}, ObjectKindRegistry{}, UprightMeshConfig{}, kNoSprite);

	ASSERT_EQ(mesh.vertices.size(), 8U);
	// Object a at tile (0,0): z = 0.25; object b at tile (4,4): z = (4+0.5)*0.5 = 2.25.
	EXPECT_FLOAT_EQ(mesh.vertices[0].z, 0.25F);
	EXPECT_FLOAT_EQ(mesh.vertices[4].z, 2.25F);
}

// ── Sprite / UV mapping ──────────────────────────────────────────────────────────

TEST(UprightMeshBuilderTest, ResolvedSpriteProducesInsetAtlasUVs) {
	const auto obj = MakeObject({glm::ivec2{0, 0}}, "monument", 1.0F);
	const UprightMeshConfig cfg{.tileSize = 0.5F, .atlasWidthPx = 128.0F, .atlasHeightPx = 256.0F};

	// Resolver returns a 64x64 atlas cell at pixel (64,128).
	const SpriteRectResolver resolver = [](std::string_view) -> std::optional<Rect> {
		return Rect{.x = 64.0F, .y = 128.0F, .w = 64.0F, .h = 64.0F};
	};
	const auto mesh = BuildUprightObjectMesh({obj}, ObjectKindRegistry{}, cfg, resolver);

	ASSERT_EQ(mesh.uvs.size(), 4U);
	// Half-pixel inset, computed independently from the cell rect and atlas dimensions.
	constexpr float u0 = 64.5F / 128.0F;
	constexpr float v0 = 128.5F / 256.0F;
	constexpr float u1 = 127.5F / 128.0F;
	constexpr float v1 = 191.5F / 256.0F;

	// The sprite's top (v0) maps to the quad top (TR/TL); its bottom (v1) to the quad base (BL/BR).
	EXPECT_FLOAT_EQ(mesh.uvs[0].x, u0); // BL
	EXPECT_FLOAT_EQ(mesh.uvs[0].y, v1);
	EXPECT_FLOAT_EQ(mesh.uvs[1].x, u1); // BR
	EXPECT_FLOAT_EQ(mesh.uvs[1].y, v1);
	EXPECT_FLOAT_EQ(mesh.uvs[2].x, u1); // TR
	EXPECT_FLOAT_EQ(mesh.uvs[2].y, v0);
	EXPECT_FLOAT_EQ(mesh.uvs[3].x, u0); // TL
	EXPECT_FLOAT_EQ(mesh.uvs[3].y, v0);
}

TEST(UprightMeshBuilderTest, ObjectKindRegistryMapsKindToSpriteName) {
	// The builder must resolve the sprite name through the kind's ObjectKindDef, so a designer can
	// point a kind at any atlas sprite via data (games/farm/assets/data/object_kinds.json).
	ObjectKindRegistry reg;
	reg.defs["tree_oak"] = ObjectKindDef{.kind = "tree_oak", .height = 2.0F, .sprite = "oak_canopy"};

	std::string seenSprite;
	const SpriteRectResolver spy = [&seenSprite](std::string_view sprite) -> std::optional<Rect> {
		seenSprite = std::string{sprite};
		return std::nullopt;
	};

	const auto obj = MakeObject({glm::ivec2{0, 0}}, "tree_oak", 2.0F);
	static_cast<void>(BuildUprightObjectMesh({obj}, reg, UprightMeshConfig{}, spy));
	EXPECT_EQ(seenSprite, "oak_canopy");
}

TEST(UprightMeshBuilderTest, UnknownKindFallsBackToKindAsSpriteName) {
	std::string seenSprite;
	const SpriteRectResolver spy = [&seenSprite](std::string_view sprite) -> std::optional<Rect> {
		seenSprite = std::string{sprite};
		return std::nullopt;
	};
	const auto obj = MakeObject({glm::ivec2{0, 0}}, "mystery", 1.0F);
	static_cast<void>(BuildUprightObjectMesh({obj}, ObjectKindRegistry{}, UprightMeshConfig{}, spy));
	EXPECT_EQ(seenSprite, "mystery");
}

TEST(UprightMeshBuilderTest, EmptyObjectCollectionProducesEmptyMesh) {
	const auto mesh = BuildUprightObjectMesh({}, ObjectKindRegistry{}, UprightMeshConfig{}, kNoSprite);
	EXPECT_TRUE(mesh.vertices.empty());
	EXPECT_TRUE(mesh.indices.empty());
	EXPECT_TRUE(mesh.uvs.empty());
	EXPECT_TRUE(mesh.colors.empty());
}

// ── Tile-unit scaling contract ────────────────────────────────────────────────────

// Documented contract: WorldObject::height is in tile units. The mesh builder converts to world
// units at render time by multiplying by UprightMeshConfig::tileSize. This test is a living
// specification: "a tree with height=3.0 tiles and tileSize=0.5 stands 1.5 world units tall".
TEST(UprightMeshBuilderTest, TileUnitScalingContract) {
	// height=3.0 tiles, tileSize=0.5 → vertex Y = 1.5 WU
	{
		const auto obj = MakeObject({glm::ivec2{0, 0}}, "tree", 3.0F);
		const UprightMeshConfig cfg{.tileSize = 0.5F};
		const auto mesh = BuildUprightObjectMesh({obj}, ObjectKindRegistry{}, cfg, kNoSprite);
		ASSERT_GE(mesh.vertices.size(), 4U);
		EXPECT_FLOAT_EQ(mesh.vertices[2].y, 1.5F) << "3.0 tiles × 0.5 tileSize = 1.5 WU";
		EXPECT_FLOAT_EQ(mesh.vertices[3].y, 1.5F);
	}
	// height=3.0 tiles, tileSize=1.0 → vertex Y = 3.0 WU
	{
		const auto obj = MakeObject({glm::ivec2{0, 0}}, "tree", 3.0F);
		const UprightMeshConfig cfg{.tileSize = 1.0F};
		const auto mesh = BuildUprightObjectMesh({obj}, ObjectKindRegistry{}, cfg, kNoSprite);
		ASSERT_GE(mesh.vertices.size(), 4U);
		EXPECT_FLOAT_EQ(mesh.vertices[2].y, 3.0F) << "3.0 tiles × 1.0 tileSize = 3.0 WU";
		EXPECT_FLOAT_EQ(mesh.vertices[3].y, 3.0F);
	}
}

// ── Opacity ──────────────────────────────────────────────────────────────────────

TEST(UprightMeshBuilderTest, QuadsAreOpaqueWhiteByDefault) {
	const auto mesh =
		BuildUprightObjectMesh({MakeObject({{0, 0}}, "tree_oak", 4.0F)}, ObjectKindRegistry{}, {}, kNoSprite);

	ASSERT_EQ(mesh.colors.size(), 4U);
	for (const glm::vec4& c : mesh.colors) EXPECT_EQ(c, (glm::vec4{1.0F, 1.0F, 1.0F, 1.0F}));
}

TEST(UprightMeshBuilderTest, ConfiguredAlphaIsBakedIntoEveryVertex) {
	const UprightMeshConfig cfg{.alpha = 0.35F};
	const auto mesh = BuildUprightObjectMesh(
		{MakeObject({{0, 0}, {1, 0}}, "house", 3.0F), MakeObject({{4, 4}}, "tree_oak", 1.0F)},
		ObjectKindRegistry{},
		cfg,
		kNoSprite
	);

	ASSERT_EQ(mesh.colors.size(), 12U); // three quads, short objects included
	for (const glm::vec4& c : mesh.colors) EXPECT_EQ(c, (glm::vec4{1.0F, 1.0F, 1.0F, 0.35F}));
}
