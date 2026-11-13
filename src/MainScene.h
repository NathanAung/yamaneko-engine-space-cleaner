#pragma once

#include <vector>
#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <SDL2/SDL.h>
#include "../engine/scene/Scene.h"
#include "../engine/scene/Collision.h"
#include "../engine/platform/sdl/Window.h"
#include "../engine/platform/sdl/Input.h"
#include "../engine/core/math/Random.h"
#include "../engine/platform/sdl/Time.h"
#include "../engine/rhi/RHI.h"
#include "../engine/scene/module/Camera.h"
#include "Player.h"
#include "Debri.h"

using namespace yamaneko_engine;

enum class GamePhase {
	InstructionP1,
	InstructionP2,
	Movement,
	Resolution,
	GameOver
};

class MainScene : public Scene {
public:
	static constexpr int kUnitsPerTeam = 5;
	static constexpr int kWinScore = 3;

	MainScene() {
		RHIDevice* device = RHI::GetDevice();
		shader_ = device->CreateShader(
			"assets/shader/simple_vertex_shader.glsl",
			"assets/shader/simple_fragment_shader.glsl"
		);
		RHI::GetCommandList()->SetShader(shader_);

		// Camera setup looking at the XZ soccer pitch from an elevated angle
		auto camera = std::make_unique<GameObject>("Camera", "", shader_);
		camera->AddModule<Camera>();
		camera->transform.position = Vector3(0.0f, 21.0f, 9.5f);
		camera->transform.rotation = Vector3(Math::Radians(-66.0f), 0.0f, 0.0f);
		camera_obj_ = camera.get();
		AddGameObject(std::move(camera));

		// Team 1 (Left Side, attacks +X)
		const std::array<Vector3, kUnitsPerTeam> p1_positions = {
			Vector3(-10.0f, 0.0f,  0.0f), // GK
			Vector3(-6.5f,  0.0f, -3.5f), // Top Defender
			Vector3(-6.5f,  0.0f,  3.5f), // Bottom Defender
			Vector3(-2.5f,  0.0f, -2.2f), // Top Forward
			Vector3(-2.5f,  0.0f,  2.2f)  // Bottom Forward
		};

		for (int i = 0; i < kUnitsPerTeam; ++i) {
			auto unit_go = std::make_unique<GameObject>("Team1", "assets/model/airplane/airplane.obj", shader_);
			Player* unit_mod = unit_go->AddModule<Player>(0, i, p1_positions[i], Vector3(1.0f, 0.0f, 0.0f));
			team1_units_[i] = unit_mod;
			all_units_.push_back(unit_mod);
			AddGameObject(std::move(unit_go));
		}

		// Team 2 (Right Side, attacks -X)
		const std::array<Vector3, kUnitsPerTeam> p2_positions = {
			Vector3(10.0f, 0.0f,  0.0f),  // GK
			Vector3(6.5f,  0.0f, -3.5f),  // Top Defender
			Vector3(6.5f,  0.0f,  3.5f),  // Bottom Defender
			Vector3(2.5f,  0.0f, -2.2f),  // Top Forward
			Vector3(2.5f,  0.0f,  2.2f)   // Bottom Forward
		};

		for (int i = 0; i < kUnitsPerTeam; ++i) {
			auto unit_go = std::make_unique<GameObject>("Team2", "assets/model/airplane/airplane.obj", shader_);
			Player* unit_mod = unit_go->AddModule<Player>(1, i, p2_positions[i], Vector3(-1.0f, 0.0f, 0.0f));
			team2_units_[i] = unit_mod;
			all_units_.push_back(unit_mod);
			AddGameObject(std::move(unit_go));
		}

		// Soccer Ball using debri.obj
		auto ball_go = std::make_unique<GameObject>("Ball", "assets/model/debri/debri.obj", shader_);
		ball_ = ball_go->AddModule<Debri>();
		ball_obj_ = ball_go.get();
		AddGameObject(std::move(ball_go));

		StartInstructionPhaseP1();
	}

	void OnEnter() override {}
	void OnExit() override {}

	void OnUpdate(float deltaTime) override {
		float dt = std::min(deltaTime, 0.05f);

		switch (phase_) {
		case GamePhase::InstructionP1:
			UpdateInstructionP1(dt);
			break;
		case GamePhase::InstructionP2:
			UpdateInstructionP2(dt);
			break;
		case GamePhase::Movement:
			UpdateMovementPhase(dt);
			break;
		case GamePhase::Resolution:
			UpdateResolutionPhase(dt);
			break;
		case GamePhase::GameOver:
			UpdateGameOverPhase();
			break;
		}
	}

	void OnRender() override {}

	void OnUIRender() override {
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 screen_size = io.DisplaySize;

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(screen_size, ImGuiCond_Always);

		if (ImGui::Begin("Soccer HUD & Field Overlay", nullptr, canvas_flags_)) {
			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			DrawFieldLines(draw_list, screen_size);
			DrawUnitIndicatorsAndVectors(draw_list, screen_size);
			DrawBallIndicator(draw_list, screen_size);
			DrawHUD(draw_list, screen_size);
		}
		ImGui::End();
	}

private:
	// Field Dimensions on the XZ plane
	const float field_half_w_ = 12.0f;
	const float field_half_h_ = 7.0f;
	const float goal_half_w_ = 2.5f;
	const float goal_depth_ = 1.2f;

	// State Machine
	GamePhase phase_ = GamePhase::InstructionP1;
	int active_unit_index_ = 0;
	int turn_number_ = 1;

	int p1_score_ = 0;
	int p2_score_ = 0;
	int last_scored_team_ = -1; // -1: none, 0: P1, 1: P2

	float movement_timer_ = 0.0f;
	const float movement_duration_ = 2.0f;
	float resolution_timer_ = 0.0f;
	const float goal_banner_duration_ = 1.4f;

