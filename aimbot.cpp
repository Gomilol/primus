#include "includes.h"

Aimbot g_aimbot{ };;
void FixVelocity(Player* m_player, LagRecord* record, LagRecord* previous, float max_speed) {
	if (!previous) {
		if (record->m_layers[6].m_playback_rate > 0.0f && record->m_layers[6].m_weight != 0.f && record->m_velocity.length() > 0.1f) {
			auto v30 = max_speed;

			if (record->m_flags & 6)
				v30 *= 0.34f;
			else if (m_player->m_bIsWalking())
				v30 *= 0.52f;

			auto v35 = record->m_layers[6].m_weight * v30;
			record->m_velocity *= v35 / record->m_velocity.length();
		}
		else 
			record->m_velocity.clear();

		if (record->m_flags & 1)
			record->m_velocity.z = 0.f;

		record->m_anim_velocity = record->m_velocity;
		return;
	}

	if ((m_player->m_fEffects() & 8) != 0
		|| m_player->m_ubEFNoInterpParity() != m_player->m_ubEFNoInterpParityOld()) {
		record->m_velocity.clear();
		record->m_anim_velocity.clear();
		return;
	}

	auto is_jumping = !(record->m_flags & FL_ONGROUND && previous->m_flags & FL_ONGROUND);

	if (record->m_lag > 1) {
		record->m_velocity.clear();
		auto origin_delta = (record->m_origin - previous->m_origin);

		if (!(previous->m_flags & FL_ONGROUND || record->m_flags & FL_ONGROUND))// if not previous on ground or on ground
		{
			auto currently_ducking = record->m_flags & FL_DUCKING;
			if ((previous->m_flags & FL_DUCKING) != currently_ducking) {
				float duck_modifier = 0.f;

				if (currently_ducking)
					duck_modifier = 9.f;
				else
					duck_modifier = -9.f;

				origin_delta.z -= duck_modifier;
			}
		}

		auto sqrt_delta = origin_delta.length_2d_sqr();

		if (sqrt_delta > 0.f && sqrt_delta < 1000000.f)
			record->m_velocity = origin_delta / game::TICKS_TO_TIME(record->m_lag);

		record->m_velocity.validate_vec();

		if (is_jumping) {
			if (record->m_flags & FL_ONGROUND && !g_csgo.sv_enablebunnyhopping->GetInt()) {

				// 260 x 1.1 = 286 units/s.
				float max = m_player->m_flMaxspeed() * 1.1f;

				// get current velocity.
				float speed = record->m_velocity.length();

				// reset velocity to 286 units/s.
				if (max > 0.f && speed > max)
					record->m_velocity *= (max / speed);
			}

			// assume the player is bunnyhopping here so set the upwards impulse.
			record->m_velocity.z = g_csgo.sv_jump_impulse->GetFloat();
		}
		// we are not on the ground
		// apply gravity and airaccel.
		else if (!(record->m_flags & FL_ONGROUND)) {
			// apply one tick of gravity.
			record->m_velocity.z -= g_csgo.sv_gravity->GetFloat() * g_csgo.m_globals->m_interval;
		}
	}

	record->m_anim_velocity = record->m_velocity;

	if (!record->m_fake_walk) {
		if (record->m_anim_velocity.length_2d() > 0 && (record->m_flags & FL_ONGROUND)) {
			float anim_speed = 0.f;

			if (!is_jumping
				&& record->m_layers[11].m_weight > 0.0f
				&& record->m_layers[11].m_weight < 1.0f
				&& record->m_layers[11].m_playback_rate == previous->m_layers[11].m_playback_rate) {
				// calculate animation speed yielded by anim overlays
				auto flAnimModifier = 0.35f * (1.0f - record->m_layers[11].m_weight);
				if (flAnimModifier > 0.0f && flAnimModifier < 1.0f)
					anim_speed = max_speed * (flAnimModifier + 0.55f);
			}

			// this velocity is valid ONLY IN ANIMFIX UPDATE TICK!!!
			// so don't store it to record as m_vecVelocity
			// -L3D451R7
			if (anim_speed > 0.0f) {
				anim_speed /= record->m_anim_velocity.length_2d();
				record->m_anim_velocity.x *= anim_speed;
				record->m_anim_velocity.y *= anim_speed;
			}
		}
	}
	else
		record->m_anim_velocity.clear();

	record->m_anim_velocity.validate_vec();
}

