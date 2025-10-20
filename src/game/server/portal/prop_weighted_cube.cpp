#include "cbase.h"
#include "prop_weighted_cube.h"
#include "datacache/imdlcache.h"
#include "trigger_portal_cleanser.h"
#include "portal_player.h"
#include "portal/weapon_physcannon.h"

#define BOX_MODEL			"models/props/metal_box.mdl"
#define BOX_REFLECT_MODEL	"models/props/reflection_cube.mdl"
#define BOX_SPHERE_MODEL	"models/props/sphere.mdl"

BEGIN_DATADESC( CPropWeightedCube )

	DEFINE_OUTPUT( m_OnFizzled, "OnFizzled" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Dissolve", InputDissolve ),
	
	DEFINE_KEYFIELD( m_nCubeType, FIELD_INTEGER, "CubeType" ),

END_DATADESC()

//IMPLEMENT_SERVERCLASS_ST( CPropWeightedCube, DT_PropWeightedCube )
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( prop_weighted_cube, CPropWeightedCube );

CPropWeightedCube::CPropWeightedCube() : 
				   m_nCubeType( CUBE_STANDARD )
{
	m_spawnflags |= ( SF_PHYSPROP_DONT_TAKE_PHYSICS_DAMAGE | SF_PHYSPROP_ENABLE_ON_PHYSCANNON | SF_PHYSPROP_ENABLE_PICKUP_OUTPUT );
}

void CPropWeightedCube::Spawn( void )
{
	SetModelName( castable_string_t( BOX_MODEL ) );

	Precache();
	
	SetCubeType();

	BaseClass::Spawn();
}

void CPropWeightedCube::Precache( void )
{
	BaseClass::Precache();
	
	switch ( m_nCubeType )
	{
	default:
	case CUBE_STANDARD:
	case CUBE_COMPANION:
		PrecacheModel( BOX_MODEL );
		break;

	case CUBE_REFLECTIVE:
		PrecacheModel( BOX_REFLECT_MODEL );
		break;

	case CUBE_SPHERE:
		PrecacheModel( BOX_SPHERE_MODEL );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::SetCubeType( void )
{	
	switch( m_nCubeType )
	{
		//Standard cube
		case CUBE_STANDARD:
		case CUBE_COMPANION:
		{
			SetModelName( MAKE_STRING( BOX_MODEL ) );
			
			if ( m_nCubeType == CUBE_COMPANION )
			{
				m_nSkin = 1;
			}
			break;
		}
		
		//Reflective cube
		case CUBE_REFLECTIVE:
		{
			SetModelName( MAKE_STRING( BOX_REFLECT_MODEL ) );
			AddSpawnFlags( SF_PHYSPROP_ENABLE_ON_PHYSCANNON );
			break;
		}
		
		//Sphere
		case CUBE_SPHERE:
		{
			SetModelName( MAKE_STRING( BOX_SPHERE_MODEL ) );
			break;
		}
	}
}

void CPropWeightedCube::InputDissolve( inputdata_t &inputdata )
{
	CTriggerPortalCleanser::FizzleBaseAnimating( NULL, this );
}

bool CPropWeightedCube::ShouldCollide( int collisionGroup, int contentsMask ) const
{
#if 1
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj && ( pObj->GetGameFlags() & FVPHYSICS_PLAYER_HELD ) )
	{
		if ( collisionGroup == COLLISION_GROUP_PLAYER || collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
		{
			extern bool HeldObjectShouldHitPlayer( CPortal_Player *pPlayer );
			if ( !HeldObjectShouldHitPlayer( (CPortal_Player*)UTIL_GetLocalPlayer() ) )
			{
				// Held objects shouldn't collide with players 
				// BUG: Not sure if we want this in MP, intention is to not collide with the holding player, not necessarily all.
				return false;
			}
		}
	}
#endif
	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

//-----------------------------------------------------------------------------
// Purpose: Only bother with preferred carry angles if we're a reflective cube
//-----------------------------------------------------------------------------
bool CPropWeightedCube::HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer )
{
	//return false; // For now, don't use preferred carry angles
	return m_nCubeType == CUBE_REFLECTIVE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
QAngle CPropWeightedCube::PreferredCarryAngles( void )
{
	CBasePlayer *pPlayer = GetPlayerHoldingEntity( this );
	if ( !pPlayer )
		return vec3_angle;

	float pitch = 0 - pPlayer->EyeAngles()[PITCH];
	pitch = AngleNormalize( pitch );
	pitch -= pPlayer->GetPunchAngle()[PITCH];
	
	return QAngle( pitch, 0, 0 );
}

void CPropWeightedCube::SetLaser( CBaseEntity *pLaser )
{
	m_hLaser = pLaser;

	// need to update transmitstate to prevent laser going through box when box goes outside PVS
	UpdateTransmitState();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool UTIL_IsWeightedCube( CBaseEntity *pEntity )
{
	if ( pEntity == NULL )
		return false;

	return ( FClassnameIs( pEntity, "prop_weighted_cube" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool UTIL_IsReflectiveCube( CBaseEntity *pEntity )
{
	if ( UTIL_IsWeightedCube( pEntity ) == false )
		return false;

	CPropWeightedCube *pCube = assert_cast<CPropWeightedCube*>( pEntity );
	return ( pCube && pCube->GetCubeType() == CUBE_REFLECTIVE );
}

//-----------------------------------------------------------------------------
// Creates a weighted cube of a specific type
//-----------------------------------------------------------------------------
void CPropWeightedCube::CreatePortalWeightedCube( WeightedCubeType_e objectType, bool bAtCursorPosition, const Vector &position )
{
	MDLCACHE_CRITICAL_SECTION();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	// Try to create entity
	CPropWeightedCube *entity = ( CPropWeightedCube* )CreateEntityByName("prop_weighted_cube");
	if (entity)
	{
		//entity->PrecacheModel( PORTAL_REFLECTOR_CUBE_MODEL_NAME );
		//entity->SetModel( PORTAL_REFLECTOR_CUBE_MODEL_NAME );
		entity->SetName( MAKE_STRING("box") );
		entity->AddSpawnFlags( SF_PHYSPROP_ENABLE_PICKUP_OUTPUT );
		entity->m_nCubeType = objectType;
		entity->Precache();

		if ( !bAtCursorPosition )
		{
			entity->SetAbsOrigin( position );
		}

		DispatchSpawn(entity);

		if ( bAtCursorPosition )
		{
			// Now attempt to drop into the world
			CBasePlayer* pPlayer = UTIL_GetCommandClient();
			trace_t tr;
			Vector forward;
			pPlayer->EyeVectors( &forward );
			UTIL_TraceLine(pPlayer->EyePosition(),
				pPlayer->EyePosition() + forward * MAX_TRACE_LENGTH,MASK_SOLID, 
				pPlayer, COLLISION_GROUP_NONE, &tr);
			if ( tr.fraction != 1.0 )
			{
				tr.endpos.z += 12;
				entity->Teleport( &tr.endpos, NULL, NULL );
				UTIL_DropToFloor( entity, MASK_SOLID );
			}
		}

	}
	CBaseEntity::SetAllowPrecache( allowPrecache );
}

// Console command functions
void CC_Create_PortalWeightedCube()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_STANDARD );
}

void CC_Create_PortalCompanionCube()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_COMPANION );
}

void CC_Create_PortalReflectorCube()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_REFLECTIVE );
}

void CC_Create_PortalWeightedSphere()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_SPHERE );
}

// Console commands for creating cubes
static ConCommand ent_create_portal_reflector_cube("ent_create_portal_reflector_cube", CC_Create_PortalReflectorCube, "Creates a laser reflector cube cube where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);
static ConCommand ent_create_portal_companion_cube("ent_create_portal_companion_cube", CC_Create_PortalCompanionCube, "Creates a companion cube where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);
static ConCommand ent_create_portal_weighted_cube("ent_create_portal_weighted_cube", CC_Create_PortalWeightedCube, "Creates a standard cube where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);
static ConCommand ent_create_portal_weighted_sphere("ent_create_portal_weighted_sphere", CC_Create_PortalWeightedSphere, "Creates a weighted sphere where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);