	std::string last_event_text_ = "Kickoff! Player 1: Assign actions to your 5 units.";

	// Scene Objects
	RHIShader* shader_ = nullptr;
	GameObject* camera_obj_ = nullptr;
	GameObject* ball_obj_ = nullptr;
	Debri* ball_ = nullptr;
	std::array<Player*, kUnitsPerTeam> team1_units_{};
	std::array<Player*, kUnitsPerTeam> team2_units_{};
	std::vector<Player*> all_units_;

	ImGuiWindowFlags canvas_flags_ = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoBackground
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoNav
		| ImGuiWindowFlags_NoInputs;

	// --- Phase Transitions ---

	void StartInstructionPhaseP1() {
		phase_ = GamePhase::InstructionP1;
		active_unit_index_ = 0;
		for (auto* unit : all_units_) {
			unit->BeginInstructionPhase();
		}
		if (ball_ && ball_->holder) {
			ball_->UpdateDribblePosition();
		}
	}

	void StartInstructionPhaseP2() {
		phase_ = GamePhase::InstructionP2;
		active_unit_index_ = 0;
		last_event_text_ = "Player 2: Assign actions to your 5 units.";
	}

	void StartMovementPhase() {
		phase_ = GamePhase::Movement;
		movement_timer_ = 0.0f;
		last_scored_team_ = -1;
		last_event_text_ = "Movement Phase: Executing simultaneous actions!";

		// Execute immediate kicks for units holding or touching the ball with Shoot assigned
		for (auto* unit : all_units_) {
			if (unit->assigned_action == ActionType::Shoot && !unit->has_kicked_this_turn) {
				if (ball_->holder == unit) {
					ball_->Kick(unit->assigned_direction, unit);
					last_event_text_ = (unit->team_id == 0 ? "P1-" : "P2-") + std::to_string(unit->unit_index + 1) + " shoots/passes the ball!";
				} else if (ball_->holder == nullptr && Physics::CheckSphereCollision(*unit->GetGameObject(), *ball_obj_)) {
					ball_->Kick(unit->assigned_direction, unit);
					last_event_text_ = (unit->team_id == 0 ? "P1-" : "P2-") + std::to_string(unit->unit_index + 1) + " kicks the loose ball!";
				}
			}
		}
	}

	void StartResolutionPhase(int scored_team) {
		phase_ = GamePhase::Resolution;
		last_scored_team_ = scored_team;
		resolution_timer_ = 0.0f;

		if (scored_team == 0) {
			p1_score_++;
			last_event_text_ = "GOAL! Player 1 scores a point!";
		} else if (scored_team == 1) {
			p2_score_++;
			last_event_text_ = "GOAL! Player 2 scores a point!";
		} else {
			last_event_text_ = "Turn resolved. Returning to Instruction Phase.";
		}
	}

	void ResetPositionsForKickoff() {
		if (ball_) {
			ball_->ResetBall();
		}
		for (auto* unit : all_units_) {
			unit->ResetToHome();
		}
	}

	void RestartMatch() {
		p1_score_ = 0;
		p2_score_ = 0;
		turn_number_ = 1;
		last_scored_team_ = -1;
		ResetPositionsForKickoff();
		last_event_text_ = "New Match Started! First to 3 points wins.";
		StartInstructionPhaseP1();
	}

	// --- Instruction Phase Logic ---

	void AdvanceOrFinishTeamInput(const std::array<Player*, kUnitsPerTeam>& units, bool is_player_1) {
		// Check if all 5 units have confirmed actions
		bool all_confirmed = true;
		for (int i = 0; i < kUnitsPerTeam; ++i) {
			if (!units[i]->action_confirmed) {
				all_confirmed = false;
				break;
			}
		}

		if (all_confirmed) {
			if (is_player_1) {
				StartInstructionPhaseP2();
			} else {
				StartMovementPhase();
			}
			return;
		}

		// Move selection to the next unconfirmed unit
		for (int offset = 1; offset <= kUnitsPerTeam; ++offset) {
			int next_idx = (active_unit_index_ + offset) % kUnitsPerTeam;
			if (!units[next_idx]->action_confirmed) {
				active_unit_index_ = next_idx;
				return;
			}
		}
	}

	void UpdateInstructionP1(float dt) {
		// Switch Active Unit: Tab
		if (Input::GetKeyDown(SDLK_TAB)) {
			active_unit_index_ = (active_unit_index_ + 1) % kUnitsPerTeam;
		}

		Player* active_unit = team1_units_[active_unit_index_];

		// Set Direction: W, A, S, D
		Vector3 input_dir(0.0f);
		if (Input::GetKey(SDL_SCANCODE_W)) input_dir.z -= 1.0f;
		if (Input::GetKey(SDL_SCANCODE_S)) input_dir.z += 1.0f;
		if (Input::GetKey(SDL_SCANCODE_A)) input_dir.x -= 1.0f;
		if (Input::GetKey(SDL_SCANCODE_D)) input_dir.x += 1.0f;

		bool key_just_pressed = Input::GetKeyDown(SDLK_w) || Input::GetKeyDown(SDLK_s) ||
			Input::GetKeyDown(SDLK_a) || Input::GetKeyDown(SDLK_d);

		if (glm::length(input_dir) > 0.01f) {
			active_unit->SteerDirection(input_dir, dt, key_just_pressed);
			if (ball_ && ball_->holder == active_unit) {
				ball_->UpdateDribblePosition();
			}
		}

		// Action - Stay: Z
		if (Input::GetKeyDown(SDLK_z)) {
			active_unit->assigned_action = ActionType::Stay;
			active_unit->action_confirmed = true;
			AdvanceOrFinishTeamInput(team1_units_, true);
			return;
		}

		// Action - Move Straight: X
		if (Input::GetKeyDown(SDLK_x)) {
			active_unit->assigned_action = ActionType::MoveStraight;
			active_unit->action_confirmed = true;
			AdvanceOrFinishTeamInput(team1_units_, true);
			return;
		}

		// Action - Shoot: C
		if (Input::GetKeyDown(SDLK_c)) {
			active_unit->assigned_action = ActionType::Shoot;
			active_unit->action_confirmed = true;
			AdvanceOrFinishTeamInput(team1_units_, true);
			return;
		}
	}