void AimPlayer::UpdateAnimations(LagRecord* record) {
	CCSGOPlayerAnimState* state = m_player->m_PlayerAnimState();


	// player respawned.
	if (m_player->m_flSpawnTime() != m_spawn) {
		// reset animation state.
		game::ResetAnimationState(state);

		// note new spawn time.
		m_spawn = m_player->m_flSpawnTime();
	}

	// first off lets backup our globals.
	auto curtime = g_csgo.m_globals->m_curtime;
	auto realtime = g_csgo.m_globals->m_realtime;
	auto frametime = g_csgo.m_globals->m_frametime;
	auto absframetime = g_csgo.m_globals->m_abs_frametime;
	auto framecount = g_csgo.m_globals->m_frame;
	auto tickcount = g_csgo.m_globals->m_tick_count;
	auto interpolation = g_csgo.m_globals->m_interp_amt;

	// backup stuff that we do not want to fuck with.
	AnimationBackup_t backup;

	backup.m_origin = m_player->m_vecOrigin();
	backup.m_abs_origin = m_player->GetAbsOrigin();
	backup.m_velocity = m_player->m_vecVelocity();
	backup.m_abs_velocity = m_player->m_vecAbsVelocity();
	backup.m_flags = m_player->m_fFlags();
	backup.m_eflags = m_player->m_iEFlags();
	backup.m_duck = m_player->m_flDuckAmount();
	backup.m_body = m_player->m_flLowerBodyYawTarget();
	m_player->GetAnimLayers(backup.m_layers);

	LagRecord* previous = m_records.size() > 1 ? m_records[1].get() : nullptr;

	if (previous && (previous->dormant() || !previous->m_setup))
		previous = nullptr;

	// set globals.
	g_csgo.m_globals->m_curtime = record->m_anim_time;
	g_csgo.m_globals->m_realtime = record->m_anim_time;
	g_csgo.m_globals->m_frame = game::TIME_TO_TICKS(record->m_anim_time);
	g_csgo.m_globals->m_tick_count = game::TIME_TO_TICKS(record->m_anim_time);
	g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_abs_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_interp_amt = 0.f;

	// is player a bot?
	bool bot = game::IsFakePlayer(m_player->index());
	// reset resolver/fakewalk/fakeflick state.
	record->m_mode = Resolver::Modes::RESOLVE_NONE;
	record->m_fake_walk = false;
//	record->m_fake_flick = false;

	// thanks llama.
	if (record->m_flags & FL_ONGROUND) {
		// they are on ground.
		state->m_ground = true;
		// no they didnt land.
		state->m_land = false;
	}

	// fix velocity.
	float max_speed = 260.f;
	if (record->m_lag > 0 && record->m_lag < 16 && m_records.size() >= 2) {
		FixVelocity(m_player, record, previous, max_speed);
	}

	// fix CGameMovement::FinishGravity
	if (!(m_player->m_fFlags() & FL_ONGROUND))
		record->m_velocity.z -= game::TICKS_TO_TIME(g_csgo.sv_gravity->GetFloat());
	else
		record->m_velocity.z = 0.0f;

	// set this fucker, it will get overriden.
	record->m_anim_velocity = record->m_velocity;

	// fix various issues with the game.
	// these issues can only occur when a player is choking data.
	if (record->m_lag > 1 && !bot) {

		auto speed = record->m_velocity.length();


		// detect fakewalking players
		if (speed > 1.f
			&& record->m_lag >= 12
			&& record->m_layers[12].m_weight == 0.0f
			&& record->m_layers[6].m_weight == 0.0f
			&& record->m_layers[6].m_playback_rate < 0.0001f
			&& (record->m_flags & FL_ONGROUND))
			record->m_fake_walk = true;

		// if they fakewalk scratch this shit.
		if (record->m_fake_walk)
			record->m_anim_velocity = record->m_velocity = { 0.f, 0.f, 0.f };

		// detect fakeflicking players
		//if (record->m_velocity.length() < 18.f
		//	&& record->m_layers[6].m_weight != 1.0f
		//	&& record->m_layers[6].m_weight != 0.0f
		//	&& record->m_layers[6].m_weight != m_records.front().get()->m_layers[6].m_weight
		//	&& (record->m_flags & FL_ONGROUND))
		//	record->m_fake_flick = true;

		// detect fakeflicking players
		if (record->m_velocity.length() < 0.1f != record->m_velocity.length() > 38.0f) { // check
			if (record->m_velocity.length() < 0.1f
				&& record->m_layers[6].m_weight != 1.0f
				&& record->m_layers[6].m_weight != 0.0f
				&& record->m_layers[6].m_weight != previous->m_layers[6].m_weight // correct weight??
				&& (record->m_flags & FL_ONGROUND));
				//record->m_fake_flick = true;
		}

		// we need atleast 2 updates/records
		// to fix these issues.
		if (m_records.size() >= 2) {
			// get pointer to previous record.
			LagRecord* previous = m_records[1].get();

			// valid previous record.
			if (previous && !previous->dormant()) {
				// LOL.
				if ((record->m_origin - previous->m_origin).IsZero())
					record->m_anim_velocity = record->m_velocity = { 0.f, 0.f, 0.f };

				// jumpfall.
				bool bOnGround = record->m_flags & FL_ONGROUND;
				bool bJumped = false;
				bool bLandedOnServer = false;
				float flLandTime = 0.f;

				// magic llama code.
				if (record->m_layers[4].m_cycle < 0.5f && (!(record->m_flags & FL_ONGROUND) || !(previous->m_flags & FL_ONGROUND))) {
					flLandTime = record->m_sim_time - float(record->m_layers[4].m_playback_rate / record->m_layers[4].m_cycle);
					bLandedOnServer = flLandTime >= previous->m_sim_time;
				}

				// jump_fall fix
				if (bLandedOnServer && !bJumped) {
					if (flLandTime <= record->m_anim_time) {
						bJumped = true;
						bOnGround = true;
					}
					else {
						bOnGround = previous->m_flags & FL_ONGROUND;
					}
				}

				// do the fix. hahaha
				if (bOnGround) {
					m_player->m_fFlags() |= FL_ONGROUND;
				}
				else {
					m_player->m_fFlags() &= ~FL_ONGROUND;
				}

				// delta in duckamt and delta in time..
				float duck = record->m_duck - previous->m_duck;
				float time = record->m_sim_time - previous->m_sim_time;

				// get the duckamt change per tick.
				float change = (duck / time) * g_csgo.m_globals->m_interval;

				// fix crouching players.
				m_player->m_flDuckAmount() = previous->m_duck + change;

				//if (!record->m_fake_walk) {
				//	// fix the velocity till the moment of animation.
				//	vec3_t velo = record->m_velocity - previous->m_velocity;

				//	// accel per tick.
				//	vec3_t accel = (velo / time) * g_csgo.m_globals->m_interval;

				//	// set the anim velocity to the previous velocity.
				//	// and predict one tick ahead.
				//	record->m_anim_velocity = previous->m_velocity + accel;
				//}
			}
		}
	}

	// lol?
	for (int i = 0; i < 13; i++)
		m_player->m_AnimOverlay()[i].m_1owner = m_player;


	bool fake = g_menu.main.aimbot.correct.get() && !bot;

	// if using fake angles, correct angles.
	if (fake)
		g_resolver.ResolveAngles(m_player, record);

	// force to use correct abs origin and velocity ( no CalcAbsolutePosition and CalcAbsoluteVelocity calls )
	m_player->m_iEFlags() &= ~(EFL_DIRTY_ABSTRANSFORM | EFL_DIRTY_ABSVELOCITY);

	// set stuff before animating.
	m_player->m_vecOrigin() = record->m_origin;
	m_player->m_vecVelocity() = m_player->m_vecAbsVelocity() = record->m_anim_velocity;
	m_player->m_flLowerBodyYawTarget() = record->m_body;

	// write potentially resolved angles.
	m_player->m_angEyeAngles() = record->m_eye_angles;

	// fix animating in same frame.
	if (state->m_frame == g_csgo.m_globals->m_frame)
		state->m_frame -= 1;

	// get invalidated bone cache.
	static auto& invalidatebonecache = pattern::find(g_csgo.m_client_dll, XOR("C6 05 ? ? ? ? ? 89 47 70")).add(0x2);

	// make sure we keep track of the original invalidation state
	const auto oldbonecache = invalidatebonecache;

	// update animtions now.
	m_player->m_bClientSideAnimation() = true;
	m_player->UpdateClientSideAnimation();
	m_player->m_bClientSideAnimation() = false;

	// we don't want to enable cache invalidation by accident
	invalidatebonecache = oldbonecache;

	// player animations have updated.
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::ANGLES_CHANGED);
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::ANIMATION_CHANGED);
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::BOUNDS_CHANGED);
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::SEQUENCE_CHANGED);

	// if fake angles.
	if (fake) {
		// correct poses.
		g_resolver.ResolvePoses(m_player, record);
	}

	// store updated/animated poses and rotation in lagrecord.
	m_player->GetPoseParameters(record->m_poses);
	record->m_abs_ang = m_player->GetAbsAngles();

	// restore backup data.
	m_player->m_vecOrigin() = backup.m_origin;
	m_player->m_vecVelocity() = backup.m_velocity;
	m_player->m_vecAbsVelocity() = backup.m_abs_velocity;
	m_player->m_fFlags() = backup.m_flags;
	m_player->m_iEFlags() = backup.m_eflags;
	m_player->m_flDuckAmount() = backup.m_duck;
	m_player->m_flLowerBodyYawTarget() = backup.m_body;
	m_player->SetAbsOrigin(backup.m_abs_origin);
	m_player->SetAnimLayers(backup.m_layers);

	// restore globals.
	g_csgo.m_globals->m_curtime = curtime;
	g_csgo.m_globals->m_realtime = realtime;
	g_csgo.m_globals->m_frametime = frametime;
	g_csgo.m_globals->m_abs_frametime = absframetime;
	g_csgo.m_globals->m_frame = framecount;
	g_csgo.m_globals->m_tick_count = tickcount;
	g_csgo.m_globals->m_interp_amt = interpolation;

	// IMPORTANT: do not restore poses here, since we want to preserve them for rendering.
	// also dont restore the render angles which indicate the model rotation.
}

