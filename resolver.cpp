#include "includes.h"

Resolver g_resolver{};;

LagRecord* Resolver::FindIdealRecord(AimPlayer* data) {
	LagRecord* first_valid = nullptr;

	if (data->m_records.empty())
		return nullptr;

	for (const auto& record : data->m_records) {
		if (record->dormant() || record->immune() || !record->valid())
			continue;

		if (record->m_shot || record->m_mode == Modes::RESOLVE_BODY || record->m_mode == Modes::RESOLVE_WALK || record->m_mode == Modes::RESOLVE_NONE)
			return record.get();

		if (!first_valid)
			first_valid = record.get();
	}

	return first_valid;
}


LagRecord* Resolver::FindLastRecord(AimPlayer* data) {
	if (data->m_records.empty())
		return nullptr;

	
		
		// Iterate records in reverse to find the last valid record.
		for (auto it = data->m_records.crbegin(); it != data->m_records.crend(); ++it) {
			LagRecord* current = it->get();
			if (current->valid() && !current->immune() && !current->dormant())
				return current;
		}

	return nullptr;
}




void Resolver::OnBodyUpdate( Player* player, float value ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// set data.
	data->m_old_body = data->m_body;
	data->m_body     = value;
}

float Resolver::GetAwayAngle(LagRecord* record)
{
	vec3_t local_origin = g_cl.m_local->m_vecOrigin();
	vec3_t target_origin = record->m_pred_origin;

	vec3_t direction = target_origin - local_origin;
	direction.normalize();

	float angle = atan2(direction.y, direction.x);
	return angle;
}

void Resolver::MatchShot(AimPlayer* data, LagRecord* record)
{
	// do not attempt to do this in nospread mode.
	if (g_menu.main.config.mode.get() == 1)
		return;

	Weapon* weapon = data->m_player->GetActiveWeapon();
	if (!weapon)
		return;

	float shoot_time = weapon->m_fLastShotTime() + g_csgo.m_globals->m_interval;

	// this record has a shot on it.
	if (game::TIME_TO_TICKS(shoot_time) == game::TIME_TO_TICKS(record->m_sim_time))
	{
		// if the player's lag is 2 or less, mark this record as shot
		if (record->m_lag <= 2)
		{
			record->m_shot = true;
		}
		else
		{
			// if the player has at least 2 lag records, use the previous pitch
			// (since more than 1 choke means the player cannot hit the pitch)
			if (data->m_records.size() >= 2)
			{
				LagRecord* previous = data->m_records[1].get();
				if (previous && !previous->dormant())
					record->m_eye_angles.x = previous->m_eye_angles.x;
			}
		}
	}
}


void Resolver::SetMode( LagRecord* record ) {
	// the resolver has 3 modes to chose from.
	// these modes will vary more under the hood depending on what data we have about the player
	// and what kind of hack vs. hack we are playing (mm/nospread).

	float speed = record->m_anim_velocity.length( );

	// if on ground, moving, and not fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && speed > 0.1f && !record->m_fake_walk )
		record->m_mode = Modes::RESOLVE_WALK;

	// if on ground, not moving or fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && ( speed <= 0.1f || record->m_fake_walk ) )
		record->m_mode = Modes::RESOLVE_STAND;

	// if not on ground.
	else if( !( record->m_flags & FL_ONGROUND ) )
		record->m_mode = Modes::RESOLVE_AIR;
}

void Resolver::ResolveAngles( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// mark this record if it contains a shot.
	MatchShot( data, record );

	// next up mark this record with a resolver mode that will be used.
	SetMode( record );

	// if we are in nospread mode, force all players pitches to down.
	// TODO; we should check thei actual pitch and up too, since those are the other 2 possible angles.
	// this should be somehow combined into some iteration that matches with the air angle iteration.
	if( g_menu.main.config.mode.get( ) == 1 )
		record->m_eye_angles.x = 90.f;

	// we arrived here we can do the acutal resolve.
	if( record->m_mode == Modes::RESOLVE_WALK ) 
		ResolveWalk( data, record );

	else if( record->m_mode == Modes::RESOLVE_STAND )
		ResolveStand( data, record, player );

	else if( record->m_mode == Modes::RESOLVE_AIR )
		ResolveAir( data, record, player );

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle( record->m_eye_angles.y );
}