	void UpdateInstructionP2(float dt) {
		// Switch Active Unit: Right Shift
		if (Input::GetKeyDown(SDLK_RSHIFT)) {
			active_unit_index_ = (active_unit_index_ + 1) % kUnitsPerTeam;
		}

		Player* active_unit = team2_units_[active_unit_index_];

		// Set Direction: Arrow Keys
		Vector3 input_dir(0.0f);
		if (Input::GetKey(SDL_SCANCODE_UP))    input_dir.z -= 1.0f;
		if (Input::GetKey(SDL_SCANCODE_DOWN))  input_dir.z += 1.0f;
		if (Input::GetKey(SDL_SCANCODE_LEFT))  input_dir.x -= 1.0f;
		if (Input::GetKey(SDL_SCANCODE_RIGHT)) input_dir.x += 1.0f;

		bool key_just_pressed = Input::GetKeyDown(SDLK_UP) || Input::GetKeyDown(SDLK_DOWN) ||
			Input::GetKeyDown(SDLK_LEFT) || Input::GetKeyDown(SDLK_RIGHT);

		if (glm::length(input_dir) > 0.01f) {
			active_unit->SteerDirection(input_dir, dt, key_just_pressed);
			if (ball_ && ball_->holder == active_unit) {
				ball_->UpdateDribblePosition();
			}
		}

		// Action - Stay: N
		if (Input::GetKeyDown(SDLK_n)) {
			active_unit->assigned_action = ActionType::Stay;
			active_unit->action_confirmed = true;
			AdvanceOrFinishTeamInput(team2_units_, false);
			return;
		}

		// Action - Move Straight: M
		if (Input::GetKeyDown(SDLK_m)) {
			active_unit->assigned_action = ActionType::MoveStraight;
			active_unit->action_confirmed = true;
			AdvanceOrFinishTeamInput(team2_units_, false);
			return;
		}

		// Action - Shoot: , (Comma)
		if (Input::GetKeyDown(SDLK_COMMA)) {
			active_unit->assigned_action = ActionType::Shoot;
			active_unit->action_confirmed = true;
			AdvanceOrFinishTeamInput(team2_units_, false);
			return;
		}
	}

	// --- Movement Phase Physics Simulation ---