void AimPlayer::OnNetUpdate(Player* player) {
	bool reset = (!g_menu.main.aimbot.enable.get() || player->m_lifeState() == LIFE_DEAD || !player->enemy(g_cl.m_local));
	bool disable = (!reset && !g_cl.m_processing);

	// if this happens, delete all the lagrecords.
	if (reset) {
		player->m_bClientSideAnimation() = true;
		m_records.clear();
		return;
	}

	// just disable anim if this is the case.
	if (disable) {
		player->m_bClientSideAnimation() = true;
		return;
	}

	// update player ptr if required.
	// reset player if changed.
	if (m_player != player)
		m_records.clear();

	// update player ptr.
	m_player = player;

	// indicate that this player has been out of pvs.
	// insert dummy record to separate records
	// to fix stuff like animation and prediction.
	if (player->dormant()) {
		bool insert = true;

		// we have any records already?
		if (!m_records.empty()) {

			LagRecord* front = m_records.front().get();

			// we already have a dormancy separator.
			if (front->dormant())
				insert = false;
		}

		if (insert) {
			m_moved = false;

			// add new record.
			m_records.emplace_front(std::make_shared< LagRecord >(player));

			// get reference to newly added record.
			LagRecord* current = m_records.front().get();

			// mark as dormant.
			current->m_dormant = true;
		}
	}

	bool update = (m_records.empty() || player->m_flSimulationTime() > m_records.front().get()->m_sim_time);

	if (!update && !player->dormant() && player->m_vecOrigin() != player->m_vecOldOrigin()) {
		update = true;

		// fix data.
		player->m_flSimulationTime() = game::TICKS_TO_TIME(g_csgo.m_cl->m_server_tick);
	}

	// this is the first data update we are receving
	// OR we received data with a newer simulation context.
	if (update) {
		// add new record.
		m_records.emplace_front(std::make_shared< LagRecord >(player));

		// get reference to newly added record.
		LagRecord* current = m_records.front().get();

		// mark as non dormant.
		current->m_dormant = false;

		// update animations on current record.
		// call resolver.
		UpdateAnimations(current);

		// create bone matrix for this record.
		g_bones.setup(m_player, nullptr, current);
	}

	// no need to store insane amt of data.
	while (m_records.size() > 256)
		m_records.pop_back();
}


void AimPlayer::OnRoundStart( Player *player ) {
	m_player = player;
	m_walk_record = LagRecord{ };
	m_shots = 0;
	m_missed_shots = 0;

	// reset stand and body index.
	m_stand_index  = 0;
	m_stand_index2 = 0;
	m_body_index   = 0;

	m_records.clear( );
	m_hitboxes.clear( );

	// IMPORTANT: DO NOT CLEAR LAST HIT SHIT.
}

void AimPlayer::SetupHitboxes( LagRecord *record, bool history ) {
	// reset hitboxes.
	m_hitboxes.clear( );

	if ( g_cl.m_weapon_id == ZEUS ) {
		// hitboxes for the zeus.
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::PREFER } );
		return;
	}

	// prefer, always.
	if ( g_menu.main.aimbot.baim1.get( 0 ) )
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::PREFER } );

	// prefer, lethal.
	if ( g_menu.main.aimbot.baim1.get( 1 ) )
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::LETHAL } );

	// prefer, lethal x2.
	if ( g_menu.main.aimbot.baim1.get( 2 ) )
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::LETHAL2 } );

	// prefer, fake.
	if ( g_menu.main.aimbot.baim1.get( 3 ) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK )
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::PREFER } );

	// prefer, in air.
	if ( g_menu.main.aimbot.baim1.get( 4 ) && !( record->m_pred_flags & FL_ONGROUND ) )
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::PREFER } );

	bool only{ false };

	// only, always.
	if ( g_menu.main.aimbot.baim2.get( 0 ) ) {
		only = true;
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::PREFER } );
	}

	// only, health.
	if ( g_menu.main.aimbot.baim2.get( 1 ) && m_player->m_iHealth( ) <= ( int )g_menu.main.aimbot.baim_hp.get( ) ) {
		only = true;
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::PREFER } );
	}

	// only, fake.
	if ( g_menu.main.aimbot.baim2.get( 2 ) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK ) {
		only = true;
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::PREFER } );
	}

	// only, in air.
	if ( g_menu.main.aimbot.baim2.get( 3 ) && !( record->m_pred_flags & FL_ONGROUND ) ) {
		only = true;
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::PREFER } );
	}

	// only, on key.
	if ( g_input.GetKeyState( g_menu.main.aimbot.baim_key.get( ) ) ) {
		only = true;
		m_hitboxes.push_back( { HITBOX_BODY, HitscanMode::PREFER } );
	}

	// only baim conditions have been met.
	// do not insert more hitboxes.
	if ( only )
		return;


	if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A) {
		std::vector< size_t > hitbox{ history ? g_menu.main.aimbot.hitbox_history_pistols.GetActiveIndices() : g_menu.main.aimbot.hitbox_pistols.GetActiveIndices() };
		if (hitbox.empty())
			return;

	//	bool ignore_limbs = record->m_velocity.length_2d() > 71.f && g_menu.main.aimbot.ignor_limbs.get();

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// chest.
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			// foot.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else if (g_cl.m_weapon_id == SSG08) {
		std::vector< size_t > hitbox{ history ? g_menu.main.aimbot.hitbox_history_scout.GetActiveIndices() : g_menu.main.aimbot.hitbox_scout.GetActiveIndices() };
		if (hitbox.empty())
			return;

	//	bool ignore_limbs = record->m_velocity.length_2d() > 71.f && g_menu.main.aimbot.ignor_limbs.get();

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// chest.
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 3 ) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			// foot.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20) {
		std::vector< size_t > hitbox{ history ? g_menu.main.aimbot.hitbox_history_auto.GetActiveIndices() : g_menu.main.aimbot.hitbox_auto.GetActiveIndices() };
		if (hitbox.empty())
			return;

	//	bool ignore_limbs = record->m_velocity.length_2d() > 71.f && g_menu.main.aimbot.ignor_limbs.get();

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// chest.
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			// foot.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else if (g_cl.m_weapon_id == AWP) {
		std::vector< size_t > hitbox{ history ? g_menu.main.aimbot.hitbox_history_awp.GetActiveIndices() : g_menu.main.aimbot.hitbox_awp.GetActiveIndices() };
		if (hitbox.empty())
			return;

	//	bool ignore_limbs = record->m_velocity.length_2d() > 71.f && g_menu.main.aimbot.ignor_limbs.get();

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// chest.
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			// foot.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else if (g_cl.m_weapon_id == g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER) {
		std::vector< size_t > hitbox{ history ? g_menu.main.aimbot.hitbox_history_heavy.GetActiveIndices() : g_menu.main.aimbot.hitbox_heavy.GetActiveIndices() };
		if (hitbox.empty())
			return;

	//	bool ignore_limbs = record->m_velocity.length_2d() > 71.f && g_menu.main.aimbot.ignor_limbs.get();

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// chest.
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			// foot.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
	else {
		std::vector< size_t > hitbox{ history ? g_menu.main.aimbot.hitbox_history_general.GetActiveIndices() : g_menu.main.aimbot.hitbox_general.GetActiveIndices() };
		if (hitbox.empty())
			return;

	//	bool ignore_limbs = record->m_velocity.length_2d() > 71.f && g_menu.main.aimbot.ignor_limbs.get();

		for (const auto& h : hitbox) {
			// head.
			if (h == 0)
				m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

			// chest.
			if (h == 1) {
				m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
			}

			// stomach.
			if (h == 2) {
				m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			}

			// arms.
			if (h == 3) {
				m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			}

			// legs.
			if (h == 4) {
				m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
			}

			// foot.
			if (h == 5) {
				m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
				m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
			}
		}
	}
}

