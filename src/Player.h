#pragma once

#include <cmath>
#include "../engine/Engine.h"

using namespace yamaneko_engine;

enum class ActionType {
	Stay,
	MoveStraight,
	Shoot
};

class Player : public Module {
public:
	int team_id = 0;       // 0: Player 1, 1: Player 2
	int unit_index = 0;    // 0 .. 4
	Vector3 home_position = Vector3(0.0f);
	Vector3 default_direction = Vector3(1.0f, 0.0f, 0.0f);

	ActionType assigned_action = ActionType::Stay;
	Vector3 assigned_direction = Vector3(1.0f, 0.0f, 0.0f);
	bool action_confirmed = false;

	bool is_dribbling = false;
	bool has_kicked_this_turn = false;
	float kick_immunity_timer = 0.0f;
	float move_speed = 5.0f;

	Player(int team, int index, const Vector3& home_pos, const Vector3& default_dir)
		: team_id(team),
		  unit_index(index),
		  home_position(home_pos),
		  default_direction(glm::normalize(default_dir)),
		  assigned_direction(glm::normalize(default_dir)) {}

	void OnInitialize() override {
		if (!GetGameObject()) return;
		GetGameObject()->radius = 0.55f;
		GetTransform().scale = Vector3(0.35f, 0.35f, 0.35f);
		ResetToHome();
	}

	void ResetToHome() {
		if (!GetGameObject()) return;
		GetTransform().position = home_position;
		assigned_action = ActionType::Stay;
		action_confirmed = false;
		is_dribbling = false;
		has_kicked_this_turn = false;
		kick_immunity_timer = 0.0f;
		SetDirection(default_direction);
	}

	void BeginInstructionPhase() {
		action_confirmed = false;
		has_kicked_this_turn = false;
		kick_immunity_timer = 0.0f;
	}

	void SetDirection(const Vector3& dir) {
		Vector3 flat_dir(dir.x, 0.0f, dir.z);
		float len = glm::length(flat_dir);
		if (len > 0.0001f) {
			assigned_direction = flat_dir / len;
		}
		UpdateVisualRotation();
	}

	void SteerDirection(const Vector3& input_dir, float delta_time, bool snap_boost = false) {
		Vector3 flat_input(input_dir.x, 0.0f, input_dir.z);
		float len = glm::length(flat_input);
		if (len <= 0.0001f) return;

		Vector3 target = flat_input / len;
		float dot = std::clamp(Math::Dot(assigned_direction, target), -1.0f, 1.0f);
		if (dot < -0.75f) {
			// Opposite direction: snap directly to avoid hesitation
			SetDirection(target);
			return;
		}

		float rate = snap_boost ? 0.65f : (14.0f * delta_time);
		Vector3 blended = assigned_direction + target * rate;
		SetDirection(blended);
	}

	void UpdateVisualRotation() {
		if (!GetGameObject()) return;
		// airplane.obj nose points along local -Z
		float yaw = std::atan2(-assigned_direction.x, -assigned_direction.z);
		GetTransform().rotation = Vector3(0.0f, yaw, 0.0f);
	}

	void OnUpdate(float delta_time) override {
		if (kick_immunity_timer > 0.0f) {
			kick_immunity_timer -= delta_time;
			if (kick_immunity_timer < 0.0f) {
				kick_immunity_timer = 0.0f;
			}
		}
	}
};