	void UpdateMovementPhase(float dt) {
		const int kSubSteps = 4;
		float sub_dt = dt / static_cast<float>(kSubSteps);

		for (int step = 0; step < kSubSteps; ++step) {
			// 1. Move all 10 units according to their assigned action
			for (auto* unit : all_units_) {
				Transform& t = unit->GetTransform();
				if (unit->assigned_action == ActionType::MoveStraight) {
					t.position += unit->assigned_direction * (unit->move_speed * sub_dt);
				} else if (unit->assigned_action == ActionType::Shoot && !unit->has_kicked_this_turn && ball_->holder != unit) {
					// If ball is loose and nearby, approach it to kick; otherwise advance slightly along aim vector
					Vector3 to_ball = ball_obj_->transform.position - t.position;
					to_ball.y = 0.0f;
					float dist_to_ball = glm::length(to_ball);
					if (ball_->holder == nullptr && dist_to_ball > 0.001f && dist_to_ball < 3.0f) {
						t.position += (to_ball / dist_to_ball) * (unit->move_speed * 0.85f * sub_dt);
					} else {
						t.position += unit->assigned_direction * (unit->move_speed * 0.35f * sub_dt);
					}
				}
				// ActionType::Stay remains stationary

				// Clamp unit within field boundaries
				t.position.x = std::clamp(t.position.x, -field_half_w_ + 0.5f, field_half_w_ - 0.5f);
				t.position.z = std::clamp(t.position.z, -field_half_h_ + 0.5f, field_half_h_ - 0.5f);
				t.position.y = 0.0f;
			}

			// 2. Handle unit-vs-unit collisions and dribble turnovers (steals)
			for (size_t i = 0; i < all_units_.size(); ++i) {
				for (size_t j = i + 1; j < all_units_.size(); ++j) {
					Player* u1 = all_units_[i];
					Player* u2 = all_units_[j];
					GameObject* go1 = u1->GetGameObject();
					GameObject* go2 = u2->GetGameObject();

					if (Physics::CheckSphereCollision(*go1, *go2)) {
						// Check if an opponent is tackling the current ball carrier
						if (u1->team_id != u2->team_id && ball_->steal_cooldown <= 0.0f) {
							if (ball_->holder == u1) {
								ball_->AttachToUnit(u2);
								ball_->steal_cooldown = 0.35f;
								last_event_text_ = (u2->team_id == 0 ? "P1-" : "P2-") + std::to_string(u2->unit_index + 1) +
									" stole the ball from " + (u1->team_id == 0 ? "P1-" : "P2-") + std::to_string(u1->unit_index + 1) + "!";
							} else if (ball_->holder == u2) {
								ball_->AttachToUnit(u1);
								ball_->steal_cooldown = 0.35f;
								last_event_text_ = (u1->team_id == 0 ? "P1-" : "P2-") + std::to_string(u1->unit_index + 1) +
									" stole the ball from " + (u2->team_id == 0 ? "P1-" : "P2-") + std::to_string(u2->unit_index + 1) + "!";
							}
						}

						// Gently separate overlapping units
						Vector3 diff = go2->transform.position - go1->transform.position;
						diff.y = 0.0f;
						float dist = glm::length(diff);
						float min_dist = go1->radius + go2->radius;
						if (dist < min_dist) {
							Vector3 normal = (dist > 0.0001f) ? (diff / dist) : Vector3(1.0f, 0.0f, 0.0f);
							float overlap = (min_dist - dist) * 0.5f;
							go1->transform.position -= normal * overlap;
							go2->transform.position += normal * overlap;
						}
					}
				}
			}

			// 3. Update Ball Physics & Trajectory Interceptions
			if (ball_->holder != nullptr) {
				Player* carrier = ball_->holder;
				ball_->UpdateDribblePosition();

				// Also check if an opponent directly collides with the dribbled ball sphere
				if (ball_->steal_cooldown <= 0.0f) {
					for (auto* unit : all_units_) {
						if (unit->team_id != carrier->team_id && Physics::CheckSphereCollision(*unit->GetGameObject(), *ball_obj_)) {
							ball_->AttachToUnit(unit);
							ball_->steal_cooldown = 0.35f;
							last_event_text_ = (unit->team_id == 0 ? "P1-" : "P2-") + std::to_string(unit->unit_index + 1) + " tackled and stole the ball!";
							carrier = unit;
							break;
						}
					}
				}

				// If the carrier has Shoot assigned and hasn't kicked yet, kick now
				if (carrier && carrier->assigned_action == ActionType::Shoot && !carrier->has_kicked_this_turn) {
					ball_->Kick(carrier->assigned_direction, carrier);
					last_event_text_ = (carrier->team_id == 0 ? "P1-" : "P2-") + std::to_string(carrier->unit_index + 1) + " shoots/passes!";
				}
			} else {
				// Free ball moving along shot/pass trajectory
				Transform& bt = ball_obj_->transform;
				bt.position += ball_->velocity * sub_dt;
				bt.position.y = 0.0f;

				// Spin visual rotation when moving
				float speed = glm::length(ball_->velocity);
				if (speed > 0.05f) {
					bt.rotation.y += speed * sub_dt * 1.5f;
					bt.rotation.x += speed * sub_dt * 1.0f;
					float damping = std::max(0.0f, 1.0f - ball_->friction * sub_dt);
					ball_->velocity *= damping;
				} else {
					ball_->velocity = Vector3(0.0f);
				}

				// Check trajectory intersection / collision with any unit
				for (auto* unit : all_units_) {
					if (unit->kick_immunity_timer > 0.0f) continue;

					if (Physics::CheckSphereCollision(*unit->GetGameObject(), *ball_obj_)) {
						ball_->AttachToUnit(unit);
						ball_->steal_cooldown = 0.25f;

						if (unit->assigned_action == ActionType::Shoot && !unit->has_kicked_this_turn) {
							ball_->Kick(unit->assigned_direction, unit);
							last_event_text_ = (unit->team_id == 0 ? "P1-" : "P2-") + std::to_string(unit->unit_index + 1) + " one-touch shoots/passes!";
						} else {
							last_event_text_ = (unit->team_id == 0 ? "P1-" : "P2-") + std::to_string(unit->unit_index + 1) + " gained possession (Dribble)!";
						}
						break;
					}
				}
			}

			// 4. Goal Detection & Field Wall Bounces for Ball
			Transform& bt = ball_obj_->transform;

			// Top / Bottom sidelines (Z = +/- field_half_h_)
			if (bt.position.z < -field_half_h_ + ball_obj_->radius) {
				bt.position.z = -field_half_h_ + ball_obj_->radius;
				if (ball_->holder == nullptr && ball_->velocity.z < 0.0f) {
					ball_->velocity.z = -ball_->velocity.z * 0.75f;
				}
			} else if (bt.position.z > field_half_h_ - ball_obj_->radius) {
				bt.position.z = field_half_h_ - ball_obj_->radius;
				if (ball_->holder == nullptr && ball_->velocity.z > 0.0f) {
					ball_->velocity.z = -ball_->velocity.z * 0.75f;
				}
			}

			// Right Goal Line (X = +field_half_w_) -> Player 1 scores!
			if (bt.position.x >= field_half_w_ - 0.15f) {
				if (std::abs(bt.position.z) <= goal_half_w_) {
					StartResolutionPhase(0);
					return;
				} else {
					bt.position.x = field_half_w_ - ball_obj_->radius;
					if (ball_->holder == nullptr && ball_->velocity.x > 0.0f) {
						ball_->velocity.x = -ball_->velocity.x * 0.75f;
					}
				}
			}

			// Left Goal Line (X = -field_half_w_) -> Player 2 scores!
			if (bt.position.x <= -field_half_w_ + 0.15f) {
				if (std::abs(bt.position.z) <= goal_half_w_) {
					StartResolutionPhase(1);
					return;
				} else {
					bt.position.x = -field_half_w_ + ball_obj_->radius;
					if (ball_->holder == nullptr && ball_->velocity.x < 0.0f) {
						ball_->velocity.x = -ball_->velocity.x * 0.75f;
					}
				}
			}
		}

		movement_timer_ += dt;
		if (movement_timer_ >= movement_duration_) {
			StartResolutionPhase(-1);
		}
	}

	// --- Resolution & Game Over Phases ---

	void UpdateResolutionPhase(float dt) {
		resolution_timer_ += dt;

		if (last_scored_team_ != -1) {
			// Wait briefly so players can see the goal banner, then reset ball & units
			if (resolution_timer_ >= goal_banner_duration_) {
				if (p1_score_ >= kWinScore || p2_score_ >= kWinScore) {
					phase_ = GamePhase::GameOver;
					return;
				}
				ResetPositionsForKickoff();
				turn_number_++;
				StartInstructionPhaseP1();
			}
		} else {
			// No goal scored: immediately proceed to the next turn's Instruction Phase
			turn_number_++;
			StartInstructionPhaseP1();
		}
	}

