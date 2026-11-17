#include "jolt_backend.hpp"

#include <algorithm>
#include <cstdarg>
#include <ranges>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>

#include "engine/physics/gltf_geometry.hpp"

// Jolt Physics includes. Order matters!
// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyLock.h>
// clang-format on

#include "jolt_debug_renderer.hpp"

namespace engine::physics {

// A Jolt character plus what the backend needs to place it. Not a body: the physics system never sees it.
struct CharacterRecord {
	JPH::Ref<JPH::CharacterVirtual> character;
	CharacterBody settings;
	glm::vec3 shape_offset{0.0F}; // capsule center relative to the entity origin, in the up-aligned frame
	glm::vec3 written_position{0.0F};
};

struct MeshShapeCache {
	std::unordered_map<std::string, JPH::ShapeRefC> shapes;
};

// Jolt callbacks
static void JoltTraceImpl(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	spdlog::trace("[Jolt] {}", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
static bool JoltAssertFailedImpl(const char* expr, const char* msg, const char* file, uint32_t line) {
	spdlog::error("[Jolt] Assert: {} at {}:{}: {}", expr, file, line, msg ? msg : "");
	return true;
}
#endif

namespace {
// Jolt's allocator hooks, Factory, type registration and default material are process-global, shared by every backend
// (one per physics world). They're set up once, by the first Initialize, and torn down at process exit, never when a
// world shuts down, so worlds come and go in any order and a later world reuses the registration. That's the usage
// Jolt expects (register once per process), and registering again after UnregisterTypes crashes on WASM.
class JoltGlobals {
public:
	JoltGlobals() {
		JPH::RegisterDefaultAllocator();
		JPH::Trace = JoltTraceImpl;
#ifdef JPH_ENABLE_ASSERTS
		JPH::AssertFailed = JoltAssertFailedImpl;
#endif
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();
	}

	// Runs during static destruction. Jolt's own statics that RegisterTypes creates finish constructing before this
	// object does, so they're destroyed after it. On WASM the page closes without running it.
	~JoltGlobals() {
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	JoltGlobals(const JoltGlobals&) = delete;
	JoltGlobals& operator=(const JoltGlobals&) = delete;
	JoltGlobals(JoltGlobals&&) = delete;
	JoltGlobals& operator=(JoltGlobals&&) = delete;
};

// Sets up the Jolt globals on the first call; later calls do nothing. A function-local static rather than a world
// singleton because it outlives every world.
void EnsureJoltGlobals() { static const JoltGlobals globals; }
} // namespace

// Layer configuration. A Jolt ObjectLayer encodes (CollisionLayer, moving bit) as layer * 2 + moving, so each
// CollisionLayer has a non-moving and a moving object layer on the two broad-phase layers. Contacts look only at the
// moving bit (non-moving never meets non-moving); queries filter on the CollisionLayer.
namespace {
// A query ray shorter than this has no direction and hits nothing.
constexpr float kMinRayDirectionLength = 1e-6F;

constexpr JPH::BroadPhaseLayer BP_LAYER_NON_MOVING(0);
constexpr JPH::BroadPhaseLayer BP_LAYER_MOVING(1);

constexpr JPH::ObjectLayer ToObjectLayer(const CollisionLayer layer, const bool moving) {
	return static_cast<JPH::ObjectLayer>(static_cast<uint32_t>(layer) * 2U + (moving ? 1U : 0U));
}
constexpr bool IsMoving(const JPH::ObjectLayer layer) { return (layer & 1U) != 0U; }
constexpr uint32_t CollisionLayerIndex(const JPH::ObjectLayer layer) { return static_cast<uint32_t>(layer) >> 1U; }

class BroadPhaseLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface {
public:
	[[nodiscard]] uint32_t GetNumBroadPhaseLayers() const override { return 2; }

	[[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(const JPH::ObjectLayer layer) const override {
		return IsMoving(layer) ? BP_LAYER_MOVING : BP_LAYER_NON_MOVING;
	}

#ifdef JPH_EXTERNAL_PROFILE
	[[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
		return static_cast<JPH::BroadPhaseLayer::Type>(layer) == 0 ? "NON_MOVING" : "MOVING";
	}
#endif
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
	[[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer layer, const JPH::BroadPhaseLayer bp_layer) const override {
		// Non-moving only collides with moving.
		return IsMoving(layer) || bp_layer == BP_LAYER_MOVING;
	}
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
	[[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer layer1, const JPH::ObjectLayer layer2) const override {
		return IsMoving(layer1) || IsMoving(layer2);
	}
};

// Query filters for RaycastFilter: a layer mask on the CollisionLayer, and entities to pass through.
class LayerMaskFilter final : public JPH::ObjectLayerFilter {
public:
	explicit LayerMaskFilter(const uint32_t mask) : mask_(mask) {}
	[[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer layer) const override {
		return ((mask_ >> CollisionLayerIndex(layer)) & 1U) != 0U;
	}

private:
	uint32_t mask_;
};

class IgnoreEntitiesFilter final : public JPH::BodyFilter {
public:
	explicit IgnoreEntitiesFilter(const std::span<const flecs::entity_t> ignore) : ignore_(ignore) {}
	[[nodiscard]] bool ShouldCollideLocked(const JPH::Body& body) const override {
		return std::ranges::find(ignore_, body.GetUserData()) == ignore_.end();
	}

private:
	std::span<const flecs::entity_t> ignore_;
};

// Sets a flag for a scope, so queries made while the world steps can refuse.
class ScopedFlag {
public:
	explicit ScopedFlag(bool& flag) : flag_(flag) { flag_ = true; }
	~ScopedFlag() { flag_ = false; }
	ScopedFlag(const ScopedFlag&) = delete;
	ScopedFlag& operator=(const ScopedFlag&) = delete;
	ScopedFlag(ScopedFlag&&) = delete;
	ScopedFlag& operator=(ScopedFlag&&) = delete;

private:
	bool& flag_;
};

class ContactListenerImpl : public JPH::ContactListener {
public:
	[[nodiscard]] std::vector<CollisionInfo> TakeEvents() { return std::exchange(events_, {}); }

	JPH::ValidateResult
	OnContactValidate(const JPH::Body&, const JPH::Body&, JPH::RVec3Arg, const JPH::CollideShapeResult&) override {
		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	void OnContactAdded(
		const JPH::Body& body1,
		const JPH::Body& body2,
		const JPH::ContactManifold& manifold,
		JPH::ContactSettings&
	) override {
		CollisionInfo info;
		info.entity_a = body1.GetUserData();
		info.entity_b = body2.GetUserData();

		for (uint32_t i = 0; i < manifold.mRelativeContactPointsOn1.size(); ++i) {
			ContactPoint point;
			JPH::Vec3 world_pt = manifold.GetWorldSpaceContactPointOn1(i);
			point.position = {world_pt.GetX(), world_pt.GetY(), world_pt.GetZ()};
			point.normal = {
				manifold.mWorldSpaceNormal.GetX(),
				manifold.mWorldSpaceNormal.GetY(),
				manifold.mWorldSpaceNormal.GetZ()
			};
			point.penetration_depth = manifold.mPenetrationDepth;
			info.contacts.push_back(point);
		}
		events_.push_back(info);
	}

private:
	std::vector<CollisionInfo> events_;
};
} // namespace

JoltBackend::JoltBackend(const PhysicsConfig& config) :
		config_(config), mesh_shape_cache_(std::make_unique<MeshShapeCache>()) {}

JoltBackend::~JoltBackend() { Shutdown(); }

bool JoltBackend::Initialize() {
	if (initialized_) {
		spdlog::warn("[Physics] Already initialized");
		return true;
	}

	EnsureJoltGlobals();

	temp_allocator_ = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

	auto num_threads = std::max(1U, std::thread::hardware_concurrency() - 1);
	job_system_ = std::make_unique<JPH::JobSystemThreadPool>(
		JPH::cMaxPhysicsJobs,
		JPH::cMaxPhysicsBarriers,
		static_cast<int>(num_threads)
	);

	broad_phase_layer_interface_ = std::make_unique<BroadPhaseLayerInterfaceImpl>();
	object_vs_broad_phase_layer_filter_ = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
	object_layer_pair_filter_ = std::make_unique<ObjectLayerPairFilterImpl>();

	physics_system_ = std::make_unique<JPH::PhysicsSystem>();
	physics_system_->Init(
		65536,
		0,
		65536,
		10240,
		*broad_phase_layer_interface_,
		*object_vs_broad_phase_layer_filter_,
		*object_layer_pair_filter_
	);

	physics_system_->SetGravity({config_.gravity.x, config_.gravity.y, config_.gravity.z});

	contact_listener_ = std::make_unique<ContactListenerImpl>();
	physics_system_->SetContactListener(contact_listener_.get());

	spdlog::info("[Physics] Jolt initialized with {} threads", num_threads);
	initialized_ = true;
	return true;
}

void JoltBackend::Shutdown() {
	if (!initialized_) return;

	auto& body_interface = physics_system_->GetBodyInterface();
	for (const auto& record : entity_to_body_ | std::views::values) {
		JPH::BodyID bid(record.id);
		body_interface.RemoveBody(bid);
		body_interface.DestroyBody(bid);
	}
	entity_to_body_.clear();
	characters_.clear();
	mesh_shape_cache_->shapes.clear();

	physics_system_.reset();
	contact_listener_.reset();
	object_layer_pair_filter_.reset();
	object_vs_broad_phase_layer_filter_.reset();
	broad_phase_layer_interface_.reset();
	job_system_.reset();
	temp_allocator_.reset();

	spdlog::info("[Physics] Jolt shutdown");
	initialized_ = false;
}

void JoltBackend::SetGravity(const glm::vec3& gravity) const {
	if (physics_system_) {
		physics_system_->SetGravity({gravity.x, gravity.y, gravity.z});
	}
}

glm::vec3 JoltBackend::GetGravity() const {
	if (physics_system_) {
		const auto g = physics_system_->GetGravity();
		return {g.GetX(), g.GetY(), g.GetZ()};
	}
	return config_.gravity;
}

void JoltBackend::StepSimulation(const float delta_time) const {
	if (!initialized_ || !physics_system_) return;

	const ScopedFlag stepping(stepping_);
	physics_system_->Update(delta_time, config_.collision_steps, temp_allocator_.get(), job_system_.get());
}

static JPH::EMotionType ToJoltMotionType(const MotionType type) {
	switch (type) {
	case MotionType::Static: return JPH::EMotionType::Static;
	case MotionType::Kinematic: return JPH::EMotionType::Kinematic;
	case MotionType::Dynamic: return JPH::EMotionType::Dynamic;
	}
	return JPH::EMotionType::Dynamic;
}

static JPH::ShapeRefC CreateShape(const ShapeConfig& cfg) {
	switch (cfg.type) {
	case ShapeType::Box:
	{
		JPH::BoxShapeSettings settings({cfg.box_half_extents.x, cfg.box_half_extents.y, cfg.box_half_extents.z});
		return settings.Create().Get();
	}
	case ShapeType::Sphere:
	{
		JPH::SphereShapeSettings settings(cfg.sphere_radius);
		return settings.Create().Get();
	}
	case ShapeType::Capsule:
	{
		JPH::CapsuleShapeSettings settings(cfg.capsule_height * 0.5F, cfg.capsule_radius);
		return settings.Create().Get();
	}
	case ShapeType::Cylinder:
	{
		JPH::CylinderShapeSettings settings(cfg.cylinder_height * 0.5F, cfg.cylinder_radius);
		return settings.Create().Get();
	}
	case ShapeType::ConvexHull:
	{
		if (!cfg.vertices.empty()) {
			std::vector<JPH::Vec3> verts;
			for (const auto& v : cfg.vertices) {
				verts.emplace_back(v.x, v.y, v.z);
			}
			JPH::ConvexHullShapeSettings settings(verts.data(), static_cast<int>(verts.size()));
			return settings.Create().Get();
		}
		return nullptr; // no vertices: the caller logs; never a silent box
	}
	case ShapeType::Mesh:
	{
		if (cfg.vertices.empty() || cfg.indices.empty()) return nullptr;
		JPH::TriangleList triangles;
		for (size_t i = 0; i + 2 < cfg.indices.size(); i += 3) {
			const auto& v0 = cfg.vertices[cfg.indices[i]];
			const auto& v1 = cfg.vertices[cfg.indices[i + 1]];
			const auto& v2 = cfg.vertices[cfg.indices[i + 2]];
			triangles.push_back(
				JPH::Triangle(
					JPH::Float3(v0.x, v0.y, v0.z),
					JPH::Float3(v1.x, v1.y, v1.z),
					JPH::Float3(v2.x, v2.y, v2.z)
				)
			);
		}
		JPH::MeshShapeSettings settings(triangles);
		return settings.Create().Get();
	}
	case ShapeType::Compound:
	{
		JPH::StaticCompoundShapeSettings settings;
		for (const auto& child_data : cfg.compound_children) {
			// Create a ShapeConfig from the ChildShape to reuse CreateShape
			ShapeConfig child_cfg;
			child_cfg.type = child_data.type;
			child_cfg.box_half_extents = child_data.box_half_extents;
			child_cfg.sphere_radius = child_data.sphere_radius;
			child_cfg.capsule_radius = child_data.capsule_radius;
			child_cfg.capsule_height = child_data.capsule_height;
			child_cfg.cylinder_radius = child_data.cylinder_radius;
			child_cfg.cylinder_height = child_data.cylinder_height;

			if (auto child = CreateShape(child_cfg)) {
				settings.AddShape(
					{child_data.position.x, child_data.position.y, child_data.position.z},
					{child_data.rotation.x, child_data.rotation.y, child_data.rotation.z, child_data.rotation.w},
					child
				);
			}
		}
		return settings.Create().Get();
	}
	default: break;
	}

	// Default box
	JPH::BoxShapeSettings settings({0.5F, 0.5F, 0.5F});
	return settings.Create().Get();
}

static ShapeConfig MakeShapeConfig(const CollisionShape& shape) {
	ShapeConfig cfg;
	cfg.type = shape.type;
	cfg.box_half_extents = shape.box_half_extents;
	cfg.sphere_radius = shape.sphere_radius;
	cfg.capsule_radius = shape.capsule_radius;
	cfg.capsule_height = shape.capsule_height;
	cfg.cylinder_radius = shape.cylinder_radius;
	cfg.cylinder_height = shape.cylinder_height;
	cfg.offset = shape.offset;
	cfg.rotation = shape.rotation;
	cfg.compound_children = shape.compound_children;
	return cfg;
}

// Builds (or reuses) the static mesh shape for a model file, unplaced (see PlaceShape). Logs one error and
// returns null when the body can't be a mesh: not static, file unreadable, or no triangles.
static JPH::ShapeRefC BuildMeshShape(
	MeshShapeCache& cache,
	const std::string_view entity_name,
	const RigidBody& body,
	const CollisionShape& shape
) {
	if (body.motion_type != MotionType::Static) {
		spdlog::error(
			"[Physics] Entity '{}': a mesh collider needs a static rigid_body (path '{}').",
			entity_name,
			shape.mesh_path
		);
		return nullptr;
	}

	JPH::ShapeRefC mesh;
	if (const auto cached = cache.shapes.find(shape.mesh_path); cached != cache.shapes.end()) {
		mesh = cached->second;
	}
	else {
		ShapeConfig cfg;
		cfg.type = ShapeType::Mesh;
		GltfGeometry geometry = shape.mesh_path.empty() ? GltfGeometry{} : LoadGltfGeometry(shape.mesh_path);
		if (geometry.Empty()) {
			spdlog::error(
				"[Physics] Entity '{}': mesh collider '{}' is missing, unreadable, or has no triangles.",
				entity_name,
				shape.mesh_path
			);
			return nullptr;
		}
		cfg.vertices = std::move(geometry.positions);
		cfg.indices = std::move(geometry.indices);
		mesh = CreateShape(cfg);
		if (!mesh) {
			spdlog::error("[Physics] Entity '{}': Jolt rejected the mesh collider '{}'.", entity_name, shape.mesh_path);
			return nullptr;
		}
		cache.shapes.emplace(shape.mesh_path, mesh);
	}

	return mesh;
}

// Places a rigid body's shape at its authored offset and rotation from the entity origin, so the body's position stays
// the entity's. Characters apply their own offset instead (CharacterRecord::shape_offset).
static JPH::ShapeRefC PlaceShape(const JPH::ShapeRefC& shape_ref, const CollisionShape& shape) {
	const bool identity_rotation = shape.rotation == glm::quat{1.0F, 0.0F, 0.0F, 0.0F};
	if (shape.offset == glm::vec3{0.0F} && identity_rotation) return shape_ref;
	return JPH::RotatedTranslatedShapeSettings(
			   JPH::Vec3(shape.offset.x, shape.offset.y, shape.offset.z),
			   JPH::Quat(shape.rotation.x, shape.rotation.y, shape.rotation.z, shape.rotation.w),
			   shape_ref
	)
		.Create()
		.Get();
}

// Builds a rigid body's placed Jolt shape, or logs an error naming the entity and returns null.
static JPH::ShapeRefC BuildBodyShape(
	MeshShapeCache& cache,
	const std::string_view entity_name,
	const RigidBody& body,
	const CollisionShape& shape
) {
	JPH::ShapeRefC jolt_shape;
	if (shape.type == ShapeType::Mesh) {
		jolt_shape = BuildMeshShape(cache, entity_name, body, shape);
		if (!jolt_shape) return nullptr;
	}
	else if (shape.type == ShapeType::ConvexHull) {
		spdlog::error("[Physics] Entity '{}': a convex hull collider has no vertices to build from.", entity_name);
		return nullptr;
	}
	else {
		jolt_shape = CreateShape(MakeShapeConfig(shape));
		if (!jolt_shape) {
			spdlog::error("[Physics] Entity '{}': failed to create shape.", entity_name);
			return nullptr;
		}
	}
	return PlaceShape(jolt_shape, shape);
}

// Reads a body's position, rotation and velocities.
static PhysicsSyncResult ReadBodyState(const JPH::BodyInterface& body_interface, const JPH::BodyID bid) {
	JPH::RVec3 pos;
	JPH::Quat rot{};
	body_interface.GetPositionAndRotation(bid, pos, rot);
	const JPH::Vec3 lin_vel = body_interface.GetLinearVelocity(bid);
	const JPH::Vec3 ang_vel = body_interface.GetAngularVelocity(bid);
	return {
		.position = {pos.GetX(), pos.GetY(), pos.GetZ()},
		.rotation = {rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()},
		.linear_velocity = {lin_vel.GetX(), lin_vel.GetY(), lin_vel.GetZ()},
		.angular_velocity = {ang_vel.GetX(), ang_vel.GetY(), ang_vel.GetZ()},
	};
}

// Whether the body has motion properties, which Jolt needs to make it kinematic or dynamic. A body created static has
// none; one created kinematic or dynamic keeps them after being made static.
static bool HasMotionProperties(const JPH::BodyLockInterface& locks, const JPH::BodyID bid) {
	const JPH::BodyLockRead lock(locks, bid);
	return lock.Succeeded() && lock.GetBody().GetMotionPropertiesUnchecked() != nullptr;
}

// The one RigidBody -> Jolt mapping, for a new body and a changed one alike: everything but the shape and the user data.
// A body without motion properties stays static whatever `body.motion_type` asks, and skips the fields that live in
// them (gravity, CCD, damping, mass).
static void ApplyRigidBody(
	JPH::BodyInterface& body_interface,
	const JPH::BodyLockInterface& locks,
	const JPH::BodyID bid,
	const RigidBody& body
) {
	const bool has_motion = HasMotionProperties(locks, bid);
	const MotionType motion_type = has_motion ? body.motion_type : MotionType::Static;

	body_interface.SetMotionType(bid, ToJoltMotionType(motion_type), JPH::EActivation::Activate);
	// The moving bit of the object layer follows the motion type.
	body_interface.SetObjectLayer(bid, ToObjectLayer(body.layer, motion_type != MotionType::Static));
	body_interface.SetFriction(bid, body.friction);
	body_interface.SetRestitution(bid, body.restitution);
	if (motion_type == MotionType::Static) return;

	body_interface.SetGravityFactor(bid, body.use_gravity ? body.gravity_scale : 0.0F);
	body_interface.SetMotionQuality(
		bid,
		body.enable_ccd ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete
	);

	const JPH::BodyLockWrite lock(locks, bid);
	if (!lock.Succeeded()) return;
	JPH::Body& jolt_body = lock.GetBody();
	JPH::MotionProperties* motion = jolt_body.GetMotionProperties();
	motion->SetLinearDamping(body.linear_damping);
	motion->SetAngularDamping(body.angular_damping);
	if (motion_type == MotionType::Dynamic) {
		// The shape's inertia, scaled to the authored mass (Jolt's EOverrideMassProperties::CalculateInertia).
		JPH::MassProperties mass_properties = jolt_body.GetShape()->GetMassProperties();
		mass_properties.ScaleToMass(body.mass);
		motion->SetMassProperties(JPH::EAllowedDOFs::All, mass_properties);
	}
}

// Creates and adds a body at `transform`, then applies `body` to it; an invalid BodyID (logged) when Jolt refuses.
static JPH::BodyID CreateJoltBody(
	JPH::BodyInterface& body_interface,
	const JPH::BodyLockInterface& locks,
	const flecs::entity_t entity,
	const JPH::ShapeRefC& jolt_shape,
	const PhysicsTransform& transform,
	const RigidBody& body
) {
	JPH::BodyCreationSettings settings(
		jolt_shape,
		{transform.position.x, transform.position.y, transform.position.z},
		{transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w},
		ToJoltMotionType(body.motion_type),
		ToObjectLayer(body.layer, body.motion_type != MotionType::Static)
	);
	settings.mAllowSleeping = true;
	settings.mUserData = entity;

	const JPH::BodyID bid = body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
	if (bid.IsInvalid()) {
		spdlog::error("[Physics] Failed to create body for entity {}", entity);
		return bid;
	}
	ApplyRigidBody(body_interface, locks, bid, body);
	return bid;
}

void JoltBackend::SyncBodyToBackend(
	const flecs::entity_t entity,
	const std::string_view entity_name,
	const PhysicsTransform& transform,
	const RigidBody& body,
	const CollisionShape& shape
) {
	if (!initialized_) {
		spdlog::error("[Physics] Cannot sync - not initialized");
		return;
	}

	auto& body_interface = physics_system_->GetBodyInterface();
	const auto& locks = physics_system_->GetBodyLockInterface();
	const auto existing = entity_to_body_.find(entity);

	if (existing == entity_to_body_.end()) {
		const JPH::ShapeRefC jolt_shape = BuildBodyShape(*mesh_shape_cache_, entity_name, body, shape);
		if (!jolt_shape) return;
		if (const JPH::BodyID bid = CreateJoltBody(body_interface, locks, entity, jolt_shape, transform, body);
			!bid.IsInvalid()) {
			entity_to_body_[entity] = {.id = bid.GetIndexAndSequenceNumber(), .body = body, .shape = shape};
		}
		return;
	}

	BodyRecord& record = existing->second;
	if (record.body == body && record.shape == shape) return;

	const JPH::BodyID bid(record.id);
	// Jolt can't make a body without motion properties (one created static) kinematic or dynamic, so that takes a new body.
	const bool leaves_static = record.body.motion_type != body.motion_type
							   && body.motion_type != MotionType::Static
							   && !HasMotionProperties(locks, bid);
	const bool recreate = record.shape != shape || leaves_static;

	if (!recreate || !RecreateBody(entity, entity_name, record, body, shape)) {
		// In place; also after a failed recreate (logged), so the rest of the change still reaches the old body.
		ApplyRigidBody(body_interface, locks, bid, body);
	}
	// Recorded even when a recreate failed, so setting the same values again doesn't retry and log the failure again.
	record.body = body;
	record.shape = shape;
}

bool JoltBackend::RecreateBody(
	const flecs::entity_t entity,
	const std::string_view entity_name,
	BodyRecord& record,
	const RigidBody& body,
	const CollisionShape& shape
) {
	const JPH::ShapeRefC jolt_shape = BuildBodyShape(*mesh_shape_cache_, entity_name, body, shape);
	if (!jolt_shape) return false;

	auto& body_interface = physics_system_->GetBodyInterface();
	const JPH::BodyID old_bid(record.id);
	const PhysicsSyncResult state = ReadBodyState(body_interface, old_bid);
	const JPH::BodyID new_bid = CreateJoltBody(
		body_interface,
		physics_system_->GetBodyLockInterface(),
		entity,
		jolt_shape,
		{.position = state.position, .rotation = state.rotation},
		body
	);
	if (new_bid.IsInvalid()) return false;

	body_interface.RemoveBody(old_bid);
	body_interface.DestroyBody(old_bid);
	if (body.motion_type != MotionType::Static) {
		body_interface.SetLinearAndAngularVelocity(
			new_bid,
			{state.linear_velocity.x, state.linear_velocity.y, state.linear_velocity.z},
			{state.angular_velocity.x, state.angular_velocity.y, state.angular_velocity.z}
		);
	}
	record.id = new_bid.GetIndexAndSequenceNumber();
	return true;
}

void JoltBackend::SetBodyTransform(const flecs::entity_t entity, const PhysicsTransform& transform) {
	auto& body_interface = physics_system_->GetBodyInterface();

	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		const JPH::BodyID bid(it->second.id);
		const JPH::Vec3 linear_vel = body_interface.GetLinearVelocity(bid);
		const JPH::Vec3 angular_vel = body_interface.GetAngularVelocity(bid);
		body_interface.SetPositionAndRotationWhenChanged(
			bid,
			{transform.position.x, transform.position.y, transform.position.z},
			{transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w},
			JPH::EActivation::Activate
		);
		body_interface.SetLinearVelocity(bid, linear_vel);
		body_interface.SetAngularVelocity(bid, angular_vel);
		body_interface.ActivateBody(bid);
	}
}

PhysicsSyncResult JoltBackend::GetPhysicsSyncResult(const flecs::entity_t entity) const {
	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		return ReadBodyState(physics_system_->GetBodyInterface(), JPH::BodyID(it->second.id));
	}
	return {};
}

void JoltBackend::RemoveBody(const flecs::entity_t entity) {
	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		auto& body_interface = physics_system_->GetBodyInterface();
		const JPH::BodyID bid(it->second.id);
		if (!bid.IsInvalid() && body_interface.IsAdded(bid)) {
			body_interface.RemoveBody(bid);
		}
		if (!bid.IsInvalid()) {
			body_interface.DestroyBody(bid);
		}
		entity_to_body_.erase(it);
	}
}

bool JoltBackend::HasBody(const flecs::entity_t entity) const { return entity_to_body_.contains(entity); }

void JoltBackend::ApplyForce(const flecs::entity_t entity, const glm::vec3& force, const glm::vec3& torque) {
	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		auto& body_interface = physics_system_->GetBodyInterface();
		const JPH::BodyID bid(it->second.id);
		if (glm::length(force) > 0.0F) {
			body_interface.AddForce(bid, {force.x, force.y, force.z});
		}
		if (glm::length(torque) > 0.0F) {
			body_interface.AddTorque(bid, {torque.x, torque.y, torque.z});
		}
	}
}

void JoltBackend::ApplyImpulse(const flecs::entity_t entity, const glm::vec3& impulse, const glm::vec3& point) {
	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		auto& body_interface = physics_system_->GetBodyInterface();
		const JPH::BodyID bid(it->second.id);
		const JPH::Vec3 jolt_impulse{impulse.x, impulse.y, impulse.z};
		if (glm::length(point) > 0.0F) {
			body_interface.AddImpulse(bid, jolt_impulse, JPH::RVec3(point.x, point.y, point.z));
		}
		else {
			body_interface.AddImpulse(bid, jolt_impulse);
		}
	}
}

void JoltBackend::SetBodyVelocity(const flecs::entity_t entity, const glm::vec3& linear, const glm::vec3& angular) {
	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		auto& body_interface = physics_system_->GetBodyInterface();
		const JPH::BodyID bid(it->second.id);
		body_interface.SetLinearAndAngularVelocity(
			bid,
			{linear.x, linear.y, linear.z},
			{angular.x, angular.y, angular.z}
		);
	}
}

namespace {
constexpr float kMinGravityLength = 1e-6F;
// A grounded character whose vertical speed is below this isn't moving away from the ground.
constexpr float kLeavingGroundSpeed = 0.1F;

JPH::Vec3 ToJolt(const glm::vec3& v) { return {v.x, v.y, v.z}; }
glm::vec3 FromJolt(const JPH::Vec3 v) { return {v.GetX(), v.GetY(), v.GetZ()}; }

// up = -normalize(gravity); zero gravity keeps world Y so a character in zero-g still has an up to stand on.
JPH::Vec3 UpFromGravity(const glm::vec3& gravity) {
	const float length = glm::length(gravity);
	return length < kMinGravityLength ? JPH::Vec3::sAxisY() : ToJolt(-gravity / length);
}

// Rotates the capsule's Y axis onto up.
JPH::Quat AlignToUp(const JPH::Vec3 up) { return JPH::Quat::sFromTo(JPH::Vec3::sAxisY(), up); }

// A character always moves, so its inner body (and the filters for its own collision checks) use the moving layer.
JPH::ObjectLayer CharacterObjectLayer(const CharacterBody& body) { return ToObjectLayer(body.layer, true); }
} // namespace

void JoltBackend::AddCharacter(
	const flecs::entity_t entity,
	const std::string_view entity_name,
	const glm::vec3& position,
	const CharacterBody& body,
	const CollisionShape& shape
) {
	if (!initialized_) return;
	if (shape.type != ShapeType::Capsule) {
		spdlog::error("[Physics] Entity '{}': a character_body needs a capsule collision_shape.", entity_name);
		return;
	}
	characters_.erase(entity);

	const JPH::ShapeRefC capsule = CreateShape(MakeShapeConfig(shape));
	if (!capsule) {
		spdlog::error("[Physics] Entity '{}': failed to create the character capsule.", entity_name);
		return;
	}

	const JPH::Vec3 up = JPH::Vec3::sAxisY();
	JPH::CharacterVirtualSettings settings;
	settings.mShape = capsule;
	settings.mUp = up;
	settings.mMaxSlopeAngle = glm::radians(body.max_slope_deg);
	settings.mMass = body.mass;
	// Only contacts below the capsule's bottom hemisphere count as support (Jolt's character sample does the same).
	settings.mSupportingVolume = JPH::Plane(up, -shape.capsule_radius);
	if (body.inner_body) {
		// Jolt places the inner body at the character's position (the capsule center) with its rotation, so the same
		// unplaced capsule lines up with the character's own, and gives it the character's user data (the entity).
		settings.mInnerBodyShape = capsule;
		settings.mInnerBodyLayer = CharacterObjectLayer(body);
	}

	auto record = std::make_unique<CharacterRecord>();
	record->settings = body;
	record->shape_offset = shape.offset;
	record->written_position = position;
	record->character = new JPH::CharacterVirtual(
		&settings,
		ToJolt(position + shape.offset),
		JPH::Quat::sIdentity(),
		entity,
		physics_system_.get()
	);
	characters_[entity] = std::move(record);
}

void JoltBackend::RemoveCharacter(const flecs::entity_t entity) { characters_.erase(entity); }

bool JoltBackend::HasCharacter(const flecs::entity_t entity) const { return characters_.contains(entity); }

void JoltBackend::SetCharacterPosition(const flecs::entity_t entity, const glm::vec3& position) {
	const auto it = characters_.find(entity);
	if (it == characters_.end()) return;
	CharacterRecord& record = *it->second;
	const JPH::Quat align = record.character->GetRotation();
	record.character->SetPosition(ToJolt(position) + align * ToJolt(record.shape_offset));
	record.written_position = position;
}

void JoltBackend::SetCharacterVelocity(const flecs::entity_t entity, const glm::vec3& velocity) {
	if (const auto it = characters_.find(entity); it != characters_.end()) {
		it->second->character->SetLinearVelocity(ToJolt(velocity));
	}
}

void JoltBackend::RefreshCharacterContacts(const flecs::entity_t entity) {
	const auto it = characters_.find(entity);
	if (it == characters_.end()) return;
	const JPH::ObjectLayer layer = CharacterObjectLayer(it->second->settings);
	it->second->character->RefreshContacts(
		physics_system_->GetDefaultBroadPhaseLayerFilter(layer),
		physics_system_->GetDefaultLayerFilter(layer),
		{},
		{},
		*temp_allocator_
	);
}

void JoltBackend::StepCharacter(const flecs::entity_t entity, const float delta_time, const CharacterStepInput& input) {
	const auto it = characters_.find(entity);
	if (it == characters_.end()) return;
	CharacterRecord& record = *it->second;
	JPH::CharacterVirtual& character = *record.character;

	const JPH::Vec3 up = UpFromGravity(input.gravity);
	const JPH::Vec3 old_up = character.GetUp();
	const JPH::Quat old_rotation = character.GetRotation();
	const JPH::Quat align = AlignToUp(up);
	if (!up.IsClose(old_up)) {
		// The capsule's center moves with the entity origin when up changes, so keep the origin fixed.
		const JPH::RVec3 origin = character.GetPosition() - old_rotation * ToJolt(record.shape_offset);
		character.SetUp(up);
		character.SetRotation(align);
		character.SetPosition(origin + align * ToJolt(record.shape_offset));
	}

	const JPH::Vec3 move = ToJolt(input.move_velocity);
	const JPH::Vec3 planar = move - up * move.Dot(up);
	const float vertical_speed = character.GetLinearVelocity().Dot(up);
	const bool on_ground = character.GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
	const float kept_vertical = (on_ground && vertical_speed < kLeavingGroundSpeed) ? 0.0F : vertical_speed;
	const JPH::Vec3 gravity = ToJolt(input.gravity);
	character.SetLinearVelocity(planar + up * kept_vertical + gravity * delta_time);

	JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
	update_settings.mStickToFloorStepDown = -up * record.settings.stick_to_floor;
	update_settings.mWalkStairsStepUp = up * record.settings.step_height;
	const JPH::ObjectLayer layer = CharacterObjectLayer(record.settings);
	const ScopedFlag stepping(stepping_);
	character.ExtendedUpdate(
		delta_time,
		gravity,
		update_settings,
		physics_system_->GetDefaultBroadPhaseLayerFilter(layer),
		physics_system_->GetDefaultLayerFilter(layer),
		{},
		{},
		*temp_allocator_
	);
}

CharacterSyncResult JoltBackend::GetCharacterSyncResult(const flecs::entity_t entity) const {
	CharacterSyncResult result;
	const auto it = characters_.find(entity);
	if (it == characters_.end()) return result;
	const CharacterRecord& record = *it->second;
	const JPH::CharacterVirtual& character = *record.character;

	const JPH::Vec3 center = JPH::Vec3(character.GetPosition());
	result.position = FromJolt(center - character.GetRotation() * ToJolt(record.shape_offset));
	result.velocity = FromJolt(character.GetLinearVelocity());
	result.grounded = character.GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
	if (result.grounded) {
		result.ground_normal = FromJolt(character.GetGroundNormal());
		if (const auto* ground = physics_system_->GetBodyLockInterface().TryGetBody(character.GetGroundBodyID())) {
			result.ground_entity = ground->GetUserData();
		}
	}
	else {
		result.ground_normal = FromJolt(character.GetUp());
	}
	return result;
}

glm::vec3 JoltBackend::GetCharacterWrittenPosition(const flecs::entity_t entity) const {
	const auto it = characters_.find(entity);
	return it == characters_.end() ? glm::vec3{0.0F} : it->second->written_position;
}

void JoltBackend::SetCharacterWrittenPosition(const flecs::entity_t entity, const glm::vec3& position) {
	if (const auto it = characters_.find(entity); it != characters_.end()) {
		it->second->written_position = position;
	}
}

std::vector<CollisionInfo> JoltBackend::TakeCollisionEvents() {
	if (contact_listener_) {
		return dynamic_cast<ContactListenerImpl*>(contact_listener_.get())->TakeEvents();
	}
	return {};
}

bool JoltBackend::RejectQueryWhileStepping() const {
	if (!stepping_) return false;
	spdlog::error(
		"[Physics] A raycast was made while the physics world steps; it hits nothing. Query outside PhysicsStep."
	);
	return true;
}

namespace {
// The Jolt ray for a query: a normalized direction scaled by max_distance, so a hit's fraction times max_distance is
// its distance. Empty for a ray that can hit nothing.
std::optional<JPH::RRayCast> MakeJoltRay(const Ray& ray) {
	const float length = glm::length(ray.direction);
	if (length < kMinRayDirectionLength || ray.max_distance <= 0.0F) return std::nullopt;
	return JPH::RRayCast(ToJolt(ray.origin), ToJolt(ray.direction / length * ray.max_distance));
}

// Fills a hit from Jolt's result. Inside a convex shape (fraction 0) there is no surface at the hit point, so the
// normal faces back along the ray.
RaycastResult MakeRaycastResult(
	const JPH::PhysicsSystem& system,
	const JPH::RRayCast& jolt_ray,
	const JPH::RayCastResult& hit,
	const float max_distance
) {
	RaycastResult result;
	const JPH::RVec3 point = jolt_ray.GetPointOnRay(hit.mFraction);
	result.hit_point = FromJolt(JPH::Vec3(point));
	result.distance = hit.mFraction * max_distance;
	result.normal = FromJolt(-jolt_ray.mDirection.Normalized());
	const JPH::BodyLockRead lock(system.GetBodyLockInterface(), hit.mBodyID);
	if (!lock.Succeeded()) return result;
	const JPH::Body& body = lock.GetBody();
	result.entity = body.GetUserData();
	if (hit.mFraction > 0.0F) {
		result.normal = FromJolt(body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point));
	}
	return result;
}
} // namespace

std::optional<RaycastResult> JoltBackend::Raycast(const Ray& ray, const RaycastFilter& filter) const {
	if (!physics_system_ || RejectQueryWhileStepping()) return std::nullopt;
	const auto jolt_ray = MakeJoltRay(ray);
	if (!jolt_ray) return std::nullopt;

	const LayerMaskFilter layer_filter(filter.layer_mask);
	const IgnoreEntitiesFilter body_filter(filter.ignore);
	JPH::RayCastResult hit;
	std::optional<RaycastResult> result;
	if (physics_system_->GetNarrowPhaseQuery().CastRay(*jolt_ray, hit, {}, layer_filter, body_filter)) {
		result = MakeRaycastResult(*physics_system_, *jolt_ray, hit, ray.max_distance);
	}
	DebugRecordRay(ray, result ? std::span<const RaycastResult>(&*result, 1) : std::span<const RaycastResult>{});
	return result;
}

std::vector<RaycastResult> JoltBackend::RaycastAll(const Ray& ray, const RaycastFilter& filter) const {
	if (!physics_system_ || RejectQueryWhileStepping()) return {};
	const auto jolt_ray = MakeJoltRay(ray);
	if (!jolt_ray) return {};

	// Jolt reports a hit per sub-shape (each triangle of a mesh, each child of a compound); keep each body's nearest.
	const LayerMaskFilter layer_filter(filter.layer_mask);
	const IgnoreEntitiesFilter body_filter(filter.ignore);
	JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
	physics_system_->GetNarrowPhaseQuery().CastRay(*jolt_ray, {}, collector, {}, layer_filter, body_filter);
	collector.Sort();

	std::vector<RaycastResult> results;
	std::vector<JPH::BodyID> seen;
	for (const JPH::RayCastResult& hit : collector.mHits) {
		if (std::ranges::find(seen, hit.mBodyID) != seen.end()) continue;
		seen.push_back(hit.mBodyID);
		results.push_back(MakeRaycastResult(*physics_system_, *jolt_ray, hit, ray.max_distance));
	}
	DebugRecordRay(ray, results);
	return results;
}

void JoltBackend::DebugRecordRay(
	[[maybe_unused]] const Ray& ray,
	[[maybe_unused]] const std::span<const RaycastResult> hits
) const {
#ifdef ENGINE_PHYSICS_DEBUG_RENDERER
	if (debug_renderer_ == nullptr || !debug_renderer_->Enabled()) return;
	const float length = glm::length(ray.direction);
	debug_renderer_->RecordRay(ray.origin, ray.origin + ray.direction / length * ray.max_distance, hits);
#endif
}

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER
void JoltBackend::SetDebugRenderer(JoltDebugRendererAdapter* renderer) { debug_renderer_ = renderer; }

void JoltBackend::DebugDrawBodies() const {
	if (!physics_system_ || !debug_renderer_) return;

	// Draw all bodies using Jolt's debug drawing system
	physics_system_->DrawBodies(JPH::BodyManager::DrawSettings{}, debug_renderer_);
}
#endif

} // namespace engine::physics