void Resolver::ResolveWalk(AimPlayer* data, LagRecord* record) {
	record->m_eye_angles.y = record->m_body;
	record->m_mode = RESOLVE_WALK;


		// Delay body update
		data->m_body_update = record->m_anim_time + 0.22f;

	// Reset indices
	data->m_moving_index = 0;
	data->m_stand_index = 0;
	data->m_stand_index2 = 0;
	data->m_body_index = 0;
	data->m_freestanding_index = 0;
	data->m_lby_delta_index = 0;
	data->m_unknown_move = 0;
	data->m_last_move = 0;
	data->m_missed_shots = 0;

	// Store record for later use
	std::memcpy(&data->m_walk_record, record, sizeof(LagRecord));
}

bool CheckLBY(Player* player, LagRecord* record, LagRecord* prev_record)
{
	// Check player's velocity
	if (player->m_vecVelocity().length_2d() > 1.1f)
		return false;
	// Check if player is choking (stuttering)
	bool choking = fabs(player->m_flSimulationTime() - player->m_flOldSimulationTime()) > g_csgo.m_globals->m_interval;

	for (int i = 0; i < 13; i++)
	{
		auto layer = record->m_layers[i];
		auto prev_layer = prev_record->m_layers[i];

		// Check if the animation changed
		if (layer.m_cycle != prev_layer.m_cycle)
		{
			if (layer.m_cycle > 0.9 || layer.m_weight == 1.f) // Triggered layer
			{
				if (i == 3) // LBY adjust layer
					return true;

				// Check for LBY flick
				if (choking && fabs(math::NormalizedAngle(record->m_body - prev_record->m_body)) > 5.f)
					return true;
			}
			else if (choking) // Improper LBY breakers
			{
				if (player->GetSequenceActivity(layer.m_sequence) == 979)
				{
					if (player->GetSequenceActivity(prev_layer.m_sequence) == 979)
					{
						return true;
					}
				}
			}
			return false;
		}
		return false;
	}
	return false;
}

bool Resolver::Spin_Detection(AimPlayer* data) {
	if (data->m_records.empty()) return false;
	size_t size = 0;
	for (const auto& it : data->m_records) {
		if (it->dormant()) break;
		++size;
	}
	if (size > 2) {
		LagRecord* record = data->m_records[0].get();
		spindelta = (record->m_body - data->m_records[1].get()->m_body) / data->m_records[1].get()->m_lag;
		spinbody = record->m_body;
		return spindelta == (data->m_records[1].get()->m_body - data->m_records[2].get()->m_body) / data->m_records[2].get()->m_lag && spindelta > 0.5f;
	}
	return false;
}

void Resolver::ResolveStand(AimPlayer* data, LagRecord* record, Player* player) {
	data->m_moved = false;

	// For no-spread, call a separate resolver
	if (g_menu.main.config.mode.get() == 1) {
		StandNS(data, record);
		return;
	}

	LagRecord* move = &data->m_walk_record;
	if (move->m_sim_time > 0.f && !move->dormant() && !record->dormant() && data->m_last_move < 1) {
		vec3_t delta = move->m_origin - record->m_origin;
		if (delta.length() <= 100.f) {
			data->m_moved = true;
		}
	}

	// Check if the player is breaking their lby
	bool breaking = CheckLBY(data->m_player, record, FindLastRecord(data));

	// If we found a valid moving context
	if (data->m_moved) {
		float diff = math::NormalizedAngle(record->m_body - move->m_body);
		float delta = record->m_anim_time - move->m_anim_time;

		// First time resolving this player's movement
		if (data->m_last_move < 1) {
			record->m_mode = Modes::RESOLVE_LASTMOVE;
			record->m_eye_angles.y = move->m_body;
		}

		// Resolve the player's body update
		if (data->m_records.size() >= 2) {
			LagRecord* previous = data->m_records[1].get();
			if (previous) {
				if (record->m_body != previous->m_body && data->m_body_index < 1) {
					record->m_eye_angles.y = record->m_body;
					data->m_body_update = record->m_anim_time + 1.1f;
					iPlayers[record->m_player->index()] = false;
					record->m_mode = Modes::RESOLVE_BODY;
				}
			}
		}
	}
	else {
		AntiFreestand(record);
	}
}
void Resolver::StandNS( AimPlayer* data, LagRecord* record ) {
	// get away angles.
	float away = GetAwayAngle( record );

	switch( data->m_shots % 8 ) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 90.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 45.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 45.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 0.f;
		break;

	default:
		break;
	}

	// force LBY to not fuck any pose and do a true bruteforce.
	record->m_body = record->m_eye_angles.y;
}