	void UpdateGameOverPhase() {
		// Allow restarting match with Space, Return, Z, or N
		if (Input::GetKeyDown(SDLK_SPACE) || Input::GetKeyDown(SDLK_RETURN) ||
			Input::GetKeyDown(SDLK_z) || Input::GetKeyDown(SDLK_n)) {
			RestartMatch();
		}
	}

	// --- 3D-to-2D Projection & UI Rendering Helpers ---

	bool WorldToScreen(const Vector3& world_pos, const ImVec2& screen_size, ImVec2& out_screen) const {
		if (! ViewInfo::current || screen_size.x <= 0.0f || screen_size.y <= 0.0f) return false;

		Matrix4 view = ViewInfo::current->GetViewMatrix();
		Matrix4 proj = ViewInfo::current->GetProjectionMatrix();
		Vector4 clip = proj * view * Vector4(world_pos, 1.0f);
		if (clip.w <= 0.001f) return false;

		Vector3 ndc = Vector3(clip) / clip.w;
		out_screen.x = (ndc.x * 0.5f + 0.5f) * screen_size.x;
		out_screen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * screen_size.y;
		return true;
	}

	void DrawWorldLine(ImDrawList* draw_list, const Vector3& a, const Vector3& b, const ImVec2& screen_size, ImU32 color, float thickness = 2.0f) const {
		ImVec2 sa, sb;
		if (WorldToScreen(a, screen_size, sa) && WorldToScreen(b, screen_size, sb)) {
			draw_list->AddLine(sa, sb, color, thickness);
		}
	}

	void DrawWorldArrow(ImDrawList* draw_list, const Vector3& start, const Vector3& end, const ImVec2& screen_size, ImU32 color, float thickness = 3.0f) const {
		ImVec2 s_start, s_end;
		if (!WorldToScreen(start, screen_size, s_start) || !WorldToScreen(end, screen_size, s_end)) return;

		draw_list->AddLine(s_start, s_end, color, thickness);

		ImVec2 dir(s_end.x - s_start.x, s_end.y - s_start.y);
		float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
		if (len > 4.0f) {
			dir.x /= len;
			dir.y /= len;
			ImVec2 perp(-dir.y, dir.x);
			float head_len = 11.0f;
			float head_w = 6.0f;
			ImVec2 p1(s_end.x - dir.x * head_len + perp.x * head_w, s_end.y - dir.y * head_len + perp.y * head_w);
			ImVec2 p2(s_end.x - dir.x * head_len - perp.x * head_w, s_end.y - dir.y * head_len - perp.y * head_w);
			draw_list->AddTriangleFilled(s_end, p1, p2, color);
		}
	}

	void DrawWorldCircleXZ(ImDrawList* draw_list, const Vector3& center, float radius, const ImVec2& screen_size, ImU32 color, int segments = 28, float thickness = 2.0f) const {
		ImVec2 prev;
		bool has_prev = false;
		for (int i = 0; i <= segments; ++i) {
			float angle = (static_cast<float>(i) / static_cast<float>(segments)) * 6.2831853f;
			Vector3 pt(center.x + std::cos(angle) * radius, center.y, center.z + std::sin(angle) * radius);
			ImVec2 sp;
			if (WorldToScreen(pt, screen_size, sp)) {
				if (has_prev) {
					draw_list->AddLine(prev, sp, color, thickness);
				}
				prev = sp;
				has_prev = true;
			}
		}
	}

	void DrawFieldLines(ImDrawList* draw_list, const ImVec2& screen_size) const {
		const ImU32 line_col = IM_COL32(220, 240, 220, 140);
		const ImU32 p1_goal_col = IM_COL32(70, 160, 255, 220);
		const ImU32 p2_goal_col = IM_COL32(255, 95, 85, 220);

		// Pitch outer boundary
		Vector3 tl(-field_half_w_, 0.0f, -field_half_h_);
		Vector3 tr( field_half_w_, 0.0f, -field_half_h_);
		Vector3 br( field_half_w_, 0.0f,  field_half_h_);
		Vector3 bl(-field_half_w_, 0.0f,  field_half_h_);

		DrawWorldLine(draw_list, tl, tr, screen_size, line_col, 2.5f);
		DrawWorldLine(draw_list, tr, br, screen_size, line_col, 2.5f);
		DrawWorldLine(draw_list, br, bl, screen_size, line_col, 2.5f);
		DrawWorldLine(draw_list, bl, tl, screen_size, line_col, 2.5f);

		// Halfway line & center circle
		DrawWorldLine(draw_list, Vector3(0.0f, 0.0f, -field_half_h_), Vector3(0.0f, 0.0f, field_half_h_), screen_size, line_col, 2.0f);
		DrawWorldCircleXZ(draw_list, Vector3(0.0f, 0.0f, 0.0f), 2.2f, screen_size, line_col, 32, 2.0f);

		// Penalty boxes
		float pen_w = 3.2f;
		float pen_h = 4.2f;
		// Left penalty box
		DrawWorldLine(draw_list, Vector3(-field_half_w_, 0.0f, -pen_h), Vector3(-field_half_w_ + pen_w, 0.0f, -pen_h), screen_size, line_col, 1.8f);
		DrawWorldLine(draw_list, Vector3(-field_half_w_ + pen_w, 0.0f, -pen_h), Vector3(-field_half_w_ + pen_w, 0.0f, pen_h), screen_size, line_col, 1.8f);
		DrawWorldLine(draw_list, Vector3(-field_half_w_ + pen_w, 0.0f, pen_h), Vector3(-field_half_w_, 0.0f, pen_h), screen_size, line_col, 1.8f);
		// Right penalty box
		DrawWorldLine(draw_list, Vector3(field_half_w_, 0.0f, -pen_h), Vector3(field_half_w_ - pen_w, 0.0f, -pen_h), screen_size, line_col, 1.8f);
		DrawWorldLine(draw_list, Vector3(field_half_w_ - pen_w, 0.0f, -pen_h), Vector3(field_half_w_ - pen_w, 0.0f, pen_h), screen_size, line_col, 1.8f);
		DrawWorldLine(draw_list, Vector3(field_half_w_ - pen_w, 0.0f, pen_h), Vector3(field_half_w_, 0.0f, pen_h), screen_size, line_col, 1.8f);

		// Left Goal (Defended by P1)
		DrawWorldLine(draw_list, Vector3(-field_half_w_, 0.0f, -goal_half_w_), Vector3(-field_half_w_ - goal_depth_, 0.0f, -goal_half_w_), screen_size, p1_goal_col, 3.5f);
		DrawWorldLine(draw_list, Vector3(-field_half_w_ - goal_depth_, 0.0f, -goal_half_w_), Vector3(-field_half_w_ - goal_depth_, 0.0f, goal_half_w_), screen_size, p1_goal_col, 3.5f);
		DrawWorldLine(draw_list, Vector3(-field_half_w_ - goal_depth_, 0.0f, goal_half_w_), Vector3(-field_half_w_, 0.0f, goal_half_w_), screen_size, p1_goal_col, 3.5f);

		// Right Goal (Defended by P2)
		DrawWorldLine(draw_list, Vector3(field_half_w_, 0.0f, -goal_half_w_), Vector3(field_half_w_ + goal_depth_, 0.0f, -goal_half_w_), screen_size, p2_goal_col, 3.5f);
		DrawWorldLine(draw_list, Vector3(field_half_w_ + goal_depth_, 0.0f, -goal_half_w_), Vector3(field_half_w_ + goal_depth_, 0.0f, goal_half_w_), screen_size, p2_goal_col, 3.5f);
		DrawWorldLine(draw_list, Vector3(field_half_w_ + goal_depth_, 0.0f, goal_half_w_), Vector3(field_half_w_, 0.0f, goal_half_w_), screen_size, p2_goal_col, 3.5f);
	}

