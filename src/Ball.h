#pragma once

#include "../engine/Engine.h"
#include "Player.h"

using namespace yamaneko_engine;

/// <summary>
/// Ball data structure and state module.
/// Holds possession/dribble state and physics state data for the core game loop.
/// </summary>
class Ball : public Module {
public:
	Vector3 spawn_position = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 velocity = Vector3(0.0f, 0.0f, 0.0f);
	Player* holder = nullptr; // Pointer to the unit currently in dribble state (nullptr if loose)

	/// <summary>
	/// Called when the ball module is initialized. Resets the ball's position and state for kickoff.
	/// </summary>
	void OnInitialize() override {
		ResetBall();
	}


	/// <summary>
	/// Resets the ball's position to its spawn position, clears its velocity, and detaches it from any holder for the kickoff phase.
	/// </summary>
	void ResetBall() {
		ClearHolder();
		velocity = Vector3(0.0f, 0.0f, 0.0f);
		if (GetGameObject()) {
			GetTransform().position = spawn_position;
			GetTransform().rotation = Vector3(0.0f, 0.0f, 0.0f);
		}
	}


	/// <summary>
	/// Sets the holder of the ball to the specified unit, updating the dribble state accordingly.
	/// </summary>
	void SetHolder(Player* new_holder) {
		if (holder && holder != new_holder) {
			holder->is_dribbling = false;
		}
		holder = new_holder;
		if (holder) {
			holder->is_dribbling = true;
		}
	}


	/// <summary>
	/// Clears the current holder of the ball, resetting the dribble state and detaching it from any unit.
	/// </summary>
	void ClearHolder() {
		if (holder) {
			holder->is_dribbling = false;
			holder = nullptr;
		}
	}


	// =========================================================================
	// [GAMEPLAY PROGRAMMER HOOK]
	// Implement ball movement, friction, kicking, and dribble attachment here.
	// =========================================================================

	/// <summary>
	/// Updates the ball's gameplay mechanics, including position, velocity, and dribble attachment based on the elapsed time.
	/// </summary>
	void UpdateGameplayMechanics(float delta_time) {
		(void)delta_time;
		// TODO (Gameplay Programmer):
		// - If holder != nullptr, attach ball position to holder's dribble offset.
		// - If holder == nullptr, integrate velocity and apply turf damping.
	}
	

	// =========================================================================
	// [VISUAL PROGRAMMER HOOK]
	// Implement ball rotation, trail effects, or visual updates here.
	// =========================================================================

	/// <summary>
	/// Updates the ball's visual representation, including rotation based on velocity and any visual effects, based on the elapsed time.
	/// </summary>
	void UpdateVisuals(float delta_time) {
		(void)delta_time;
		// TODO (Visual Programmer):
		// - Rotate ball mesh based on velocity.
	}

	void OnUpdate(float delta_time) override {
		UpdateVisuals(delta_time);
	}
};