void Resolver::ResolveAir(AimPlayer* data, LagRecord* record, Player* player) {
	if (g_menu.main.config.mode.get() == 1) {
		AirNS(data, record);
		return;
	}

	if (record->m_velocity.length_2d() < 60.f) {
		record->m_mode = Modes::RESOLVE_STAND;
		ResolveStand(data, record, player);
		return;
	}

	float velocity_yaw = math::rad_to_deg(std::atan2(record->m_velocity.y, record->m_velocity.x));
	int shot_mod = data->m_shots % 3;
	switch (shot_mod) {
	case 0:
		record->m_eye_angles.y = velocity_yaw + 180.f;
		break;
	case 1:
		record->m_eye_angles.y = velocity_yaw - 90.f;
		break;
	case 2:
		record->m_eye_angles.y = velocity_yaw + 90.f;
		break;
	}
}
void Resolver::AirNS( AimPlayer* data, LagRecord* record ) {
	// get away angles.
	float away = GetAwayAngle( record );

	switch( data->m_shots % 9 ) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 150.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 150.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 165.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 165.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 8:
		record->m_eye_angles.y = away - 90.f;
		break;

	default:
		break;
	}
}

void Resolver::ResolvePoses(Player* player, LagRecord* record) {
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

		if (record->m_mode == Modes::RESOLVE_AIR) {
			player->m_flPoseParameter()[2] = g_csgo.RandomFloat(0.0f, 1.0f);
			player->m_flPoseParameter()[11] = g_csgo.RandomFloat(0.25f, 0.75f);
		}
}


void Resolver::AntiFreestand(LagRecord* record) {
	// constants
	constexpr float STEP{ 4.f };
	constexpr float RANGE{ 32.f };

	// best target.
	vec3_t enemypos = record->m_player->GetShootPosition();
	float away = GetAwayAngle(record);

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back(away - 179.f);
	angles.emplace_back(away + 89.f);
	angles.emplace_back(away - 89.f);

	// start the trace at the your shoot pos.
	vec3_t start = g_cl.m_local->GetShootPosition();

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };

	// iterate vector of angles.
	for (auto it = angles.begin(); it != angles.end(); ++it) {

		// compute the 'rough' estimation of where our head will be.
		vec3_t end{ enemypos.x + std::cos(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemypos.y + std::sin(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemypos.z };

		// draw a line for debugging purposes
		/*Color clr = g_menu.main.aimbot.debugaimcol.get();
			g_csgo.m_debug_overlay->AddLineOverlay(start, end, clr.r(), clr.g(), clr.b(), true, 0.1f); */

			// compute the direction.
		vec3_t dir = end - start;
		float len = dir.normalize();

		// should never happen.
		if (len <= 0.f)
			continue;

		// step thru the total distance, 4 units per step.
		for (float i{ 0.f }; i < len; i += STEP) {
			// get the current step position.
			vec3_t point = start + (dir * i);

			// get the contents at this point.
			int contents = g_csgo.m_engine_trace->GetPointContents(point, MASK_SHOT_HULL);

			// contains nothing that can stop a bullet.
			if (!(contents & MASK_SHOT_HULL))
				continue;

			float mult = 1.f;

			// over 50% of the total length, prioritize this shit.
			if (i > (len * 0.5f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.75f))
				mult = 1.55f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.9f))
				mult = 1.75f;

			// append 'penetrated distance'. 
			it->m_dist += (STEP * mult);

			// mark that we found anything.
			valid = true;
		}
	}

	if (!valid) {
		return;
	}

	// put the most distance at the front of the container.a
	std::sort(angles.begin(), angles.end(),
		[](const AdaptiveAngle& a, const AdaptiveAngle& b) {
			return a.m_dist > b.m_dist;
		});

	// the best angle should be at the front now.

	AdaptiveAngle* best = &angles.front();

	record->m_mode = Modes::RESOLVE_FREESTAND; // set our mode accordingly 
	record->m_eye_angles.y = best->m_yaw;
}
