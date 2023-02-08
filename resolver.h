#pragma once

class ShotRecord;

struct AntiFreestandingRecord
{
	int right_damage = 0, left_damage = 0, back_damage = 0;
	float right_fraction = 0.f, left_fraction = 0.f, back_fraction = 0.f;
};

class Resolver {
public:
	enum Modes : size_t {
		RESOLVE_NONE = 0,
		RESOLVE_WALK,
		RESOLVE_STAND,
		RESOLVE_AIR,
		RESOLVE_LBY_UPDATE,
		RESOLVE_OVERRIDE,
		RESOLVE_LAST_LBY,
		RESOLVE_BRUTEFORCE,
		RESOLVE_FREESTAND,
		RESOLVE_DELTA,
		RESOLVE_BODY,
		RESOLVE_STOPPED_MOVING,
		RESOLVE_UNKNOWM,
		RESOLVE_LASTMOVE,
		RESOLVE_STAND2,
		RESOLVE_STAND1
	};

public:
	LagRecord* FindIdealRecord(AimPlayer* data);
	LagRecord* FindLastRecord(AimPlayer* data);

	LagRecord* FindFirstRecord(AimPlayer* data);

	void OnBodyUpdate(Player* player, float value);
	float GetAwayAngle(LagRecord* record);

	void MatchShot(AimPlayer* data, LagRecord* record);
	void SetMode(LagRecord* record);
	bool Spin_Detection(AimPlayer* data);

	void ResolveAngles(Player* player, LagRecord* record);
	void ResolveWalk(AimPlayer* data, LagRecord* record);
	void ResolveStand(AimPlayer* data, LagRecord* record, Player* player);
	void StandNS(AimPlayer* data, LagRecord* record);
	void ResolveAir(AimPlayer* data, LagRecord* record, Player* player);

	void AirNS(AimPlayer* data, LagRecord* record);
	void ResolvePoses(Player* player, LagRecord* record);
	void AntiFreestand(LagRecord* record);

	void SupremAntiFreestanding(LagRecord* record);

	float GetLBYRotatedYaw(float lby, float yaw);

	bool IsYawSideways(Player* entity, float yaw);

	void SupremAntiFreestandingReversed(LagRecord* record);

	bool AntiFreestanding(Player* entity, AimPlayer* data, float& yaw);

	void MainResolver(LagRecord* record, AimPlayer* data, Player* player);

	void collect_wall_detect(const Stage_t stage);

	AntiFreestandingRecord anti_freestanding_record;



	float spindelta;
	float spinbody;
	int spin_step;

	bool is_flicking;

	int m_iMode;

	vec3_t last_eye;

	float left_damage[64];
	float right_damage[64];
	float back_damage[64];

public:
	std::array< vec3_t, 64 > m_impacts;
	int	   iPlayers[64];
	std::vector<vec3_t> last_eye_positions;
};

class PlayerResolveRecord
{
public:
	struct AntiFreestandingRecord
	{
		int right_damage = 0, left_damage = 0;
		float right_fraction = 0.f, left_fraction = 0.f;
	};
};

extern Resolver g_resolver;