void Aimbot::init( ) {
	// clear old targets.
	m_targets.clear( );

	m_target = nullptr;
	m_aim = vec3_t{ };
	m_angle = ang_t{ };
	m_damage = 0.f;
	m_record = nullptr;
	m_stop = false;

	m_best_dist = std::numeric_limits< float >::max( );
	m_best_fov = 180.f + 1.f;
	m_best_damage = 0.f;
	m_best_hp = 100 + 1;
	m_best_lag = std::numeric_limits< float >::max( );
	m_best_height = std::numeric_limits< float >::max( );
}

void Aimbot::StripAttack( ) {
	if ( g_cl.m_weapon_id == REVOLVER )
		g_cl.m_cmd->m_buttons &= ~IN_ATTACK2;

	else
		g_cl.m_cmd->m_buttons &= ~IN_ATTACK;
}

void Aimbot::think( ) {
	// do all startup routines.
	init( );

	// sanity.
	if ( !g_cl.m_weapon )
		return;

	// no grenades or bomb.
	if ( g_cl.m_weapon_type == WEAPONTYPE_GRENADE || g_cl.m_weapon_type == WEAPONTYPE_C4 )
		return;

	if ( !g_cl.m_weapon_fire )
		StripAttack( );

	// we have no aimbot enabled.
	if ( !g_menu.main.aimbot.enable.get( ) )
		return;

	// animation silent aim, prevent the ticks with the shot in it to become the tick that gets processed.
	// we can do this by always choking the tick before we are able to shoot.
	bool revolver = g_cl.m_weapon_id == REVOLVER && g_cl.m_revolver_cock != 0;

	// one tick before being able to shoot.
	if ( revolver && g_cl.m_revolver_cock > 0 && g_cl.m_revolver_cock == g_cl.m_revolver_query ) {
		*g_cl.m_packet = false;
		return;
	}

	// we have a normal weapon or a non cocking revolver
	// choke if its the processing tick.
	if ( g_cl.m_weapon_fire && !g_cl.m_lag && !revolver ) {
		*g_cl.m_packet = false;
		StripAttack( );
		return;
	}

	// no point in aimbotting if we cannot fire this tick.
	if ( !g_cl.m_weapon_fire )
		return;

	// setup bones for all valid targets.
	for ( int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i ) {
		Player *player = g_csgo.m_entlist->GetClientEntity< Player * >( i );

		if ( !IsValidTarget( player ) )
			continue;

		AimPlayer *data = &m_players[ i - 1 ];
		if ( !data )
			continue;

		// store player as potential target this tick.
		m_targets.emplace_back( data );
	}

	// run knifebot.
	if ( g_cl.m_weapon_type == WEAPONTYPE_KNIFE && g_cl.m_weapon_id != ZEUS ) {

		if ( g_menu.main.aimbot.knifebot.get( ) )
			knife( );

		return;
	}

	// scan available targets... if we even have any.
	find( );

	// finally set data when shooting.
	apply( );
}

