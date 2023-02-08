#pragma once

struct matrices_t {
    int                         ent_index;
    ModelRenderInfo_t           info;
    DrawModelState_t            state;
    matrix3x4_t                 pBoneToWorld[128] = { };
    float                       time;
    matrix3x4_t                 model_to_world;
};

struct OffScreenDamageData_t {
    float m_time, m_color_step;
    Color m_color;

    __forceinline OffScreenDamageData_t( ) : m_time{ 0.f }, m_color{ colors::white } {}
    __forceinline OffScreenDamageData_t( float time, float m_color_step, Color color ) : m_time{ time }, m_color{ color } {}
};

struct impact_info {
    float x, y, z;
    float time;
};

struct client_hit_verify_t {
    vec3_t pos;
    float time;
    float expires;
};

class Visuals {
public:
	std::array< bool, 64 >                  m_draw;
	std::array< float, 2048 >               m_opacities;
    std::array< OffScreenDamageData_t, 64 > m_offscreen_damage;
	vec2_t                                  m_crosshair;
	bool                                    m_thirdperson;
	float					                m_hit_start, m_hit_end, m_hit_duration;

    std::vector< matrices_t >               m_hit_matrix;
    std::vector<impact_info>                impacts;

    // info about planted c4.
    bool        m_c4_planted;
    Entity      *m_planted_c4;
    float       m_planted_c4_explode_time;
    vec3_t      m_planted_c4_explosion_origin;
    float       m_planted_c4_damage;
    float       m_planted_c4_radius;
    float       m_planted_c4_radius_scaled;
    std::string m_last_bombsite;

	IMaterial* smoke1;
	IMaterial* smoke2;
	IMaterial* smoke3;
	IMaterial* smoke4;

    std::unordered_map< int, char > m_weapon_icons = {
    { DEAGLE, 'A' },
    { ELITE, 'B' },
    { FIVESEVEN, 'C' },
    { GLOCK, 'D' },
    { P2000, 'E' },
    { P250, 'F' },
    { USPS, 'G' },
    { TEC9, 'H' },
    { CZ75A, 'I' },
    { REVOLVER, 'J' },
    { MAC10, 'K' },
    { UMP45, 'L' },
    { BIZON, 'M' },
    { MP7, 'N' },
    { MP9, 'O' },
    { P90, 'P' },
    { GALIL, 'Q' },
    { FAMAS, 'R' },
    { M4A4, 'S' },
    { M4A1S, 'T' },
    { AUG, 'U' },
    { SG553, 'V' },
    { AK47, 'W' },
    { G3SG1, 'X' },
    { SCAR20, 'Y' },
    { AWP, 'Z' },
    { SSG08, 'a' },
    { XM1014, 'b' },
    { SAWEDOFF, 'c' },
    { MAG7, 'd' },
    { NOVA, 'e' },
    { NEGEV, 'f' },
    { M249, 'g' },
    { ZEUS, 'h' },
    { KNIFE_T, 'i' },
    { KNIFE_CT, 'j' },
    { KNIFE_FALCHION, '0' },
    { KNIFE_BAYONET, '1' },
    { KNIFE_FLIP, '2' },
    { KNIFE_GUT, '3' },
    { KNIFE_KARAMBIT, '4' },
    { KNIFE_M9_BAYONET, '5' },
    { KNIFE_HUNTSMAN, '6' },
    { KNIFE_BOWIE, '7' },
    { KNIFE_BUTTERFLY, '8' },
    { FLASHBANG, 'k' },
    { HEGRENADE, 'l' },
    { SMOKE, 'm' },
    { MOLOTOV, 'n' },
    { DECOY, 'o' },
    { FIREBOMB, 'p' },
    { C4, 'q' },
    };

    std::string GetWeaponIcon(const int id) {
        auto search = m_weapon_icons.find(id);
        if (search != m_weapon_icons.end())
            return std::string(&search->second, 1);

        return "";
    }
public:
	static void ModulateWorld( );
	void ThirdpersonThink( );
    void ImpactData();
	void Hitmarker( );
	void NoSmoke( );
	void think( );
	void Spectators( );
	void StatusIndicators( );
	void SpreadCrosshair( );
    void PenetrationCrosshair( );
    void DrawPlantedC4();
	void draw( Entity* ent );
	void DrawProjectile( Weapon* ent );
	void DrawItem( Weapon* item );
	void OffScreen( Player* player, int alpha );
	void DrawPlayer( Player* player );
	bool GetPlayerBoxRect( Player* player, Rect& box );
	void DrawHistorySkeleton( Player* player, int opacity );
	void DrawSkeleton( Player* player, int opacity );
	void RenderGlow( );
    void override_material(bool ignoreZ, bool use_env, Color& color, IMaterial* material);
    void on_post_screen_effects();
	void DrawHitboxMatrix( LagRecord* record, Color col, float time );
    void DrawBeams( );
	void DebugAimbotPoints( Player* player );
};

extern Visuals g_visuals;