#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/render/camera_projection.hpp"

using engine::render::BuildProjection;
using engine::render::Camera;
using engine::render::Projection;

namespace {

void ExpectMatNear(const glm::mat4& actual, const glm::mat4& expected, const float tol = 1e-5F) {
	for (int col = 0; col < 4; ++col) {
		for (int row = 0; row < 4; ++row) {
			EXPECT_NEAR(actual[col][row], expected[col][row], tol) << "mismatch at [" << col << "][" << row << "]";
		}
	}
}

} // namespace

// An orthographic camera's projection matrix equals glm::ortho built from the view size: the
// vertical extent is `ortho_size`, the horizontal extent scales with aspect, and near/far carry
// through. Expected extents are derived by hand (independent of the implementation's arithmetic).
TEST(CameraProjectionTest, orthographic_matches_glm_ortho_with_derived_extents) {
	Camera cam{};
	cam.projection = Projection::Orthographic;
	cam.ortho_size = 20.0F;
	cam.near_plane = 0.5F;
	cam.far_plane = 200.0F;

	constexpr float aspect = 1.6F;
	constexpr float halfHeight = 10.0F;            // ortho_size / 2
	constexpr float halfWidth = halfHeight * 1.6F; // halfHeight * aspect = 16

	const glm::mat4 expected = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.5F, 200.0F);
	ExpectMatNear(BuildProjection(cam, aspect), expected);
}

// The x/y scale entries equal 1/halfWidth and 1/halfHeight for a symmetric ortho frustum. These
// are depth-convention independent, so they verify the extent derivation without relying on
// glm::ortho's internal depth mapping.
TEST(CameraProjectionTest, orthographic_scale_entries_are_reciprocal_half_extents) {
	Camera cam{};
	cam.projection = Projection::Orthographic;
	cam.ortho_size = 20.0F;

	constexpr float aspect = 1.6F;
	const glm::mat4 proj = BuildProjection(cam, aspect);

	EXPECT_NEAR(proj[0][0], 1.0F / 16.0F, 1e-6F); // 1 / halfWidth
	EXPECT_NEAR(proj[1][1], 1.0F / 10.0F, 1e-6F); // 1 / halfHeight
	// Symmetric frustum: no lateral/vertical translation in the projection.
	EXPECT_NEAR(proj[3][0], 0.0F, 1e-6F);
	EXPECT_NEAR(proj[3][1], 0.0F, 1e-6F);
}

// Horizontal extent tracks the supplied aspect ratio independently of the vertical view size.
TEST(CameraProjectionTest, orthographic_width_tracks_aspect_ratio) {
	Camera cam{};
	cam.projection = Projection::Orthographic;
	cam.ortho_size = 20.0F;

	const glm::mat4 wide = BuildProjection(cam, 2.0F);
	const glm::mat4 narrow = BuildProjection(cam, 1.0F);

	EXPECT_NEAR(wide[0][0], 1.0F / 20.0F, 1e-6F);   // halfWidth = 10 * 2
	EXPECT_NEAR(narrow[0][0], 1.0F / 10.0F, 1e-6F); // halfWidth = 10 * 1
	// Vertical extent is unaffected by aspect.
	EXPECT_NEAR(wide[1][1], 1.0F / 10.0F, 1e-6F);
	EXPECT_NEAR(narrow[1][1], 1.0F / 10.0F, 1e-6F);
}

// A perspective camera (the default) uses glm::perspective, unchanged by this ticket.
TEST(CameraProjectionTest, perspective_matches_glm_perspective) {
	Camera cam{};
	cam.projection = Projection::Perspective;
	cam.fov = 60.0F;
	cam.near_plane = 0.1F;
	cam.far_plane = 100.0F;

	constexpr float aspect = 1.7777778F;
	const glm::mat4 expected = glm::perspective(glm::radians(60.0F), aspect, 0.1F, 100.0F);
	ExpectMatNear(BuildProjection(cam, aspect), expected);
}

// A default-constructed Camera is perspective, so BuildProjection must not accidentally select
// the orthographic branch for cameras that never opt in.
TEST(CameraProjectionTest, default_camera_is_perspective) {
	const Camera cam{};
	EXPECT_EQ(cam.projection, Projection::Perspective);

	constexpr float aspect = 1.7777778F;
	const glm::mat4 expected = glm::perspective(glm::radians(cam.fov), aspect, cam.near_plane, cam.far_plane);
	ExpectMatNear(BuildProjection(cam, aspect), expected);
}