void Aimbot::find( ) {
	struct BestTarget_t { Player *player; vec3_t pos; float damage; LagRecord *record; };

	vec3_t       tmp_pos;
	float        tmp_damage;
	BestTarget_t best;
	best.player = nullptr;
	best.damage = -1.f;
	best.pos = vec3_t{ };
	best.record = nullptr;

	if ( m_targets.empty( ) )
		return;

	if ( g_cl.m_weapon_id == ZEUS && !g_menu.main.aimbot.zeusbot.get( ) )
		return;

	// iterate all targets.
	for ( const auto &t : m_targets ) {
		if ( t->m_records.empty( ) )
			continue;

		// this player broke lagcomp.
		// his bones have been resetup by our lagcomp.
		// therfore now only the front record is valid.
		if ( g_menu.main.aimbot.lagfix.get( ) && g_lagcomp.StartPrediction( t ) ) {
			LagRecord *front = t->m_records.front( ).get( );

			t->SetupHitboxes( front, false );
			if ( t->m_hitboxes.empty( ) )
				continue;

			// rip something went wrong..
			if ( t->GetBestAimPosition( tmp_pos, tmp_damage, front ) && SelectTarget( front, tmp_pos, tmp_damage ) ) {

				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = front;
			}
		}

		// player did not break lagcomp.
		// history aim is possible at this point.
		else {
			LagRecord *ideal = g_resolver.FindIdealRecord( t );
			if ( !ideal )
				continue;

			t->SetupHitboxes( ideal, false );
			if ( t->m_hitboxes.empty( ) )
				continue;

			// try to select best record as target.
			if ( t->GetBestAimPosition( tmp_pos, tmp_damage, ideal ) && SelectTarget( ideal, tmp_pos, tmp_damage ) ) {
				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = ideal;
			}

			LagRecord *last = g_resolver.FindLastRecord( t );
			if ( !last || last == ideal )
				continue;

			t->SetupHitboxes( last, true );
			if ( t->m_hitboxes.empty( ) )
				continue;

			// rip something went wrong..
			if ( t->GetBestAimPosition( tmp_pos, tmp_damage, last ) && SelectTarget( last, tmp_pos, tmp_damage ) ) {
				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = last;
			}
		}
	}

	// verify our target and set needed data.
	if ( best.player && best.record ) {
		// calculate aim angle.
		math::VectorAngles( best.pos - g_cl.m_shoot_pos, m_angle );

		// set member vars.
		m_target = best.player;
		m_aim = best.pos;
		m_damage = best.damage;
		m_record = best.record;

		// write data, needed for traces / etc.
		m_record->cache( );

		if (m_targets.size() >= 3)
		{
			auto first = rand() % m_targets.size();
			auto second = rand() % m_targets.size();
			auto third = rand() % m_targets.size();

			for (auto i = 0; i < m_targets.size(); ++i)
			{
				if (i == first || i == second || i == third)
					continue;

				m_targets.erase(m_targets.begin() + i);

				if (i > 0)
					--i;
			}
		}
		// set autostop shit.
		m_stop = !( g_cl.m_buttons & IN_JUMP );

		bool on = false;

		if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
			on = g_menu.main.aimbot.hitchance_pistols.get() && g_menu.main.config.mode.get() == 0;
		else if (g_cl.m_weapon_id == SSG08)
			on = g_menu.main.aimbot.hitchance_scout.get() && g_menu.main.config.mode.get() == 0;
		else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
			on = g_menu.main.aimbot.hitchance_auto.get() && g_menu.main.config.mode.get() == 0;
		else if (g_cl.m_weapon_id == AWP)
			on = g_menu.main.aimbot.hitchance_awp.get() && g_menu.main.config.mode.get() == 0;
		else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
			on = g_menu.main.aimbot.hitchance_heavy.get() && g_menu.main.config.mode.get() == 0;
		else
			on = g_menu.main.aimbot.hitchance_general.get() && g_menu.main.config.mode.get() == 0;


		bool hit = on && CheckHitchance( m_target, m_angle );


		if (g_menu.main.movement.autostop_always_on.get() || g_input.GetKeyState(g_menu.main.movement.autostop.get()))
		{
			m_stop = !(g_cl.m_buttons & IN_JUMP) && on && !hit;
			//	m_slow_motion_slowwalk = false;
			//  m_slow_motion_fakewalk = false;

			g_movement.QuickStop();
		}
		// if we can scope.
		bool can_scope = !g_cl.m_local->m_bIsScoped( ) && ( g_cl.m_weapon_id == AUG || g_cl.m_weapon_id == SG553 || g_cl.m_weapon_type == WEAPONTYPE_SNIPER_RIFLE );

		if ( can_scope ) {
			// always.
			if ( g_menu.main.aimbot.zoom.get( ) == 1 ) {
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
				return;
			}

			// hitchance fail.
			else if ( g_menu.main.aimbot.zoom.get( ) == 2 && on && !hit ) {
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
				return;
			}
		}

		if ( hit || !on ) {
			// right click attack.
			if ( g_menu.main.config.mode.get( ) == 1 && g_cl.m_weapon_id == REVOLVER )
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;

			// left click attack.
			else
				g_cl.m_cmd->m_buttons |= IN_ATTACK;
		}
	}
}

bool Aimbot::CheckHitchance( Player *player, const ang_t &angle ) {
	//constexpr float HITCHANCE_MAX = 100.f;
	//constexpr int   SEED_MAX = 255;

	float hc = FLT_MAX;

	if (g_cl.m_weapon_id == ZEUS)
		hc = 90;
	else if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
		hc = g_menu.main.aimbot.hitchance_amount_pistols.get();
	else if (g_cl.m_weapon_id == SSG08)
		hc = g_menu.main.aimbot.hitchance_amount_scout.get();
	else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
		hc = g_menu.main.aimbot.hitchance_amount_auto.get();
	else if (g_cl.m_weapon_id == AWP)
		hc = g_menu.main.aimbot.hitchance_amount_awp.get();
	else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
		hc = g_menu.main.aimbot.hitchance_amount_heavy.get();
	else
		hc = g_menu.main.aimbot.hitchance_amount_general.get();


	constexpr float HITCHANCE_MAX = 100.f;
	constexpr int   SEED_MAX = 255;

	vec3_t     start{ g_cl.m_shoot_pos }, end, fwd, right, up, dir, wep_spread;
	float      inaccuracy, spread;
	CGameTrace tr;
	int     total_hits{}, needed_hits{ int(float(hc / HITCHANCE_MAX) * SEED_MAX) };

	// get needed directional vectors.
	math::AngleVectors(angle, &fwd, &right, &up);

	// store off inaccuracy / spread ( these functions are quite intensive and we only need them once ).
	inaccuracy = g_cl.m_weapon->GetInaccuracy();
	spread = g_cl.m_weapon->GetSpread();

	if ((g_inputpred.m_perfect_accuracy + 0.0005f) >= inaccuracy)
		return true;

	// iterate all possible seeds.
	for (int i{}; i <= SEED_MAX; ++i) {
		// get spread. wwwaaaaaa
		wep_spread = g_cl.m_weapon->CalculateSpread(m_static_seeds[i], inaccuracy, spread);

		// get spread direction.
		dir = (fwd + (right * wep_spread.x) + (up * wep_spread.y)).normalized();

		// get end of trace.
		end = start + (dir * g_cl.m_weapon_info->m_range);

		// setup ray and trace.
		g_csgo.m_engine_trace->ClipRayToEntity(Ray(start, end), MASK_SHOT_HULL | CONTENTS_HITBOX, player, &tr);

		// check if we hit a valid player / hitgroup on the player and increment total hits.
		if (tr.m_entity == player && game::IsValidHitgroup(tr.m_hitgroup))
			++total_hits;

		// we made it.
		if (total_hits >= needed_hits)
			return true;

		// we cant make it anymore.
		if ((SEED_MAX - i + total_hits) < needed_hits)
			return false;
	}

	return false;
}

