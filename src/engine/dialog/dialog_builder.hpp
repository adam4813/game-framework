#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

#include "dialog_components.hpp"

namespace engine::dialog {

/// Builder for dialog interaction trees. Accumulates tree structure in memory,
/// then materializes it into ECS entities via build().
///
/// Usage:
///   DialogBuilder b;
///   b.Node("greeting")
///    .Line("NPC", "Hello!")
///    .Child("choice_yes")
///     .Choice("Yes please", 0);
///   auto root = b.Build(world, "MyDialog", scene_root);
class DialogBuilder {
public:
	DialogBuilder() = default;

	/// Start/get a node by ID. Returns *this for chaining.
	DialogBuilder& Node(const std::string& id) {
		// If this is the first node created, it becomes the root of the tree.
		if (root_id_.empty()) {
			root_id_ = id;
		}
		current_id_ = id;
		if (!nodes_.contains(id)) {
			nodes_[id] = NodeSpec{.id = id};
		}
		return *this;
	}

	/// Set a line (dialogue text) on the current node.
	DialogBuilder& Line(const std::string& speaker, const std::string& text) {
		auto& node = nodes_[current_id_];
		node.speaker = speaker;
		node.text = text;
		node.type = "line";
		return *this;
	}

	/// Set a choice on the current node.
	DialogBuilder& Choice(const std::string& label, const int order = 0) {
		auto& node = nodes_[current_id_];
		node.choice_label = label;
		node.choice_order = order;
		node.type = "choice";
		return *this;
	}

	/// Set a choice with a gating flag on the current node.
	DialogBuilder& Choice(const std::string& label, const std::string& requires_flag, const int order = 0) {
		auto& node = nodes_[current_id_];
		node.choice_label = label;
		node.choice_requires_flag = requires_flag;
		node.choice_order = order;
		node.type = "choice";
		return *this;
	}

	/// Mark the current node as an end (terminal) node.
	DialogBuilder& End() {
		auto& node = nodes_[current_id_];
		node.type = "end";
		return *this;
	}

	/// Set a wait (pause) on the current node.
	DialogBuilder& Wait(const float seconds = 1.0F) {
		auto& node = nodes_[current_id_];
		node.wait_seconds = seconds;
		node.type = "wait";
		return *this;
	}

	/// Set a move action on the current node.
	DialogBuilder& Move(const glm::vec3& to, const float speed = 4.0F, const bool wait_for_arrival = true) {
		auto& node = nodes_[current_id_];
		node.move_to = to;
		node.move_speed = speed;
		node.move_wait_for_arrival = wait_for_arrival;
		node.type = "move";
		return *this;
	}

	/// Set a follow action on the current node.
	DialogBuilder& Follow(const float speed = 3.0F, const float stop_distance = 1.0F) {
		auto& node = nodes_[current_id_];
		node.follow_speed = speed;
		node.follow_stop_distance = stop_distance;
		node.type = "follow";
		return *this;
	}

	/// Set a give_item action (remove from inventory) on the current node.
	DialogBuilder& GiveItem(const std::string& item, const int count = 1) {
		auto& node = nodes_[current_id_];
		node.item_name = item;
		node.item_count = count;
		node.type = "give_item";
		return *this;
	}

	/// Set a take_item action (add to inventory) on the current node.
	DialogBuilder& TakeItem(const std::string& item, const int count = 1) {
		auto& node = nodes_[current_id_];
		node.item_name = item;
		node.item_count = count;
		node.type = "take_item";
		return *this;
	}

	/// Set a give_coins action (remove coins) on the current node.
	DialogBuilder& GiveCoins(const int amount = 1) {
		auto& node = nodes_[current_id_];
		node.coin_amount = amount;
		node.type = "give_coins";
		return *this;
	}

	/// Set a take_coins action (add coins) on the current node.
	DialogBuilder& TakeCoins(const int amount = 1) {
		auto& node = nodes_[current_id_];
		node.coin_amount = amount;
		node.type = "take_coins";
		return *this;
	}

	/// Set a condition (branch on flag) on the current node.
	DialogBuilder& Condition(const std::string& flag) {
		auto& node = nodes_[current_id_];
		node.condition_flag = flag;
		node.type = "condition";
		return *this;
	}

	/// Set a set_flag action on the current node.
	DialogBuilder& SetFlag(const std::string& flag, const bool value = true) {
		auto& node = nodes_[current_id_];
		node.flag_name = flag;
		node.flag_value = value;
		node.type = "set_flag";
		return *this;
	}

	/// Add a child node. Sets it as a child of the current node and switches to it.
	/// Returns *this for chaining.
	DialogBuilder& Child(const std::string& child_id) {
		nodes_[current_id_].children.push_back(child_id);
		return Node(child_id);
	}

	/// Set the subject entity (for Move/Follow nodes). Stored as entity ID reference.
	DialogBuilder& SetSubject(const std::string& subject_id) {
		auto& node = nodes_[current_id_];
		node.subject_id = subject_id;
		return *this;
	}