	static const char* ActionToShortStr(ActionType action) {
		switch (action) {
		case ActionType::Stay:         return "STAY";
		case ActionType::MoveStraight: return "MOVE";
		case ActionType::Shoot:        return "SHOOT";
		}
		return "";
	}

	void DrawUnitIndicatorsAndVectors(ImDrawList* draw_list, const ImVec2& screen_size) const {
		for (const auto* unit : all_units_) {
			const Vector3& pos = unit->GetTransform().position;
			bool is_p1 = (unit->team_id == 0);
			bool is_active_selection =
				(phase_ == GamePhase::InstructionP1 && is_p1 && unit->unit_index == active_unit_index_) ||
				(phase_ == GamePhase::InstructionP2 && !is_p1 && unit->unit_index == active_unit_index_);

			ImU32 team_col = is_p1 ? IM_COL32(60, 165, 255, 230) : IM_COL32(255, 85, 75, 230);

			// Team ring around unit base
			DrawWorldCircleXZ(draw_list, pos, unit->GetGameObject()->radius, screen_size, team_col, 24, 2.5f);

			// Dribble indicator ring
			if (unit->is_dribbling) {
				DrawWorldCircleXZ(draw_list, pos, unit->GetGameObject()->radius + 0.22f, screen_size, IM_COL32(255, 235, 60, 255), 24, 3.0f);
			}

			// Active unit selection highlight during Instruction Phase
			if (is_active_selection) {
				DrawWorldCircleXZ(draw_list, pos, unit->GetGameObject()->radius + 0.35f, screen_size, IM_COL32(255, 255, 100, 255), 28, 3.5f);
			}

			// Show direction/action vector when appropriate:
			// - In InstructionP1: show P1 units
			// - In InstructionP2: show P2 units (and P1 confirmed status)
			// - In Movement: show all units
			bool show_vector =
				(phase_ == GamePhase::Movement) ||
				(phase_ == GamePhase::InstructionP1 && is_p1) ||
				(phase_ == GamePhase::InstructionP2 && !is_p1);

			if (show_vector) {
				if (unit->assigned_action == ActionType::MoveStraight) {
					DrawWorldArrow(draw_list, pos, pos + unit->assigned_direction * 2.4f, screen_size, IM_COL32(80, 255, 130, 230), 3.0f);
				} else if (unit->assigned_action == ActionType::Shoot) {
					DrawWorldArrow(draw_list, pos, pos + unit->assigned_direction * 3.5f, screen_size, IM_COL32(255, 215, 50, 245), 3.5f);
				} else {
					// Stay: still show a short facing indicator arrow so the player can see direction while aiming with WASD / Arrows
					DrawWorldArrow(draw_list, pos, pos + unit->assigned_direction * 1.4f, screen_size, IM_COL32(190, 200, 215, 190), 2.0f);
				}
			}

			// Unit label above/below unit in screen space
			ImVec2 sp;
			if (WorldToScreen(pos + Vector3(0.0f, 0.8f, 0.0f), screen_size, sp)) {
				char label[64];
				if ((phase_ == GamePhase::InstructionP1 && is_p1) || (phase_ == GamePhase::InstructionP2 && !is_p1)) {
					if (unit->action_confirmed) {
						std::snprintf(label, sizeof(label), "P%d-%d [%s]", unit->team_id + 1, unit->unit_index + 1, ActionToShortStr(unit->assigned_action));
					} else if (is_active_selection) {
						std::snprintf(label, sizeof(label), "> P%d-%d <", unit->team_id + 1, unit->unit_index + 1);
					} else {
						std::snprintf(label, sizeof(label), "P%d-%d", unit->team_id + 1, unit->unit_index + 1);
					}
				} else {
					std::snprintf(label, sizeof(label), "P%d-%d", unit->team_id + 1, unit->unit_index + 1);
				}

				ImVec2 text_sz = ImGui::CalcTextSize(label);
				ImVec2 text_pos(sp.x - text_sz.x * 0.5f, sp.y - 22.0f);
				draw_list->AddRectFilled(
					ImVec2(text_pos.x - 4.0f, text_pos.y - 2.0f),
					ImVec2(text_pos.x + text_sz.x + 4.0f, text_pos.y + text_sz.y + 2.0f),
					IM_COL32(15, 20, 28, 190),
					4.0f
				);
				ImU32 label_col = is_active_selection ? IM_COL32(255, 255, 100, 255) : team_col;
				draw_list->AddText(text_pos, label_col, label);
			}
		}
	}