bool AimPlayer::SetupHitboxPoints( LagRecord *record, BoneArray *bones, int index, std::vector< vec3_t > &points ) {
	// reset points.
	points.clear();

	const model_t* model = m_player->GetModel();
	if (!model)
		return false;

	studiohdr_t* hdr = g_csgo.m_model_info->GetStudioModel(model);
	if (!hdr)
		return false;

	mstudiohitboxset_t* set = hdr->GetHitboxSet(m_player->m_nHitboxSet());
	if (!set)
		return false;

	mstudiobbox_t* bbox = set->GetHitbox(index);
	if (!bbox)
		return false;

	// get hitbox scales.
	float scale = 0.f;

	if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
		scale = g_menu.main.aimbot.scale_pistols.get() / 100.f;
	else if (g_cl.m_weapon_id == SSG08)
		scale = g_menu.main.aimbot.scale_scout.get() / 100.f;
	else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
		scale = g_menu.main.aimbot.scale_auto.get() / 100.f;
	else if (g_cl.m_weapon_id == AWP)
		scale = g_menu.main.aimbot.scale_awp.get() / 100.f;
	else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
		scale = g_menu.main.aimbot.scale_heavy.get() / 100.f;
	else
		scale = g_menu.main.aimbot.scale_general.get() / 100.f;

	// big inair fix.
	//if (!(record->m_pred_flags) & FL_ONGROUND)
	//	scale = 0.7f;

	float bscale = 0.f;
	auto final_radius = bbox->m_radius * scale;

	if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A)
		bscale = g_menu.main.aimbot.body_scale_pistols.get() / 100.f;
	else if (g_cl.m_weapon_id == SSG08)
		bscale = g_menu.main.aimbot.body_scale_scout.get() / 100.f;
	else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20)
		bscale = g_menu.main.aimbot.body_scale_auto.get() / 100.f;
	else if (g_cl.m_weapon_id == AWP)
		bscale = g_menu.main.aimbot.body_scale_awp.get() / 100.f;
	else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER)
		bscale = g_menu.main.aimbot.body_scale_heavy.get() / 100.f;
	else
		bscale = g_menu.main.aimbot.body_scale_general.get() / 100.f;

	// these indexes represent boxes.
	if (bbox->m_radius <= 0.f) {
		// references: 
		//      https://developer.valvesoftware.com/wiki/Rotation_Tutorial
		//      CBaseAnimating::GetHitboxBonePosition
		//      CBaseAnimating::DrawServerHitboxes

		// convert rotation angle to a matrix.
		matrix3x4_t rot_matrix;
		g_csgo.AngleMatrix(bbox->m_angle, rot_matrix);

		// apply the rotation to the entity input space (local).
		matrix3x4_t matrix;
		math::ConcatTransforms(bones[bbox->m_bone], rot_matrix, matrix);

		// extract origin from matrix.
		vec3_t origin = matrix.GetOrigin();

		// compute raw center point.
		vec3_t center = (bbox->m_mins + bbox->m_maxs) / 2.f;

		// the feet hiboxes have a side, heel and the toe.
		/*if (index == HITBOX_R_FOOT || index == HITBOX_L_FOOT) {
			float d1 = (bbox->m_mins.z - center.z) * 0.875f;

			// invert.
			if (index == HITBOX_L_FOOT)
				d1 *= -1.f;

			// side is more optimal then center.
			points.push_back({ center.x, center.y, center.z + d1 });

			if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A || g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER) {
				if (g_menu.main.aimbot.multipoint_pistols.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * scale;
					float d3 = (bbox->m_maxs.x - center.x) * scale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
			else if (g_cl.m_weapon_id == SSG08) {
				if (g_menu.main.aimbot.multipoint_scout.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * scale;
					float d3 = (bbox->m_maxs.x - center.x) * scale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
			else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20) {
				if (g_menu.main.aimbot.multipoint_auto.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * scale;
					float d3 = (bbox->m_maxs.x - center.x) * scale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
			else if (g_cl.m_weapon_id == AWP) {
				if (g_menu.main.aimbot.multipoint_awp.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * scale;
					float d3 = (bbox->m_maxs.x - center.x) * scale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
			else {
				if (g_menu.main.aimbot.multipoint_general.get(3)) {
					// get point offset relative to center point
					// and factor in hitbox scale.
					float d2 = (bbox->m_mins.x - center.x) * scale;
					float d3 = (bbox->m_maxs.x - center.x) * scale;

					// heel.
					points.push_back({ center.x + d2, center.y, center.z });

					// toe.
					points.push_back({ center.x + d3, center.y, center.z });
				}
			}
		}*/

		// nothing to do here we are done.
		if (points.empty())
			return false;

		// rotate our bbox points by their correct angle
		// and convert our points to world space.
		for (auto& p : points) {
			// VectorRotate.
			// rotate point by angle stored in matrix.
			p = { p.dot(matrix[0]), p.dot(matrix[1]), p.dot(matrix[2]) };

			// transform point to world space.
			p += origin;
		}
	}

	// these hitboxes are capsules.
	else {
		// factor in the pointscale.
		float r = bbox->m_radius * scale;
		float br = bbox->m_radius * bscale;

		// compute raw center point.
		vec3_t center = (bbox->m_mins + bbox->m_maxs) / 2.f;

		// head has 5 points.
		if (index == HITBOX_HEAD) {
			// add center.
			points.push_back(center);

			if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A) {
				if (g_menu.main.aimbot.multipoint_pistols.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.  THIS IS SILLY LOL -kiera.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}
			else if (g_cl.m_weapon_id == SSG08) {
				if (g_menu.main.aimbot.multipoint_scout.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.  THIS IS SILLY LOL -kiera.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}
			else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20) {
				if (g_menu.main.aimbot.multipoint_auto.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.  THIS IS SILLY LOL -kiera.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}
			else if (g_cl.m_weapon_id == AWP) {
				if (g_menu.main.aimbot.multipoint_awp.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.  THIS IS SILLY LOL -kiera.
					// if we are standing still and have the lowest possible pitch pose.
					//if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
					points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					//}
				}
			}
			else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER) {
				if (g_menu.main.aimbot.multipoint_heavy.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.  THIS IS SILLY LOL -kiera.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}
			else {
				if (g_menu.main.aimbot.multipoint_general.get(0)) {
					// rotation matrix 45 degrees.
					// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
					// std::cos( deg_to_rad( 45.f ) )
					constexpr float rotation = 0.70710678f;

					// top/back 45 deg.
					// this is the best spot to shoot at.
					points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

					// right.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

					// left.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

					// back.
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

					// get animstate ptr.
					CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

					// add this point only under really specific circumstances.  THIS IS SILLY LOL -kiera.
					// if we are standing still and have the lowest possible pitch pose.
					if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_min_pitch) {

						// bottom point.
						points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
					}
				}
			}
		}

		// body has 5 points.
		else if (index == HITBOX_BODY) {
			// center.
			points.push_back(center);

			if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A) {
				// back.
				if (g_menu.main.aimbot.multipoint_pistols.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
			else if (g_cl.m_weapon_id == SSG08) {
				// back.
				if (g_menu.main.aimbot.multipoint_scout.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
			else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20) {
				// back.
				if (g_menu.main.aimbot.multipoint_auto.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
			else if (g_cl.m_weapon_id == AWP) {
				// back.
				if (g_menu.main.aimbot.multipoint_awp.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
			else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER) {
				// back.
				if (g_menu.main.aimbot.multipoint_heavy.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
			else {
				// back.
				if (g_menu.main.aimbot.multipoint_general.get(2))
					points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
		}

		else if (index == HITBOX_PELVIS || index == HITBOX_UPPER_CHEST) {

			points.push_back(center);

			if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_pistols.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });

					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
			else if (g_cl.m_weapon_id == SSG08) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_scout.get(1))
				{
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });
					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
			else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_auto.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });
					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
			else if (g_cl.m_weapon_id == AWP) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_awp.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });
					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });

				}

			}
			else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_heavy.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });

					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
			else {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_general.get(1)) {}
				points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
				points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });

				points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
			}

		}

		// other stomach/chest hitboxes have 2 points.
		else if (index == HITBOX_THORAX || index == HITBOX_CHEST || index == HITBOX_UPPER_CHEST) {
			// add center.
			points.push_back(center);

			if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_pistols.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });
					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
			else if (g_cl.m_weapon_id == SSG08) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_scout.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });

					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
			else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_auto.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });

					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
			else if (g_cl.m_weapon_id == AWP) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_awp.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });

					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
			else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER) {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_heavy.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });

					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
			else {
				// add extra point on back.
				if (g_menu.main.aimbot.multipoint_general.get(1)) {
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + final_radius });
					points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - final_radius });

					points.push_back({ center.x, bbox->m_maxs.y - final_radius, center.z });
				}
			}
		}

		else if (index == HITBOX_R_CALF || index == HITBOX_L_CALF) {
			// add center.
			points.push_back(center);

			if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A) {
				// half bottom.
				if (g_menu.main.aimbot.multipoint_pistols.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}
			else if (g_cl.m_weapon_id == SSG08) {
				// half bottom.
				if (g_menu.main.aimbot.multipoint_scout.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}
			else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20) {
				// half bottom.
				if (g_menu.main.aimbot.multipoint_auto.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}
			else if (g_cl.m_weapon_id == AWP) {
				// half bottom.
				if (g_menu.main.aimbot.multipoint_awp.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}
			else if (g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER) {
				// half bottom.
				if (g_menu.main.aimbot.multipoint_heavy.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}
			else {
				// half bottom.
				if (g_menu.main.aimbot.multipoint_general.get(3))
					points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
			}
		}

		else if (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH) {
			// add center.
			points.push_back(center);
		}

		// arms get only one point.
		else if (index == HITBOX_R_UPPER_ARM || index == HITBOX_L_UPPER_ARM) {
			// elbow.
			points.push_back({ bbox->m_maxs.x + bbox->m_radius, center.y, center.z });
		}

		// nothing left to do here.
		if (points.empty())
			return false;

		// transform capsule points.
		for (auto& p : points)
			math::VectorTransform(p, bones[bbox->m_bone], p);
	}

	return true;
}

bool AimPlayer::GetBestAimPosition(vec3_t& aim, float& damage, LagRecord* record) {
	bool                  done, pen;
	float                 dmg, pendmg;
	HitscanData_t         scan;
	std::vector< vec3_t > points;

	// get player hp.
	int hp = std::min(100, m_player->m_iHealth());
	int half_hp = hp / 2;
	if (g_cl.m_weapon_id == ZEUS) {
		dmg = pendmg = hp;
		pen = true;
	}

	else {

		if (g_cl.m_weapon_id == GLOCK || g_cl.m_weapon_id == P2000 || g_cl.m_weapon_id == USPS || g_cl.m_weapon_id == ELITE || g_cl.m_weapon_id == P250 || g_cl.m_weapon_id == TEC9 || g_cl.m_weapon_id == CZ75A) {
			dmg = g_menu.main.aimbot.minimal_damage_pistols.get();
			if (g_menu.main.aimbot.minimal_damage_pistols.get() > 100)
				dmg = std::ceil((hp + dmg) - 100);

				pendmg = g_menu.main.aimbot.minimal_damage_pistols.get();
				if (g_menu.main.aimbot.minimal_damage_pistols.get() > 100)
					pendmg = std::ceil((hp + pendmg) - 100);

				pen = g_menu.main.aimbot.penetrate_pistols.get();
			}
		else if (g_cl.m_weapon_id == SSG08) {
			dmg = g_menu.main.aimbot.minimal_damage_scout.get();
			if (g_menu.main.aimbot.minimal_damage_scout.get() > 100)
				dmg = std::ceil((hp + dmg) - 100);

				pendmg = g_menu.main.aimbot.minimal_damage_scout.get();
				if (g_menu.main.aimbot.minimal_damage_scout.get() > 100)
					pendmg = std::ceil((hp + pendmg) - 100);

				pen = g_menu.main.aimbot.penetrate_scout.get();
			}
			else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20) {
				dmg = g_menu.main.aimbot.minimal_damage_auto.get();
				if (g_menu.main.aimbot.minimal_damage_auto.get() > 100)
					dmg = std::ceil((hp + dmg) - 100);

				pendmg = g_menu.main.aimbot.minimal_damage_auto.get();
				if (g_menu.main.aimbot.minimal_damage_auto.get() > 100)
					pendmg = std::ceil((hp + pendmg) - 100);

				pen = g_menu.main.aimbot.penetrate_auto.get();
			}
			else if (g_cl.m_weapon_id == AWP) {
				dmg = g_menu.main.aimbot.minimal_damage_awp.get();
				if (g_menu.main.aimbot.minimal_damage_awp.get() > 100)
					dmg = std::ceil((hp + dmg) - 100);

				pendmg = g_menu.main.aimbot.minimal_damage_awp.get();
				if (g_menu.main.aimbot.minimal_damage_awp.get() > 100)
					pendmg = std::ceil((hp + pendmg) - 100);

				pen = g_menu.main.aimbot.penetrate_awp.get();
			}
			else if (g_cl.m_weapon_id == g_cl.m_weapon_id == DEAGLE || g_cl.m_weapon_id == REVOLVER) {
				dmg = g_menu.main.aimbot.minimal_damage_heavy.get();
				if (g_menu.main.aimbot.minimal_damage_heavy.get() > 100)
					dmg = std::ceil((hp + dmg) - 100);

				pendmg = g_menu.main.aimbot.minimal_damage_heavy.get();
				if (half_hp < pendmg)
					if (g_menu.main.aimbot.minimal_damage_heavy.get() > 100)
						pendmg = std::ceil((hp + pendmg) - 100);

				pen = g_menu.main.aimbot.penetrate_heavy.get();
			}
			else {
				dmg = g_menu.main.aimbot.minimal_damage_general.get();
				if (g_menu.main.aimbot.minimal_damage_general.get() > 100)
					dmg = std::ceil((hp + dmg) - 100);

				pendmg = g_menu.main.aimbot.minimal_damage_general.get();
				if (g_menu.main.aimbot.minimal_damage_general.get() > 100)
					pendmg = std::ceil((hp + pendmg) - 100);

				pen = g_menu.main.aimbot.penetrate_general.get();
			}
		}

	// write all data of this record l0l.
	record->cache( );

	// iterate hitboxes.
	for (const auto& it : m_hitboxes) {
		done = false;

		// setup points on hitbox.
		if (!SetupHitboxPoints(record, record->m_bones, it.m_index, points))
			continue;

		// iterate points on hitbox.
		for (const auto& point : points) {
			penetration::PenetrationInput_t in;

			in.m_damage = dmg;
			in.m_damage_pen = pendmg;
			in.m_can_pen = pen;
			in.m_target = m_player;
			in.m_from = g_cl.m_local;
			in.m_pos = point;

			penetration::PenetrationOutput_t out;

			// we can hit p!
			if (penetration::run(&in, &out)) {

				// nope we did not hit head..
				if (it.m_index == HITBOX_HEAD && out.m_hitgroup != HITGROUP_HEAD)
					continue;

				// prefered hitbox, just stop now.
				if (it.m_mode == HitscanMode::PREFER)
					done = true;

				// this hitbox requires lethality to get selected, if that is the case.
				// we are done, stop now.
				else if (it.m_mode == HitscanMode::LETHAL && out.m_damage >= m_player->m_iHealth())
					done = true;

				// 2 shots will be sufficient to kill.
				else if (it.m_mode == HitscanMode::LETHAL2 && (out.m_damage * 2.f) >= m_player->m_iHealth())
					done = true;

				// this hitbox has normal selection, it needs to have more damage.
				else if (it.m_mode == HitscanMode::NORMAL) {
					// we did more damage.
					if (out.m_damage > scan.m_damage) {
						// save new best data.
						scan.m_damage = out.m_damage;
						scan.m_pos = point;

						// if the first point is lethal
						// screw the other ones.
						if (point == points.front() && out.m_damage >= m_player->m_iHealth())
							break;
					}
				}
				// we found a preferred / lethal hitbox.
				if (done) {
					// save new best data.
					scan.m_damage = out.m_damage;
					scan.m_pos = point;

					break;
				}
			}
		}

		// ghetto break out of outer loop.
		if (done)
			break;
	}

	// we found something that we can damage.
	// set out vars.
	if ( scan.m_damage > 0.f ) {
		aim = scan.m_pos;
		damage = scan.m_damage;
		return true;
	}

	return false;
}