	/// Set branch metadata (for children of Condition nodes).
	DialogBuilder& SetBranch(const bool on) {
		auto& node = nodes_[current_id_];
		node.branch_on = on;
		return *this;
	}

	/// Set a custom label for the node (e.g., for debugging/inspection).
	DialogBuilder& Label(const std::string& label) {
		auto& node = nodes_[current_id_];
		node.label = label;
		return *this;
	}

	/// Build the tree: materialize all stored nodes into ECS entities under root.
	/// Returns the root entity.
	flecs::entity Build(const flecs::world& world, const std::string& root_name, const flecs::entity parent) const {
		std::unordered_map<std::string, flecs::entity> id_to_entity;

		// Create root
		const auto root = world.entity(root_name.c_str()).child_of(parent).add<engine::graph::GraphRoot>();
		id_to_entity[root_id_] = root; // Empty string refers to root

		// First pass: create all entities
		for (const auto& [id, spec] : nodes_) {
			flecs::entity node_entity;
			if (!id_to_entity.contains(id) && id != root_id_) {
				node_entity = world.entity(id.c_str()).child_of(root);
				id_to_entity[id] = node_entity;
			}
			else {
				node_entity = id_to_entity[id];
			}

			node_entity.set<engine::graph::GraphNode>({.label = spec.label.empty() ? id : spec.label});

			// Set up parent-child relationships for the tree structure
			for (const auto& child_id : spec.children) {
				flecs::entity child_entity;
				if (!id_to_entity.contains(child_id)) {
					child_entity = world.entity(child_id.c_str());
					id_to_entity[child_id] = child_entity;
				}
				else {
					child_entity = id_to_entity[child_id];
				}
				child_entity.child_of(node_entity);
			}
		}

		// Second pass: attach components and edges
		for (const auto& [id, spec] : nodes_) {
			const auto node = id_to_entity[id];

			// Attach type-specific components
			node.set<DialogNode>({.type = spec.type});

			if (spec.type == "line") {
				node.set<DialogLine>({.speaker = spec.speaker, .text = spec.text});
			}
			else if (spec.type == "choice") {
				node.set<DialogChoice>(
					{.label = spec.choice_label, .requires_flag = spec.choice_requires_flag, .order = spec.choice_order}
				);
			}
			else if (spec.type == "end") {
				node.add<DialogEnd>();
			}
			else if (spec.type == "wait") {
				node.set<DialogWait>({.seconds = spec.wait_seconds});
			}
			else if (spec.type == "move") {
				node.set<DialogMove>(
					{.to = spec.move_to, .speed = spec.move_speed, .wait_for_arrival = spec.move_wait_for_arrival}
				);
				if (!spec.subject_id.empty() && id_to_entity.contains(spec.subject_id)) {
					node.add<DialogSubject>(id_to_entity[spec.subject_id]);
				}
			}
			else if (spec.type == "follow") {
				node.set<DialogFollow>({.speed = spec.follow_speed, .stop_distance = spec.follow_stop_distance});
				if (!spec.subject_id.empty() && id_to_entity.contains(spec.subject_id)) {
					node.add<DialogSubject>(id_to_entity[spec.subject_id]);
				}
			}
			else if (spec.type == "give_item" || spec.type == "take_item") {
				node.set<DialogGiveItem>({.item = spec.item_name, .count = spec.item_count});
			}
			else if (spec.type == "give_coins" || spec.type == "take_coins") {
				node.set<DialogGiveCoins>({.amount = spec.coin_amount});
			}
			else if (spec.type == "condition") {
				node.set<DialogCondition>({.flag = spec.condition_flag});
			}
			else if (spec.type == "set_flag") {
				node.set<DialogSetFlag>({.flag = spec.flag_name, .value = spec.flag_value});
			}

			// Set branch metadata if specified
			if (spec.branch_on.has_value()) {
				node.set<DialogBranch>({.on = spec.branch_on.value()});
			}

			// Create edges to children
			for (const auto& child_id : spec.children) {
				if (id_to_entity.contains(child_id)) {
					node.add<engine::graph::GraphEdge>(id_to_entity[child_id]);
				}
			}
		}

		return root;
	}

private:
	struct NodeSpec {
		std::string id;
		std::string label;
		std::string type;

		// Line
		std::string speaker;
		std::string text;

		// Choice
		std::string choice_label;
		std::string choice_requires_flag;
		int choice_order{0};

		// Wait
		float wait_seconds{1.0F};

		// Move
		glm::vec3 move_to{0.0F};
		float move_speed{4.0F};
		bool move_wait_for_arrival{true};

		// Follow
		float follow_speed{3.0F};
		float follow_stop_distance{1.0F};

		// Item/Coins
		std::string item_name;
		int item_count{1};
		int coin_amount{1};

		// Condition
		std::string condition_flag;

		// SetFlag
		std::string flag_name;
		bool flag_value{true};

		// Subject (entity reference by ID)
		std::string subject_id;

		// Branch
		std::optional<bool> branch_on;

		// Children
		std::vector<std::string> children;
	};

	std::unordered_map<std::string, NodeSpec> nodes_;
	std::string current_id_;
	std::string root_id_;
};

} // namespace engine::dialog