	void DrawBallIndicator(ImDrawList* draw_list, const ImVec2& screen_size) const {
		if (!ball_obj_) return;
		const Vector3& bpos = ball_obj_->transform.position;
		DrawWorldCircleXZ(draw_list, bpos, ball_obj_->radius + 0.1f, screen_size, IM_COL32(255, 255, 255, 240), 20, 2.2f);

		ImVec2 sp;
		if (WorldToScreen(bpos + Vector3(0.0f, 0.9f, 0.0f), screen_size, sp)) {
			const char* ball_lbl = (ball_ && ball_->holder) ? "BALL (DRIBBLE)" : "BALL";
			ImVec2 sz = ImGui::CalcTextSize(ball_lbl);
			draw_list->AddText(ImVec2(sp.x - sz.x * 0.5f, sp.y - 16.0f), IM_COL32(255, 255, 255, 230), ball_lbl);
		}
	}

	void DrawHUD(ImDrawList* draw_list, const ImVec2& screen_size) const {
		// Top Scoreboard Panel
		float top_w = 560.0f;
		float top_x = (screen_size.x - top_w) * 0.5f;
		draw_list->AddRectFilled(
			ImVec2(top_x, 10.0f),
			ImVec2(top_x + top_w, 82.0f),
			IM_COL32(15, 22, 32, 220),
			8.0f
		);
		draw_list->AddRect(
			ImVec2(top_x, 10.0f),
			ImVec2(top_x + top_w, 82.0f),
			IM_COL32(100, 130, 165, 180),
			8.0f, 0, 1.5f
		);

		ImGui::SetWindowFontScale(2.0f);
		char score_buf[128];
		std::snprintf(score_buf, sizeof(score_buf), "PLAYER 1   %d  -  %d   PLAYER 2", p1_score_, p2_score_);
		ImVec2 score_sz = ImGui::CalcTextSize(score_buf);
		ImGui::SetCursorPos(ImVec2((screen_size.x - score_sz.x) * 0.5f, 16.0f));
		ImGui::TextColored(ImVec4(1.0f, 0.96f, 0.80f, 1.0f), "%s", score_buf);

		ImGui::SetWindowFontScale(1.2f);
		const char* phase_str = "";
		ImVec4 phase_color(0.8f, 0.9f, 1.0f, 1.0f);
		if (phase_ == GamePhase::InstructionP1) {
			phase_str = "PHASE: PLAYER 1 INSTRUCTION (Select actions for all 5 units)";
			phase_color = ImVec4(0.35f, 0.75f, 1.0f, 1.0f);
		} else if (phase_ == GamePhase::InstructionP2) {
			phase_str = "PHASE: PLAYER 2 INSTRUCTION (Select actions for all 5 units)";
			phase_color = ImVec4(1.0f, 0.45f, 0.40f, 1.0f);
		} else if (phase_ == GamePhase::Movement) {
			phase_str = "PHASE: MOVEMENT & PHYSICS SIMULATION";
			phase_color = ImVec4(0.45f, 1.0f, 0.55f, 1.0f);
		} else if (phase_ == GamePhase::Resolution) {
			phase_str = "PHASE: RESOLUTION";
			phase_color = ImVec4(1.0f, 0.90f, 0.35f, 1.0f);
		} else if (phase_ == GamePhase::GameOver) {
			phase_str = "MATCH COMPLETE (First to 3 Points)";
			phase_color = ImVec4(1.0f, 0.85f, 0.25f, 1.0f);
		}

		ImVec2 phase_sz = ImGui::CalcTextSize(phase_str);
		ImGui::SetCursorPos(ImVec2((screen_size.x - phase_sz.x) * 0.5f, 52.0f));
		ImGui::TextColored(phase_color, "%s", phase_str);

		// Left Panel: Player 1 Controls & Unit Roster
		DrawTeamPanel(
			draw_list,
			ImVec2(16.0f, 16.0f),
			"PLAYER 1 (LEFT - BLUE)",
			"Switch Unit: [Tab]\nDirection:   [W][A][S][D]\nStay: [Z]  Move: [X]  Shoot: [C]",
			team1_units_,
			phase_ == GamePhase::InstructionP1,
			IM_COL32(60, 165, 255, 255)
		);

		// Right Panel: Player 2 Controls & Unit Roster
		DrawTeamPanel(
			draw_list,
			ImVec2(screen_size.x - 316.0f, 16.0f),
			"PLAYER 2 (RIGHT - RED)",
			"Switch Unit: [Right Shift]\nDirection:   [Arrow Keys]\nStay: [N]  Move: [M]  Shoot: [,]",
			team2_units_,
			phase_ == GamePhase::InstructionP2,
			IM_COL32(255, 85, 75, 255)
		);

		// Bottom Status Bar
		ImGui::SetWindowFontScale(1.25f);
		ImVec2 ev_sz = ImGui::CalcTextSize(last_event_text_.c_str());
		float bot_w = std::max(520.0f, ev_sz.x + 40.0f);
		float bot_x = (screen_size.x - bot_w) * 0.5f;
		float bot_y = screen_size.y - 48.0f;
		draw_list->AddRectFilled(
			ImVec2(bot_x, bot_y),
			ImVec2(bot_x + bot_w, bot_y + 36.0f),
			IM_COL32(15, 22, 32, 215),
			6.0f
		);
		ImGui::SetCursorPos(ImVec2((screen_size.x - ev_sz.x) * 0.5f, bot_y + 8.0f));
		ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", last_event_text_.c_str());

		// Center Overlay for Goal or Game Over
		if (phase_ == GamePhase::Resolution && last_scored_team_ != -1) {
			ImGui::SetWindowFontScale(3.2f);
			std::string goal_msg = (last_scored_team_ == 0) ? "GOAL! PLAYER 1 SCORES!" : "GOAL! PLAYER 2 SCORES!";
			ImVec2 gsz = ImGui::CalcTextSize(goal_msg.c_str());
			ImVec2 gpos((screen_size.x - gsz.x) * 0.5f, (screen_size.y - gsz.y) * 0.5f);
			draw_list->AddRectFilled(
				ImVec2(gpos.x - 28.0f, gpos.y - 18.0f),
				ImVec2(gpos.x + gsz.x + 28.0f, gpos.y + gsz.y + 18.0f),
				IM_COL32(10, 15, 25, 235),
				12.0f
			);
			ImGui::SetCursorPos(gpos);
			ImGui::TextColored(ImVec4(1.0f, 0.90f, 0.25f, 1.0f), "%s", goal_msg.c_str());
		} else if (phase_ == GamePhase::GameOver) {
			ImGui::SetWindowFontScale(3.0f);
			std::string win_msg = (p1_score_ >= kWinScore) ? "PLAYER 1 WINS THE MATCH!" : "PLAYER 2 WINS THE MATCH!";
			ImVec2 wsz = ImGui::CalcTextSize(win_msg.c_str());
			ImVec2 wpos((screen_size.x - wsz.x) * 0.5f, screen_size.y * 0.42f);

			draw_list->AddRectFilled(
				ImVec2(wpos.x - 36.0f, wpos.y - 24.0f),
				ImVec2(wpos.x + wsz.x + 36.0f, wpos.y + 120.0f),
				IM_COL32(10, 15, 25, 240),
				12.0f
			);
			draw_list->AddRect(
				ImVec2(wpos.x - 36.0f, wpos.y - 24.0f),
				ImVec2(wpos.x + wsz.x + 36.0f, wpos.y + 120.0f),
				IM_COL32(255, 215, 70, 230),
				12.0f, 0, 2.5f
			);

			ImGui::SetCursorPos(wpos);
			ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.25f, 1.0f), "%s", win_msg.c_str());