bool Aimbot::SelectTarget( LagRecord *record, const vec3_t &aim, float damage ) {
	float dist, fov, height;
	int   hp;

	// fov check.
	if ( g_menu.main.aimbot.fov.get( ) ) {
		// if out of fov, retn false.
		if ( math::GetFOV( g_cl.m_view_angles, g_cl.m_shoot_pos, aim ) > g_menu.main.aimbot.fov_amount.get( ) )
			return false;
	}

	switch ( g_menu.main.aimbot.selection.get( ) ) {

		// distance.
	case 0:
		dist = ( record->m_pred_origin - g_cl.m_shoot_pos ).length( );

		if ( dist < m_best_dist ) {
			m_best_dist = dist;
			return true;
		}

		break;

		// crosshair.
	case 1:
		fov = math::GetFOV( g_cl.m_view_angles, g_cl.m_shoot_pos, aim );

		if ( fov < m_best_fov ) {
			m_best_fov = fov;
			return true;
		}

		break;

		// damage.
	case 2:
		if ( damage > m_best_damage ) {
			m_best_damage = damage;
			return true;
		}

		break;

		// lowest hp.
	case 3:
		// fix for retarded servers?
		hp = std::min( 100, record->m_player->m_iHealth( ) );

		if ( hp < m_best_hp ) {
			m_best_hp = hp;
			return true;
		}

		break;

		// least lag.
	case 4:
		if ( record->m_lag < m_best_lag ) {
			m_best_lag = record->m_lag;
			return true;
		}

		break;

		// height.
	case 5:
		height = record->m_pred_origin.z - g_cl.m_local->m_vecOrigin( ).z;

		if ( height < m_best_height ) {
			m_best_height = height;
			return true;
		}

		break;

	default:
		return false;
	}

	return false;
}

