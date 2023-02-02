#pragma once

namespace callbacks {
	void SkinUpdate( );
	void ForceFullUpdate( );
	void ToggleThirdPerson( );
	void ToggleFakeLatency( );
    void ToggleKillfeed( );
	bool IsUsingBulletImpacts();
	void SaveHotkeys( );
	void ConfigLoad1( );
	void ConfigLoad2( );
	void ConfigLoad3( );
	void ConfigLoad4( );
	void ConfigLoad5( );
	void ConfigLoad6( );
	void ConfigLoad( );
	void ConfigSave( );
	bool IsWeaponGeneral();
	bool IsWeaponAuto();
	bool IsWeaponAWP();
	bool IsWeaponScout();
	bool IsWeaponPistol();
	bool IsWeaponHeavy();
	bool IsBaimHealth();
	bool IsCorrecting();
	bool IsAfterMisses();
	bool IsFovOn();
	bool IsHitchanceOnGeneral();
	bool IsHitchanceOnAuto();
	bool IsHitchanceOnAWP();
	bool IsHitchanceOnScout();
	bool IsHitchanceOnPistols();
	bool IsHitchanceOnHeavy();
	bool IsCustomOn();

	bool IsPenetrationOnGeneral();
	bool IsMultipointOnGeneral();
	bool IsMultipointBodyOnGeneral();

	bool IsPenetrationOnAuto();
	bool IsMultipointOnAuto();
	bool IsMultipointBodyOnAuto();

	bool IsPenetrationOnAWP();
	bool IsMultipointOnAWP();
	bool IsMultipointBodyOnAWP();

	bool IsPenetrationOnScout();
	bool IsMultipointOnScout();
	bool IsMultipointBodyOnScout();

	bool IsPenetrationOnPistols();
	bool IsMultipointOnPistols();
	bool IsMultipointBodyOnPistols();

	bool IsPenetrationOnHeavy();
	bool IsMultipointOnHeavy();
	bool IsMultipointBodyOnHeavy();

	bool IsAntiAimModeStand( );
	bool HasStandYaw( );
	bool IsStandYawJitter( );
	bool IsStandYawRotate( );
	bool IsStandYawRnadom( );
	bool IsStandDirAuto( );
	bool IsStandDirCustom( );
	bool IsAntiAimModeWalk( );
	bool WalkHasYaw( );
	bool IsWalkYawJitter( );
	bool IsWalkYawRotate( );
	bool IsWalkYawRandom( );
	bool IsWalkDirAuto( );
	bool IsWalkDirCustom( );
	bool IsAntiAimModeAir( );
	bool AirHasYaw( );
	bool IsAirYawJitter( );
	bool IsAirYawRotate( );
	bool IsAirYawRandom( );
	bool IsAirDirAuto( );
	bool IsAirDirCustom( );
	bool IsFakeAntiAimRelative( );
	bool IsFakeAntiAimJitter( );

	
	bool IsConfigMM( );
	bool IsConfigNS( );
	bool IsConfig1( );
	bool IsConfig2( );
	bool IsConfig3( );
	bool IsConfig4( );
	bool IsConfig5( );
	bool IsConfig6( );

	// weapon cfgs.
	bool DEAGLE( );
	bool ELITE( );
	bool FIVESEVEN( );
	bool GLOCK( );
	bool AK47( );
	bool AUG( );
	bool AWP( );
	bool FAMAS( );
	bool G3SG1( );
	bool GALIL( );
	bool M249( );
	bool M4A4( );
	bool MAC10( );
	bool P90( );
	bool UMP45( );
	bool XM1014( );
	bool BIZON( );
	bool MAG7( );
	bool NEGEV( );
	bool SAWEDOFF( );
	bool TEC9( );
	bool P2000( );
	bool MP7( );
	bool MP9( );
	bool NOVA( );
	bool P250( );
	bool SCAR20( );
	bool SG553( );
	bool SSG08( );
	bool M4A1S( );
	bool USPS( );
	bool CZ75A( );
	bool REVOLVER( );
	bool KNIFE_BAYONET( );
	bool KNIFE_FLIP( );
	bool KNIFE_GUT( );
	bool KNIFE_KARAMBIT( );
	bool KNIFE_M9_BAYONET( );
	bool KNIFE_HUNTSMAN( );
	bool KNIFE_FALCHION( );
	bool KNIFE_BOWIE( );
	bool KNIFE_BUTTERFLY( );
	bool KNIFE_SHADOW_DAGGERS( );
	bool AUTO_STOP( );
}