			ImGui::SetWindowFontScale(1.4f);
			char sub_msg[128];
			std::snprintf(sub_msg, sizeof(sub_msg), "Final Score: %d - %d   |   Press [Z], [N], or [Space] to Play Again", p1_score_, p2_score_);
			ImVec2 ssz = ImGui::CalcTextSize(sub_msg);
			ImGui::SetCursorPos(ImVec2((screen_size.x - ssz.x) * 0.5f, wpos.y + 68.0f));
			ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "%s", sub_msg);
		}

		ImGui::SetWindowFontScale(1.0f);
	}

	void DrawTeamPanel(
		ImDrawList* draw_list,
		const ImVec2& pos,
		const char* title,
		const char* controls_desc,
		const std::array<Player*, kUnitsPerTeam>& units,
		bool is_active_turn,
		ImU32 accent_col
	) const {
		float w = 300.0f;
		float h = 225.0f;
		draw_list->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(15, 22, 32, 215), 8.0f);
		draw_list->AddRect(
			pos,
			ImVec2(pos.x + w, pos.y + h),
			is_active_turn ? IM_COL32(255, 235, 80, 240) : accent_col,
			8.0f,
			0,
			is_active_turn ? 2.5f : 1.2f
		);

		ImGui::SetWindowFontScale(1.15f);
		ImGui::SetCursorPos(ImVec2(pos.x + 12.0f, pos.y + 10.0f));
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accent_col), "%s", title);

		ImGui::SetWindowFontScale(1.0f);
		ImGui::SetCursorPos(ImVec2(pos.x + 12.0f, pos.y + 34.0f));
		ImGui::TextColored(ImVec4(0.82f, 0.86f, 0.92f, 1.0f), "%s", controls_desc);

		for (int i = 0; i < kUnitsPerTeam; ++i) {
			const Player* u = units[i];
			bool selected = is_active_turn && (i == active_unit_index_);
			float row_y = pos.y + 96.0f + i * 24.0f;

			if (selected) {
				draw_list->AddRectFilled(
					ImVec2(pos.x + 8.0f, row_y - 2.0f),
					ImVec2(pos.x + w - 8.0f, row_y + 20.0f),
					IM_COL32(255, 235, 80, 50),
					4.0f
				);
			}

			ImGui::SetCursorPos(ImVec2(pos.x + 14.0f, row_y));
			const char* status_str = u->action_confirmed ? ActionToShortStr(u->assigned_action) : "PENDING";
			ImVec4 row_col = selected
				? ImVec4(1.0f, 0.95f, 0.35f, 1.0f)
				: (u->action_confirmed ? ImVec4(0.55f, 0.95f, 0.65f, 1.0f) : ImVec4(0.75f, 0.78f, 0.82f, 1.0f));

			ImGui::TextColored(
				row_col,
				"%s Unit %d: %-6s %s",
				selected ? ">" : " ",
				i + 1,
				status_str,
				u->is_dribbling ? "[BALL]" : ""
			);
		}
	}
};