void Aimbot::apply( ) {
	bool attack, attack2;

	// attack states.
	attack = ( g_cl.m_cmd->m_buttons & IN_ATTACK );
	attack2 = ( g_cl.m_weapon_id == REVOLVER && g_cl.m_cmd->m_buttons & IN_ATTACK2 );

	// ensure we're attacking.
	if ( attack || attack2 ) {
		// choke every shot.
		*g_cl.m_packet = false;

		if ( m_target ) {
			// make sure to aim at un-interpolated data.
			// do this so BacktrackEntity selects the exact record.
			if ( m_record && !m_record->m_broke_lc )
				g_cl.m_cmd->m_tick = game::TIME_TO_TICKS( m_record->m_sim_time + g_cl.m_lerp );

			// set angles to target.
			g_cl.m_cmd->m_view_angles = m_angle;

			// if not silent aim, apply the viewangles.
			if ( !g_menu.main.aimbot.silent.get( ) )
				g_csgo.m_engine->SetViewAngles( m_angle );

			g_visuals.DrawHitboxMatrix( m_record, colors::white, 10.f );
		}

		// nospread.
		if ( g_menu.main.aimbot.nospread.get( ) && g_menu.main.config.mode.get( ) == 1 )
			NoSpread( );

		// norecoil.
		if ( g_menu.main.aimbot.norecoil.get( ) )
			g_cl.m_cmd->m_view_angles -= g_cl.m_local->m_aimPunchAngle( ) * g_csgo.weapon_recoil_scale->GetFloat( );

		// store fired shot.
		g_shots.OnShotFire( m_target ? m_target : nullptr, m_target ? m_damage : -1.f, g_cl.m_weapon_info->m_bullets, m_target ? m_record : nullptr );

		// set that we fired.
		g_cl.m_shot = true;
	}
}

void Aimbot::NoSpread( ) {
	bool    attack2;
	vec3_t  spread, forward, right, up, dir;

	// revolver state.
	attack2 = ( g_cl.m_weapon_id == REVOLVER && ( g_cl.m_cmd->m_buttons & IN_ATTACK2 ) );

	// get spread.
	spread = g_cl.m_weapon->CalculateSpread( g_cl.m_cmd->m_random_seed, attack2 );

	// compensate.
	g_cl.m_cmd->m_view_angles -= { -math::rad_to_deg( std::atan( spread.length_2d( ) ) ), 0.f, math::rad_to_deg( std::atan2( spread.x, spread.y ) ) };
}