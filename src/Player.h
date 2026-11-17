#pragma once

#include "../engine/Engine.h"

using namespace yamaneko_engine;

/// <summary>
/// Command types a player can assign to each unit during the Instruction Phase.
/// </summary>
enum class ActionType {
	Stay,
	MoveStraight,
	Shoot
};


/// <summary>
/// Data structure representing the instruction assigned to a unit for the current turn.
/// </summary>
struct UnitCommand {
	ActionType action = ActionType::Stay;
	Vector3 direction = Vector3(1.0f, 0.0f, 0.0f);
	bool is_confirmed = false;
};


/// <summary>
/// Unit data structure and state module.
/// Manages unit identity, formation data, assigned turn commands, and possession state.
/// </summary>
class Player : public Module {
public:
	int team_id = 0;       // 0: Player 1, 1: Player 2
	int unit_index = 0;    // 0 .. 4
	Vector3 home_position = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 default_direction = Vector3(1.0f, 0.0f, 0.0f);

	UnitCommand command{};
	bool is_dribbling = false;

	Player(int team, int index, const Vector3& home_pos, const Vector3& default_dir)
		: team_id(team),
		  unit_index(index),
		  home_position(home_pos),
		  default_direction(glm::normalize(default_dir)) {
		command.direction = default_direction;
	}


	/// <summary>
	/// Called when the unit module is initialized. Resets the unit's position and command state for kickoff.
	/// </summary>
	void OnInitialize() override {
		ResetForKickoff();
	}


	/// <summary>
	/// Resets the unit's position to its home position and clears its command state for the kickoff phase.
	/// </summary>
	void ResetForKickoff() {
		if (GetGameObject()) {
			GetTransform().position = home_position;
		}
		command.action = ActionType::Stay;
		command.direction = default_direction;
		command.is_confirmed = false;
		is_dribbling = false;
	}

	
	/// <summary>
	/// Called at the start of the Instruction Phase to clear any previous command confirmation state.
	/// </summary>
	void BeginInstructionPhase() {
		command.is_confirmed = false;
	}


	/// <summary>
	/// Sets the unit's movement direction based on the input vector, normalizing it to ensure consistent speed.
	/// </summary>
	void SetDirection(const Vector3& input_dir) {
		Vector3 flat_dir(input_dir.x, 0.0f, input_dir.z);
		float len = glm::length(flat_dir);
		if (len > 0.0001f) {
			command.direction = flat_dir / len;
		}
	}


	/// <summary>
	/// Assigns the specified action to the unit's command and marks it as confirmed for the current turn.
	/// </summary>
	void AssignAction(ActionType action) {
		command.action = action;
		command.is_confirmed = true;
	}


	// =========================================================================
	// [GAMEPLAY PROGRAMMER HOOK]
	// Implement unit movement execution for Stay, MoveStraight, and Shoot here.
	// =========================================================================
	
	/// <summary>
	/// Executes the unit's movement action based on its assigned command and the elapsed time.
	/// </summary>
	void ExecuteMovementAction(float delta_time) {
		(void)delta_time;
		// TODO (Gameplay Programmer):
		// switch (command.action) {
		// case ActionType::Stay:
		//     break;
		// case ActionType::MoveStraight:
		//     GetTransform().position += command.direction * move_speed * delta_time;
		//     break;
		// case ActionType::Shoot:
		//     // Kick ball in command.direction if dribbling / in range
		//     break;
		// }
	}


	// =========================================================================
	// [VISUAL PROGRAMMER HOOK]
	// Implement unit orientation, animations, or visual state updates here.
	// =========================================================================

	/// <summary>
	/// Updates the unit's visual representation, including orientation to face the command direction and highlighting if active or dribbling.
	/// </summary>
	void UpdateVisuals(float delta_time) {
		(void)delta_time;
		// TODO (Visual Programmer):
		// - Update GetTransform().rotation to face command.direction.
		// - Highlight active or dribbling unit.
	}

	void OnUpdate(float delta_time) override {
		UpdateVisuals(delta_time);
	}
};
