#include "jolt_backend.hpp"

#include <algorithm>
#include <cstdarg>
#include <ranges>
#include <thread>

#include <spdlog/spdlog.h>

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
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
// clang-format on

namespace engine::physics {

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

// Layer configuration
namespace {
constexpr JPH::ObjectLayer LAYER_NON_MOVING = 0;
constexpr JPH::ObjectLayer LAYER_MOVING = 1;
[[maybe_unused]] constexpr JPH::ObjectLayer NUM_OBJECT_LAYERS = 2;

constexpr JPH::BroadPhaseLayer BP_LAYER_NON_MOVING(0);
constexpr JPH::BroadPhaseLayer BP_LAYER_MOVING(1);

class BroadPhaseLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface {
public:
	[[nodiscard]] uint32_t GetNumBroadPhaseLayers() const override { return 2; }

	[[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(const JPH::ObjectLayer layer) const override {
		return layer == LAYER_NON_MOVING ? BP_LAYER_NON_MOVING : BP_LAYER_MOVING;
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
		if (layer == LAYER_NON_MOVING) {
			return static_cast<JPH::BroadPhaseLayer::Type>(bp_layer) == 1; // Only collide with MOVING
		}
		return true;
	}
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
	[[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer layer1, const JPH::ObjectLayer layer2) const override {
		return layer1 == LAYER_NON_MOVING ? layer2 == LAYER_MOVING : true;
	}
};

class ContactListenerImpl : public JPH::ContactListener {
public:
	void ClearEvents() { events_.clear(); }

	[[nodiscard]] const std::vector<CollisionInfo>& GetEvents() const { return events_; }

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

JoltBackend::JoltBackend(const PhysicsConfig& config) : config_(config) {}

JoltBackend::~JoltBackend() { Shutdown(); }

bool JoltBackend::Initialize() {
	if (initialized_) {
		spdlog::warn("[Physics] Already initialized");
		return true;
	}

	// Register Jolt allocators and callbacks
	JPH::RegisterDefaultAllocator();
	JPH::Trace = JoltTraceImpl;
#ifdef JPH_ENABLE_ASSERTS
	JPH::AssertFailed = JoltAssertFailedImpl;
#endif

	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

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
	for (const auto body_index : entity_to_body_ | std::views::values) {
		JPH::BodyID bid(body_index);
		body_interface.RemoveBody(bid);
		body_interface.DestroyBody(bid);
	}
	entity_to_body_.clear();

	physics_system_.reset();
	contact_listener_.reset();
	object_layer_pair_filter_.reset();
	object_vs_broad_phase_layer_filter_.reset();
	broad_phase_layer_interface_.reset();
	job_system_.reset();
	temp_allocator_.reset();

	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;

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
		auto g = physics_system_->GetGravity();
		return {g.GetX(), g.GetY(), g.GetZ()};
	}
	return config_.gravity;
}

void JoltBackend::StepSimulation(const float delta_time) const {
	if (!initialized_ || !physics_system_) return;

	if (contact_listener_) {
		dynamic_cast<ContactListenerImpl*>(contact_listener_.get())->ClearEvents();
	}

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

static JPH::ObjectLayer GetObjectLayer(const MotionType type) {
	return type == MotionType::Static ? LAYER_NON_MOVING : LAYER_MOVING;
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
		break;
	}
	case ShapeType::Mesh:
	{
		if (!cfg.vertices.empty() && !cfg.indices.empty()) {
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
		break;
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

void JoltBackend::SyncBodyToBackend(
	EntityId entity,
	const PhysicsTransform& transform,
	const RigidBody& body,
	const CollisionShape& shape
) {
	if (!initialized_) {
		spdlog::error("[Physics] Cannot sync - not initialized");
		return;
	}

	ShapeConfig shape_cfg;
	shape_cfg.type = shape.type;
	shape_cfg.box_half_extents = shape.box_half_extents;
	shape_cfg.sphere_radius = shape.sphere_radius;
	shape_cfg.capsule_radius = shape.capsule_radius;
	shape_cfg.capsule_height = shape.capsule_height;
	shape_cfg.cylinder_radius = shape.cylinder_radius;
	shape_cfg.cylinder_height = shape.cylinder_height;
	shape_cfg.offset = shape.offset;
	shape_cfg.rotation = shape.rotation;
	shape_cfg.compound_children = shape.compound_children;

	auto jolt_shape = CreateShape(shape_cfg);
	if (!jolt_shape) {
		spdlog::error("[Physics] Failed to create shape for entity {}", entity);
		return;
	}

	auto& body_interface = physics_system_->GetBodyInterface();

	if (auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		// Update existing body
		JPH::BodyID bid(it->second);
		body_interface.SetShape(bid, jolt_shape, true, JPH::EActivation::Activate);
		body_interface.SetPositionAndRotation(
			bid,
			{transform.position.x, transform.position.y, transform.position.z},
			{transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w},
			JPH::EActivation::Activate
		);
		body_interface.SetFriction(bid, body.friction);
		body_interface.SetRestitution(bid, body.restitution);
		body_interface.SetGravityFactor(bid, body.use_gravity ? body.gravity_scale : 0.0F);
		body_interface.SetMotionQuality(
			bid,
			body.enable_ccd ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete
		);
	}
	else {
		// Create new body
		JPH::BodyCreationSettings settings(
			jolt_shape,
			{transform.position.x, transform.position.y, transform.position.z},
			{transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w},
			ToJoltMotionType(body.motion_type),
			GetObjectLayer(body.motion_type)
		);
		settings.mFriction = body.friction;
		settings.mRestitution = body.restitution;
		settings.mLinearDamping = body.linear_damping;
		settings.mAngularDamping = body.angular_damping;
		settings.mGravityFactor = body.use_gravity ? body.gravity_scale : 0.0F;
		settings.mAllowSleeping = true;

		if (body.motion_type == MotionType::Dynamic) {
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			settings.mMassPropertiesOverride.mMass = body.mass;
		}

		if (body.enable_ccd) {
			settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
		}

		settings.mUserData = entity;

		if (JPH::BodyID bid = body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate); !bid.IsInvalid()) {
			entity_to_body_[entity] = bid.GetIndexAndSequenceNumber();
		}
		else {
			spdlog::error("[Physics] Failed to create body for entity {}", entity);
		}
	}
}

PhysicsSyncResult JoltBackend::SyncBodyFromBackend(const EntityId entity) const {
	PhysicsSyncResult result;

	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		const auto& body_interface = physics_system_->GetBodyInterface();
		const JPH::BodyID bid(it->second);
		JPH::RVec3 pos;
		JPH::Quat rot{};
		body_interface.GetPositionAndRotation(bid, pos, rot);

		result.position = {pos.GetX(), pos.GetY(), pos.GetZ()};
		result.rotation = {rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()};

		const auto lin_vel = body_interface.GetLinearVelocity(bid);
		result.linear_velocity = {lin_vel.GetX(), lin_vel.GetY(), lin_vel.GetZ()};

		const auto ang_vel = body_interface.GetAngularVelocity(bid);
		result.angular_velocity = {ang_vel.GetX(), ang_vel.GetY(), ang_vel.GetZ()};
	}

	return result;
}

void JoltBackend::RemoveBody(const EntityId entity) {
	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		auto& body_interface = physics_system_->GetBodyInterface();
		const JPH::BodyID bid(it->second);
		if (!bid.IsInvalid() && body_interface.IsAdded(bid)) {
			body_interface.RemoveBody(bid);
		}
		if (!bid.IsInvalid()) {
			body_interface.DestroyBody(bid);
		}
		entity_to_body_.erase(it);
	}
}

bool JoltBackend::HasBody(const EntityId entity) const { return entity_to_body_.contains(entity); }

void JoltBackend::ApplyForce(const EntityId entity, const glm::vec3& force, const glm::vec3& torque) {
	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		auto& body_interface = physics_system_->GetBodyInterface();
		const JPH::BodyID bid(it->second);
		if (glm::length(force) > 0.0F) {
			body_interface.AddForce(bid, {force.x, force.y, force.z});
		}
		if (glm::length(torque) > 0.0F) {
			body_interface.AddTorque(bid, {torque.x, torque.y, torque.z});
		}
	}
}

void JoltBackend::ApplyImpulse(const EntityId entity, const glm::vec3& impulse, const glm::vec3& point) {
	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		auto& body_interface = physics_system_->GetBodyInterface();
		const JPH::BodyID bid(it->second);
		const JPH::Vec3 jolt_impulse{impulse.x, impulse.y, impulse.z};
		if (glm::length(point) > 0.0F) {
			body_interface.AddImpulse(bid, jolt_impulse, JPH::RVec3(point.x, point.y, point.z));
		}
		else {
			body_interface.AddImpulse(bid, jolt_impulse);
		}
	}
}

void JoltBackend::SetBodyVelocity(const EntityId entity, const glm::vec3& linear, const glm::vec3& angular) {
	if (const auto it = entity_to_body_.find(entity); it != entity_to_body_.end()) {
		auto& body_interface = physics_system_->GetBodyInterface();
		const JPH::BodyID bid(it->second);
		body_interface.SetLinearAndAngularVelocity(
			bid,
			{linear.x, linear.y, linear.z},
			{angular.x, angular.y, angular.z}
		);
	}
}

std::vector<CollisionInfo> JoltBackend::GetCollisionEvents() const {
	if (contact_listener_) {
		return dynamic_cast<ContactListenerImpl*>(contact_listener_.get())->GetEvents();
	}
	return {};
}

std::optional<RaycastResult> JoltBackend::Raycast(const Ray& ray) const {
	if (!physics_system_) return std::nullopt;

	const JPH::RRayCast jolt_ray(
		{ray.origin.x, ray.origin.y, ray.origin.z},
		{ray.direction.x * ray.max_distance, ray.direction.y * ray.max_distance, ray.direction.z * ray.max_distance}
	);

	if (JPH::RayCastResult hit; physics_system_->GetNarrowPhaseQuery().CastRay(jolt_ray, hit)) {
		RaycastResult result;
		const auto hit_pt = jolt_ray.GetPointOnRay(hit.mFraction);
		result.hit_point = {hit_pt.GetX(), hit_pt.GetY(), hit_pt.GetZ()};
		result.distance = hit.mFraction * ray.max_distance;
		if (const auto* body = physics_system_->GetBodyLockInterface().TryGetBody(hit.mBodyID)) {
			result.entity = body->GetUserData();
		}
		return result;
	}

	return std::nullopt;
}

std::vector<RaycastResult> JoltBackend::RaycastAll(const Ray& ray) const {
	if (auto hit = Raycast(ray)) {
		return {*hit};
	}
	return {};
}

#ifdef JPH_DEBUG_RENDERER
void JoltBackend::SetDebugRenderer(JPH::DebugRenderer* renderer) { debug_renderer_ = renderer; }

void JoltBackend::DebugDrawBodies() const {
	if (!physics_system_ || !debug_renderer_) return;

	// Draw all bodies using Jolt's debug drawing system
	physics_system_->DrawBodies(JPH::BodyManager::DrawSettings{}, debug_renderer_);
}
#endif

} // namespace engine::physics
