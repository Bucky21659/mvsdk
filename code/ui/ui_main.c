// Copyright (C) 1999-2000 Id Software, Inc.
//
/*
=======================================================================

USER INTERFACE MAIN

=======================================================================
*/

// use this to get a demo build without an explicit demo build, i.e. to get the demo ui files to build
//#define PRE_RELEASE_TADEMO

#include "ui_local.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/game_version.h"
#include "ui_force.h"

#include "mvsdk_setup.h"

/*
================
vmMain

This is the only way control passes into the module.
!!! This MUST BE THE VERY FIRST FUNCTION compiled into the .qvm file !!!
================
*/
vmCvar_t  ui_debug;
vmCvar_t  ui_initialized;
qboolean menuInJK2MV = qfalse;
qboolean isMainMenu = qfalse;
int mvapi = 0;
int Init_inGameLoad;

void _UI_Init( qboolean );
void _UI_Shutdown( void );
void _UI_KeyEvent( int key, qboolean down );
void _UI_MouseEvent( int dx, int dy );
void _UI_Refresh( int realtime );
qboolean _UI_IsFullscreen( void );
LIBEXPORT intptr_t vmMain( intptr_t command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10, intptr_t arg11  ) {
  int requestedMvApi = 0;
  if ( jk2version == VERSION_UNDEF && command != UI_GETAPIVERSION )
  { // Shouldn't happen under normal circumstances, but we had this case while debugging on old engine binaries...
	  Com_Printf("vmMain [UI]: first call to vmMain had a command != UI_GETAPIVERSION\n");
	  MV_UiDetectVersion(); // Try detecting the version now, otherwise we might be missing syscalls...
  }
  switch ( command ) {
	  case UI_GETAPIVERSION:
			if ( arg11 ) isMainMenu = qtrue;
			trap_Cvar_Set("ui_menulevel", "2");
			return /*UI_API_VERSION*/MV_UiDetectVersion();
	  case UI_INIT:
		  requestedMvApi = MVAPI_Init(arg11, arg0);
		  
		  if ( !requestedMvApi )
		  { // Only call _UI_Init if we haven't got access to the MVAPI. If we can use the MVAPI we delay the Init until the "MVAPI_AFTER_INIT" command is sent. That allows us use the MVAPI in the actual init.
			  _UI_Init(arg0);
		  }
		  else
		  { // Store the values that were meant for _UI_Init to use them later, when MVAPIR_AFTER_INIT is called.
			  Init_inGameLoad = arg0;
		  }
		  return requestedMvApi;

	  case MVAPI_AFTER_INIT:
		  MVAPI_AfterInit();
		  return 0;

	  case UI_SHUTDOWN:
		  _UI_Shutdown();
		  return 0;

	  case UI_KEY_EVENT:
		  _UI_KeyEvent( Key_GetProtocolKey15(jk2version, arg0), arg1 );
		  return 0;

	  case UI_MOUSE_EVENT:
		  _UI_MouseEvent( arg0, arg1 );
		  return 0;

	  case UI_REFRESH:
		  _UI_Refresh( arg0 );
		  return 0;

	  case UI_IS_FULLSCREEN:
		  return _UI_IsFullscreen();

	  case UI_SET_ACTIVE_MENU:
		  _UI_SetActiveMenu( arg0 );
		  return 0;

	  case UI_CONSOLE_COMMAND:
		  return UI_ConsoleCommand(arg0);

	  case UI_DRAW_CONNECT_SCREEN:
		  UI_DrawConnectScreen( arg0 );
		  return 0;
	  case UI_HASUNIQUECDKEY: // mod authors need to observe this
	    return qtrue; // bk010117 - change this to qfalse for mods!

	}

	return -1;
}

#define UI_MV_MIN_APILEVEL 1
#define UI_MV_MIN_VERSION "1.1"
int MVAPI_Init(int apilevel, int inGameLoad)
{
#ifdef JK2MV_MENU
	if (apilevel < MV_APILEVEL) {
		// using the mvmenu without jk2mv is useless
		trap_Error("This mvmenu version requires JK2MV " MV_MIN_VERSION);
	}

	menuInJK2MV = qtrue;
	mvapi = apilevel;

	// always using the newest api internally.
	return MV_APILEVEL;
#else
	char version[128];
	char jk2mv[64];

	trap_Cvar_VariableStringBuffer("version", version, sizeof(version));
	trap_Cvar_VariableStringBuffer("JK2MV", jk2mv, sizeof(jk2mv));

	if ( strstr(version, "JK2MV") || strlen(jk2mv) )
			menuInJK2MV = qtrue;

	if (!trap_Cvar_VariableValue("mv_apienabled"))
	{
		Com_Printf("UI: MVAPI is not supported at all or has been disabled.\n");
		Com_Printf("UI: You need at least JK2MV " UI_MV_MIN_VERSION ".\n");
		return 0;
	}

	if (apilevel < UI_MV_MIN_APILEVEL)
	{
		Com_Printf("UI: MVAPI level %i not supported.\n", UI_MV_MIN_APILEVEL);
		Com_Printf("UI: You need at least JK2MV " UI_MV_MIN_VERSION ".\n");
		return 0;
	}

	if (apilevel < MV_APILEVEL)
	{
		Com_Printf("UI: MVAPI level %i not supported (using level %i instead).\n", MV_APILEVEL, apilevel);
		Com_Printf("UI: You need at least JK2MV " MV_MIN_VERSION " to enable all API features.\n");
	}

	mvapi = apilevel;
	if ( mvapi > MV_APILEVEL ) mvapi = MV_APILEVEL;

	Com_Printf("UI: Using MVAPI level %i (%i supported).\n", mvapi, apilevel);
	return mvapi;
#endif
}

void MVAPI_AfterInit(void)
{
	if ( mvapi >= 3 )
	{ // If the apilevel supports it tell the engine that we're using 1.04 structs etc. internally
		// Get the inital version
		jk2startversion = trap_MVAPI_GetVersion();
		// Set the version to 1.04
		trap_MVAPI_SetVersion( VERSION_1_04 );
		// Get the current version (should always be 1.04)
		jk2version = trap_MVAPI_GetVersion();

		// Set gameplay and version
		MV_SetGameVersion( jk2version, qfalse );
		MV_SetGamePlay( jk2startversion );
	}

	// Call _UI_Init now, because we delayed it earilier
	_UI_Init( Init_inGameLoad );
}

int MV_UiDetectVersion( void )
{
#ifdef JK2MV_MENU
	jk2startversion = jk2version = VERSION_1_04;
	MV_SetGameVersion(jk2version, qtrue); // Set the GameVersion...
	return UI_API_VERSION;
#else
	char buffer[32];
	// MVSDK: Let's detect which version of the engine we are running in...
	jk2version = VERSION_UNDEF;

	trap_Cvar_VariableStringBuffer( "mv_apienabled", buffer, sizeof(buffer) );
	if ( strlen(buffer) && atoi(buffer) > 0 )
	{ // JK2MV >= 1.1
		switch ( trap_MVAPI_GetVersion() )
		{
			case VERSION_1_02:
				jk2version = VERSION_1_02;
				break;
			case VERSION_1_03:
				jk2version = VERSION_1_03;
				break;
			case VERSION_1_04:
				jk2version = VERSION_1_04;
				break;
			default:
				jk2version = VERSION_UNDEF;
		}
	}

	if ( jk2version == VERSION_UNDEF )
	{
		char version[128];

		trap_Cvar_VariableStringBuffer("version", version, sizeof(version));
		
		if ( strstr(version, "JK2MP") )
		{ // JK2MP
			     if ( strstr(version, "1.02") ) jk2version = VERSION_1_02;
			else if ( strstr(version, "1.03") ) jk2version = VERSION_1_03;
			else if ( strstr(version, "1.04") ) jk2version = VERSION_1_04;
		}
	}
	
	if ( jk2version == VERSION_UNDEF )
	{
		Com_Printf("MVSDK: Unable to detect jk2version [UI]; fallback to 1.04;");
		jk2version = VERSION_1_04;
	}
	Com_Printf("jk2version [UI]: 1.0%i\n", jk2version);
	jk2startversion = jk2version;
	MV_SetGameVersion(jk2version, qtrue); // Set the GameVersion...

	switch( jk2version )
	{
		case VERSION_1_02:
			return UI_API_VERSION_1_02;
		case VERSION_1_03:
		case VERSION_1_04:
		default:
			return UI_API_VERSION;
	}
#endif
}

/*
===================
UI_WideScreenMode
Make 2D drawing functions use widescreen or 640x480 coordinates
===================
*/
void UI_WideScreenMode(qboolean on) {
	if (mvapi >= 3) {
		if (on) {
			trap_MVAPI_SetVirtualScreen(uiInfo.screenWidth, uiInfo.screenHeight);
		}
		else {
			trap_MVAPI_SetVirtualScreen((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT);
		}
	}
}

/*
=================
UI_UpdateWidescreen
=================
*/
extern vmCvar_t ui_widescreen;
static void UI_UpdateWidescreen(void) {
	if (ui_widescreen.integer && mvapi >= 3) {
		if ( uiInfo.uiDC.glconfig.vidWidth >= uiInfo.uiDC.glconfig.vidHeight ) {
			uiInfo.screenWidth = (float)SCREEN_HEIGHT * uiInfo.uiDC.glconfig.vidWidth / uiInfo.uiDC.glconfig.vidHeight;
			uiInfo.screenHeight = (float)SCREEN_HEIGHT;
			uiInfo.portraitMode = qfalse;
		} else {
			uiInfo.screenWidth = (float)SCREEN_WIDTH;
			uiInfo.screenHeight = (float)SCREEN_WIDTH * uiInfo.uiDC.glconfig.vidHeight / uiInfo.uiDC.glconfig.vidWidth;
			uiInfo.portraitMode = qtrue;
		}
	} else {
		uiInfo.screenWidth = (float)SCREEN_WIDTH;
		uiInfo.screenHeight = (float)SCREEN_HEIGHT;
		uiInfo.portraitMode = qfalse;
	}

	uiInfo.screenXFactor = (float)SCREEN_WIDTH / uiInfo.screenWidth;
	uiInfo.screenXFactorInv = uiInfo.screenWidth / (float)SCREEN_WIDTH;

	uiInfo.screenYFactor = (float)SCREEN_HEIGHT / uiInfo.screenHeight;
	uiInfo.screenYFactorInv = uiInfo.screenHeight / (float)SCREEN_HEIGHT;

	if (uiInfo.portraitMode && isMainMenu) {
		uiInfo.screenWidth = (float)SCREEN_WIDTH;
		uiInfo.screenHeight = (float)SCREEN_HEIGHT;
	}

	if (mvapi >= 3 && ui_widescreen.integer != 2)
		trap_MVAPI_SetVirtualScreen(uiInfo.screenWidth, uiInfo.screenHeight);
}

menuDef_t *Menus_FindByName(const char *p);
void Menu_ShowItemByName(menuDef_t *menu, const char *p, qboolean bShow);
void UpdateForceUsed();

static char holdSPString[1024]={0};

uiInfo_t uiInfo;

static void UI_StartServerRefresh(qboolean full);
static void UI_StopServerRefresh( void );
static void UI_DoServerRefresh( void );
static void UI_BuildServerDisplayList(int force);
static void UI_BuildServerStatus(qboolean force);
static void UI_BuildFindPlayerList(qboolean force);
static int QDECL UI_ServersQsortCompare( const void *arg1, const void *arg2 );
static int UI_MapCountByGameType(qboolean singlePlayer);
static int UI_HeadCountByTeam( void );
static int UI_HeadCountByColor( void );
static void UI_ParseGameInfo(const char *teamFile);
static const char *UI_SelectedMap(int index, int *actual);
static const char *UI_SelectedHead(int index, int *actual);
static int UI_GetIndexFromSelection(int actual);

int ProcessNewUI( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6 );
int	uiSkinColor = SKINCOLOR_DEFAULT;

static serverFilter_t serverGameFilters[] = {
	{"All", "" },
	{"Jedi Knight 2", "" },
};
static const int numServerGameFilters = sizeof(serverGameFilters) / sizeof(serverFilter_t);

static serverFilter_t serverVersionFilters[] = {
	{"All", "All" },
	{"1.02", "1.02" },
	{"1.03", "1.03" },
	{"1.04", "1.04" },
};
static const int numServerVersionFilters = sizeof(serverVersionFilters) / sizeof(serverFilter_t);

static serverFilter_t *serverFilters;
static int numServerFilters;

static void UI_SetServerFilter( void )
{
	if (!uiInfo.inGameLoad)
	{
		serverFilters = serverVersionFilters;
		numServerFilters = numServerVersionFilters;
	}
	else
	{
		serverFilters = serverGameFilters;
		numServerFilters = numServerGameFilters;
	}
}




static const char *skillLevels[] = {
  "SKILL1",//"I Can Win",
  "SKILL2",//"Bring It On",
  "SKILL3",//"Hurt Me Plenty",
  "SKILL4",//"Hardcore",
  "SKILL5"//"Nightmare"
};
static const int numSkillLevels = sizeof(skillLevels) / sizeof(const char*);



static const char *teamArenaGameTypes[] = {
	"FFA",
	"HOLOCRON",
	"JEDIMASTER",
	"DUEL",
	"SP",
	"TEAM FFA",
	"N/A",
	"CTF",
	"CTY",
	"TEAMTOURNAMENT"
};
static int const numTeamArenaGameTypes = sizeof(teamArenaGameTypes) / sizeof(const char*);



static char* netnames[] = {
	"???",
	"UDP",
	"IPX",
	NULL
};

// static int gamecodetoui[] = {4,2,3,0,5,1,6};
// static int uitogamecode[] = {4,6,2,3,1,5,7};

const char *UI_GetStripEdString(const char *refSection, const char *refName);

const char *UI_TeamName(int team)  {
	if (team==TEAM_RED)
		return "RED";
	else if (team==TEAM_BLUE)
		return "BLUE";
	else if (team==TEAM_SPECTATOR)
		return "SPECTATOR";
	return "FREE";
}

// returns either string or NULL for OOR...
//
static const char *GetCRDelineatedString( const char *psStripFileRef, const char *psStripStringRef, int iIndex)
{
	static char sTemp[256];
	const char *psList = UI_GetStripEdString(psStripFileRef, psStripStringRef);
	char *p;

	while (iIndex--)
	{
		psList = strchr(psList,'\n');
		if (!psList){
			return NULL;	// OOR
		}
		psList++;
	}

	strcpy(sTemp,psList);
	p = strchr(sTemp,'\n');
	if (p) {
		*p = '\0';
	}

	return sTemp;
}


static const char *GetMonthAbbrevString( int iMonth )
{
	const char *p = GetCRDelineatedString("INGAMETEXT","MONTHS", iMonth);
	
	return p ? p : "Jan";	// sanity
}




/*
static const char *netSources[] = {
	"Local",
	"Internet",
	"Favorites"
//	"Mplayer"
};
static const int numNetSources = sizeof(netSources) / sizeof(const char*);
*/
static const int numNetSources = 3;	// now hard-entered in StripEd file
static const char *GetNetSourceString(int iSource)
{
	const char *p = GetCRDelineatedString("INGAMETEXT","NET_SOURCES", iSource);

	return p ? p : "??";
}




void AssetCache() {
	int n;
	//if (Assets.textFont == NULL) {
	//}
	//Assets.background = trap_R_RegisterShaderNoMip( ASSET_BACKGROUND );
	//Com_Printf("Menu Size: %i bytes\n", sizeof(Menus));
	uiInfo.uiDC.Assets.gradientBar = trap_R_RegisterShaderNoMip( ASSET_GRADIENTBAR );
	uiInfo.uiDC.Assets.fxBasePic = trap_R_RegisterShaderNoMip( ART_FX_BASE );
	uiInfo.uiDC.Assets.fxPic[0] = trap_R_RegisterShaderNoMip( ART_FX_RED );
	uiInfo.uiDC.Assets.fxPic[1] = trap_R_RegisterShaderNoMip( ART_FX_ORANGE );//trap_R_RegisterShaderNoMip( ART_FX_YELLOW );
	uiInfo.uiDC.Assets.fxPic[2] = trap_R_RegisterShaderNoMip( ART_FX_YELLOW );//trap_R_RegisterShaderNoMip( ART_FX_GREEN );
	uiInfo.uiDC.Assets.fxPic[3] = trap_R_RegisterShaderNoMip( ART_FX_GREEN );//trap_R_RegisterShaderNoMip( ART_FX_TEAL );
	uiInfo.uiDC.Assets.fxPic[4] = trap_R_RegisterShaderNoMip( ART_FX_BLUE );
	uiInfo.uiDC.Assets.fxPic[5] = trap_R_RegisterShaderNoMip( ART_FX_PURPLE );//trap_R_RegisterShaderNoMip( ART_FX_CYAN );
	uiInfo.uiDC.Assets.fxPic[6] = trap_R_RegisterShaderNoMip( ART_FX_WHITE );
	uiInfo.uiDC.Assets.scrollBar = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR );
	uiInfo.uiDC.Assets.scrollBarArrowDown = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWDOWN );
	uiInfo.uiDC.Assets.scrollBarArrowUp = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWUP );
	uiInfo.uiDC.Assets.scrollBarArrowLeft = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWLEFT );
	uiInfo.uiDC.Assets.scrollBarArrowRight = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWRIGHT );
	uiInfo.uiDC.Assets.scrollBarThumb = trap_R_RegisterShaderNoMip( ASSET_SCROLL_THUMB );
	uiInfo.uiDC.Assets.sliderBar = trap_R_RegisterShaderNoMip( ASSET_SLIDER_BAR );
	uiInfo.uiDC.Assets.sliderThumb = trap_R_RegisterShaderNoMip( ASSET_SLIDER_THUMB );

	for( n = 0; n < NUM_CROSSHAIRS; n++ ) {
		uiInfo.uiDC.Assets.crosshairShader[n] = trap_R_RegisterShaderNoMip( va("gfx/2d/crosshair%c", 'a' + n ) );
	}

	uiInfo.newHighScoreSound = 0;//trap_S_RegisterSound("sound/feedback/voc_newhighscore.wav");
}

void _UI_DrawSides(float x, float y, float w, float h, float size) {
	trap_R_DrawStretchPic( x, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x + w - size, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}

void _UI_DrawTopBottom(float x, float y, float w, float h, float size) {
	trap_R_DrawStretchPic( x, y, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x, y + h - size, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}
/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void _UI_DrawRect( float x, float y, float width, float height, float size, const float *color ) {
	trap_R_SetColor( color );

  _UI_DrawTopBottom(x, y, width, height, size);
  _UI_DrawSides(x, y, width, height, size);

	trap_R_SetColor( NULL );
}

int MenuFontToHandle(int iMenuFont)
{
	switch (iMenuFont)
	{
		case 1: return uiInfo.uiDC.Assets.qhSmallFont;
		case 2: return uiInfo.uiDC.Assets.qhMediumFont;
		case 3: return uiInfo.uiDC.Assets.qhBigFont;
	}

	return uiInfo.uiDC.Assets.qhMediumFont;	// 0;
}

int Text_Width(const char *text, float scale, int iMenuFont) 
{	
	int iFontIndex = MenuFontToHandle(iMenuFont);
	float w;

	UI_WideScreenMode(qtrue);
	w = trap_R_Font_StrLenPixels(text, iFontIndex, scale) * uiInfo.screenXFactor;
	UI_WideScreenMode(uiInfo.portraitMode);
	return w;
}

int Text_Height(const char *text, float scale, int iMenuFont) 
{
	int iFontIndex = MenuFontToHandle(iMenuFont);
	float h;
	UI_WideScreenMode(qtrue);
	h = trap_R_Font_HeightPixels(iFontIndex, scale);
	UI_WideScreenMode(uiInfo.portraitMode);
	return h;
}

static void Text_Paint(float x, float y, float scale, const vec4_t color, const char *text, float adjust, int limit, int style, int iMenuFont)
{
	int iStyleOR = 0;

	int iFontIndex = MenuFontToHandle(iMenuFont);
	//
	// kludge.. convert JK2 menu styles to SOF2 printstring ctrl codes...
	//	
	switch (style)
	{
		case  ITEM_TEXTSTYLE_NORMAL:			iStyleOR = 0;break;						// JK2 normal text
		case  ITEM_TEXTSTYLE_BLINK:				iStyleOR = (int)STYLE_BLINK;break;		// JK2 fast blinking
		case  ITEM_TEXTSTYLE_PULSE:				iStyleOR = (int)STYLE_BLINK;break;		// JK2 slow pulsing
		case  ITEM_TEXTSTYLE_SHADOWED:			iStyleOR = (int)STYLE_DROPSHADOW;break;	// JK2 drop shadow
	case  ITEM_TEXTSTYLE_OUTLINED:			iStyleOR = (int)STYLE_DROPSHADOW;break;	// JK2 drop shadow
	case  ITEM_TEXTSTYLE_OUTLINESHADOWED:	iStyleOR = (int)STYLE_DROPSHADOW;break;	// JK2 drop shadow
		case  ITEM_TEXTSTYLE_SHADOWEDMORE:		iStyleOR = (int)STYLE_DROPSHADOW;break;	// JK2 drop shadow
	}

	UI_WideScreenMode(qtrue);
	x *= uiInfo.screenXFactorInv;
	trap_R_Font_DrawString(	x,						// int ox
							y,						// int oy
							text,					// const char *text
							color,					// paletteRGBA_c c
							iStyleOR | iFontIndex,	// const int iFontHandle
							!limit?-1:limit,		// iCharLimit (-1 = none)
							scale );				// const float scale = 1.0f
							
	UI_WideScreenMode(uiInfo.portraitMode);
}

void Text_PaintWithCursor(float x, float y, float scale, const vec4_t color, const char *text, unsigned cursorPos, char cursor, unsigned limit, int style, int iMenuFont)
{
	Text_Paint(x, y, scale, color, text, 0, limit, style, iMenuFont);

	// now print the cursor as well...  (excuse the braces, it's for porting C++ to C)
	//
	{
		char sTemp[1024];
		unsigned iCopyCount = limit ? MIN((unsigned)strlen(text), (unsigned)limit) : (unsigned)strlen(text);
			iCopyCount = MIN(iCopyCount,cursorPos);
			iCopyCount = MIN(iCopyCount,(int)sizeof(sTemp)-1);

			// copy text into temp buffer for pixel measure...
			//
			Q_strncpyz(sTemp,text,iCopyCount + 1);

			{
				int iNextXpos = Text_Width(sTemp, scale, iMenuFont);

				Text_Paint(x+iNextXpos, y, scale, color, va("%c",cursor), 0, limit, style|ITEM_TEXTSTYLE_BLINK, iMenuFont);
			}
	}
}


// maxX param is initially an X limit, but is also used as feedback. 0 = text was clipped to fit within, else maxX = next pos
//
static void Text_Paint_Limit(float *maxX, float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int iMenuFont) 
{
	//float fMax = *maxX;
	int iPixelLen = Text_Width(text, scale, iMenuFont);
	if (x + iPixelLen > *maxX)
	{
		// whole text won't fit, so we need to print just the amount that does...
		//  Ok, this is slow and tacky, but only called occasionally, and it works...
		//
		char sTemp[4096]={0};	// lazy assumption
		const char *psText = text;
		char *psOut = &sTemp[0];
		char *psOutLastGood = psOut;
		unsigned int uiLetter;

		while (*psText && (x + Text_Width(sTemp, scale, iMenuFont) <= *maxX)
			&& psOut < &sTemp[sizeof(sTemp) - 1]	// sanity
			)
		{
			int iAdvanceCount;
			psOutLastGood = psOut;
			
			if ( jk2version == VERSION_1_02 )
			{
				uiLetter = trap_AnyLanguage_ReadCharFromString_1_02(&psText);
			}
			else
			{
				uiLetter = trap_AnyLanguage_ReadCharFromString_1_04(psText, &iAdvanceCount, NULL);
				psText += iAdvanceCount;
			}

			if (uiLetter > 255)
			{
				*psOut++ = uiLetter>>8;
				*psOut++ = uiLetter&0xFF;
			}
			else
			{
				*psOut++ = uiLetter&0xFF;
			}
		}
		*psOutLastGood = '\0';

		*maxX = 0;	// feedback
		Text_Paint(x, y, scale, color, sTemp, adjust, limit, ITEM_TEXTSTYLE_NORMAL, iMenuFont);
	}
	else
	{
		// whole text fits fine, so print it all...
		//
		*maxX = x + iPixelLen;	// feedback the next position, as the caller expects		
		Text_Paint(x, y, scale, color, text, adjust, limit, ITEM_TEXTSTYLE_NORMAL, iMenuFont);
	}
}


void UI_ShowPostGame(qboolean newHigh) {
	trap_Cvar_Set ("cg_cameraOrbit", "0");
	trap_Cvar_Set("cg_thirdPerson", "0");
	trap_Cvar_Set( "sv_killserver", "1" );
	uiInfo.soundHighScore = newHigh;
  _UI_SetActiveMenu(UIMENU_POSTGAME);
}
/*
=================
_UI_Refresh
=================
*/

void UI_DrawCenteredPic(qhandle_t image, int w, int h) {
  int x, y;
  x = (uiInfo.screenWidth - w) / 2;
  y = (uiInfo.screenHeight - h) / 2;
  UI_DrawHandlePic(x, y, w, h, image);
}

int frameCount = 0;
int startTime;

vmCvar_t	ui_rankChange;
static void UI_BuildPlayerList();
char parsedFPMessage[1024];
extern int FPMessageTime;
static void Text_PaintCenter(float x, float y, float scale, const vec4_t color, const char *text, float adjust, int iMenuFont);

const char *UI_GetStripEdString(const char *refSection, const char *refName)
{
	static char text[1024]={0};

	trap_SP_GetStringTextString(va("%s_%s", refSection, refName), text, sizeof(text));
	return text;
}

#define	UI_FPS_FRAMES	4
static char serverInfo[MAX_INFO_STRING];
static int serverGameType;
void _UI_Refresh( int realtime )
{
	static int index;
	static int	previousTimes[UI_FPS_FRAMES];
	static int nextRefresh;

	//if ( !( trap_Key_GetCatcher() & KEYCATCH_UI ) ) {
	//	return;
	//}

	uiInfo.uiDC.frameTime = realtime - uiInfo.uiDC.realTime;
	uiInfo.uiDC.realTime = realtime;

	previousTimes[index % UI_FPS_FRAMES] = uiInfo.uiDC.frameTime;
	index++;
	if ( index > UI_FPS_FRAMES ) {
		int i, total;
		// average multiple frames together to smooth changes out a bit
		total = 0;
		for ( i = 0 ; i < UI_FPS_FRAMES ; i++ ) {
			total += previousTimes[i];
		}
		if ( !total ) {
			total = 1;
		}
		uiInfo.uiDC.FPS = 1000 * UI_FPS_FRAMES / total;
	}

	if ( nextRefresh < realtime ) {
		nextRefresh = realtime + 1000;

		// Update the g_gametype once per second, it's unusual for servers to switch them at runtime anyway
		trap_GetConfigString( CS_SERVERINFO, serverInfo, sizeof(serverInfo) );
		serverGameType = atoi(Info_ValueForKey(serverInfo, "g_gametype"));
	}


	UI_UpdateCvars();

	if (Menu_Count() > 0) {
		// paint all the menus
		Menu_PaintAll();
		// refresh server browser list
		UI_DoServerRefresh();
		// refresh server status
		UI_BuildServerStatus(qfalse);
		// refresh find player list
		UI_BuildFindPlayerList(qfalse);

		// draw cursor
		if (ui_widescreen.integer && mvapi >= 3 && uiInfo.portraitMode && isMainMenu) { //Scale height on the main menu in portrait mode.
			UI_SetColor(NULL);
			trap_MVAPI_SetVirtualScreen((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT * uiInfo.screenYFactorInv);
			UI_DrawHandlePic(uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory * uiInfo.screenYFactorInv, 48, 48, uiInfo.uiDC.Assets.cursor);
			trap_MVAPI_SetVirtualScreen((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT);
		} else {
			UI_SetColor(NULL);
			UI_WideScreenMode(qtrue);
			UI_DrawHandlePic(uiInfo.uiDC.cursorx * uiInfo.screenXFactorInv, uiInfo.uiDC.cursory, 48, 48, uiInfo.uiDC.Assets.cursor);
			UI_WideScreenMode(uiInfo.portraitMode);
		}
	}

#ifndef NDEBUG
	if (uiInfo.uiDC.debug)
	{
		// cursor coordinates
		//FIXME
		//UI_DrawString( 0, 0, va("(%d,%d)",uis.cursorx,uis.cursory), UI_LEFT|UI_SMALLFONT, colorRed );
	}
#endif

	if (ui_rankChange.integer)
	{
		FPMessageTime = realtime + 3000;

		if (!parsedFPMessage[0] /*&& uiMaxRank > ui_rankChange.integer*/)
		{
			const char *printMessage = UI_GetStripEdString("INGAMETEXT", "SET_NEW_RANK");

			int i = 0;
			int p = 0;
			int linecount = 0;

			while (printMessage[i] && p < 1024)
			{
				parsedFPMessage[p] = printMessage[i];
				p++;
				i++;
				linecount++;

				if (linecount > 64 && printMessage[i] == ' ')
				{
					parsedFPMessage[p] = '\n';
					p++;
					linecount = 0;
				}
			}
			parsedFPMessage[p] = '\0';
		}

		//if (uiMaxRank > ui_rankChange.integer)
		{
			uiServerForceRank = ui_rankChange.integer;
			uiMaxRank = Com_Clampi(1, MAX_FORCE_RANK, ui_rankChange.integer);
			uiForceRank = uiMaxRank;

			/*
			while (x < NUM_FORCE_POWERS)
			{
				//For now just go ahead and clear force powers upon rank change
				uiForcePowersRank[x] = 0;
				x++;
			}
			uiForcePowersRank[FP_LEVITATION] = 1;
			uiForceUsed = 0;
			*/

			//Use BG_LegalizedForcePowers and transfer the result into the UI force settings
			UI_ReadLegalForce();
		}

		if (ui_freeSaber.integer && uiForcePowersRank[FP_SABERATTACK] < 1)
		{
			uiForcePowersRank[FP_SABERATTACK] = 1;
		}
		if (ui_freeSaber.integer && uiForcePowersRank[FP_SABERDEFEND] < 1)
		{
			uiForcePowersRank[FP_SABERDEFEND] = 1;
		}
		trap_Cvar_Set("ui_rankChange", "0");

		//remember to update the force power count after changing the max rank
		UpdateForceUsed();
	}

	if (ui_freeSaber.integer)
	{
		bgForcePowerCost[FP_SABERATTACK][FORCE_LEVEL_1] = 0;
		bgForcePowerCost[FP_SABERDEFEND][FORCE_LEVEL_1] = 0;
	}
	else
	{
		bgForcePowerCost[FP_SABERATTACK][FORCE_LEVEL_1] = 1;
		bgForcePowerCost[FP_SABERDEFEND][FORCE_LEVEL_1] = 1;
	}

	/*
	if (parsedFPMessage[0] && FPMessageTime > realtime)
	{
		vec4_t txtCol;
		int txtStyle = ITEM_TEXTSTYLE_SHADOWED;

		if ((FPMessageTime - realtime) < 2000)
		{
			txtCol[0] = colorWhite[0];
			txtCol[1] = colorWhite[1];
			txtCol[2] = colorWhite[2];
			txtCol[3] = (((float)FPMessageTime - (float)realtime)/2000);

			txtStyle = 0;
		}
		else
		{
			txtCol[0] = colorWhite[0];
			txtCol[1] = colorWhite[1];
			txtCol[2] = colorWhite[2];
			txtCol[3] = colorWhite[3];
		}

		Text_Paint(10, 0, 1, txtCol, parsedFPMessage, 0, 1024, txtStyle, FONT_MEDIUM);
	}
	*/
	//For now, don't bother.
}

/*
=================
_UI_Shutdown
=================
*/
void _UI_Shutdown( void ) {
	trap_LAN_SaveCachedServers();

	// We don't get a new force rank from the server during vid_restart (the server sends "nfr" on force init, the cgame
	// receives it and tells the engine to change the "ui_rankChange" cvar and ui just checks the value of that cvar each
	// refresh). So when shutting down the UI module we have to store the old ui_rankChange to restore it afterwards.
	// We only want to do this during a vid_restart, cause otherwise we would carry over the force rank from our previous
	// server to the next one. So we store the rank in a temporary cvar and let the UI_Init method decide whether it's
	// relevant or not.
	trap_Cvar_Set("_ui_serverForceRank", va("%i", uiServerForceRank));
}

char *defaultMenu = NULL;

const char *GetMenuBuffer(const char *filename) {
	int	len;
	fileHandle_t	f;
	static char buf[MAX_MENUFILE];

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( !f ) {
		trap_Print( va( S_COLOR_RED "menu file not found: %s, using default\n", filename ) );
		return defaultMenu;
	}
	if ( len >= MAX_MENUFILE ) {
		trap_Print( va( S_COLOR_RED "menu file too large: %s is %i, max allowed is %i", filename, len, MAX_MENUFILE ) );
		trap_FS_FCloseFile( f );
		return defaultMenu;
	}

	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );
	//COM_Compress(buf);
  return buf;

}

qboolean Asset_Parse(int handle) {
	char	stripedFile[MAX_STRING_CHARS];
	pc_token_t token;
	const char *tempStr;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (Q_stricmp(token.string, "{") != 0) {
		return qfalse;
	}
    
	while ( 1 ) {

		memset(&token, 0, sizeof(pc_token_t));

		if (!trap_PC_ReadToken(handle, &token))
			return qfalse;

		if (Q_stricmp(token.string, "}") == 0) {
			return qtrue;
		}

		// font
		if (Q_stricmp(token.string, "font") == 0) {
			int pointSize;
			if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle,&pointSize)) {
				return qfalse;
			}			
			//trap_R_RegisterFont(tempStr, pointSize, &uiInfo.uiDC.Assets.textFont);
			uiInfo.uiDC.Assets.qhMediumFont = trap_R_RegisterFont(tempStr);
			uiInfo.uiDC.Assets.fontRegistered = qtrue;
			continue;
		}

		if (Q_stricmp(token.string, "smallFont") == 0) {
			int pointSize;
			if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle,&pointSize)) {
				return qfalse;
			}
			//trap_R_RegisterFont(tempStr, pointSize, &uiInfo.uiDC.Assets.smallFont);
			uiInfo.uiDC.Assets.qhSmallFont = trap_R_RegisterFont(tempStr);
			continue;
		}

		if (Q_stricmp(token.string, "bigFont") == 0) {
			int pointSize;
			if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle,&pointSize)) {
				return qfalse;
			}
			//trap_R_RegisterFont(tempStr, pointSize, &uiInfo.uiDC.Assets.bigFont);
			uiInfo.uiDC.Assets.qhBigFont = trap_R_RegisterFont(tempStr);
			continue;
		}

		if (Q_stricmp(token.string, "stripedFile") == 0) 
		{
			if (!PC_String_Parse(handle, &tempStr))
			{
				Com_Printf(S_COLOR_YELLOW "Bad 1st parameter for keyword 'stripedFile'\n");
				return qfalse;
			}
			Q_strncpyz( stripedFile, tempStr,  sizeof(stripedFile) );
			trap_SP_Register(stripedFile);
			continue;
		}

		if (Q_stricmp(token.string, "cursor") == 0) 
		{
			if (!PC_String_Parse(handle, &uiInfo.uiDC.Assets.cursorStr))
			{
				Com_Printf(S_COLOR_YELLOW "Bad 1st parameter for keyword 'cursor'\n");
				return qfalse;
			}
			uiInfo.uiDC.Assets.cursor = trap_R_RegisterShaderNoMip( uiInfo.uiDC.Assets.cursorStr);
			continue;
		}

		// gradientbar
		if (Q_stricmp(token.string, "gradientbar") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			uiInfo.uiDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(tempStr);
			continue;
		}

		// enterMenuSound
		if (Q_stricmp(token.string, "menuEnterSound") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuEnterSound = trap_S_RegisterSound( tempStr );
			continue;
		}

		// exitMenuSound
		if (Q_stricmp(token.string, "menuExitSound") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuExitSound = trap_S_RegisterSound( tempStr );
			continue;
		}

		// itemFocusSound
		if (Q_stricmp(token.string, "itemFocusSound") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			uiInfo.uiDC.Assets.itemFocusSound = trap_S_RegisterSound( tempStr );
			continue;
		}

		// menuBuzzSound
		if (Q_stricmp(token.string, "menuBuzzSound") == 0) {
			if (!PC_String_Parse(handle, &tempStr)) {
				return qfalse;
			}
			uiInfo.uiDC.Assets.menuBuzzSound = trap_S_RegisterSound( tempStr );
			continue;
		}

		if (Q_stricmp(token.string, "fadeClamp") == 0) {
			if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.fadeClamp)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "fadeCycle") == 0) {
			if (!PC_Int_Parse(handle, &uiInfo.uiDC.Assets.fadeCycle)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "fadeAmount") == 0) {
			if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.fadeAmount)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "shadowX") == 0) {
			if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.shadowX)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "shadowY") == 0) {
			if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.shadowY)) {
				return qfalse;
			}
			continue;
		}

		if (Q_stricmp(token.string, "shadowColor") == 0) {
			if (!PC_Color_Parse(handle, &uiInfo.uiDC.Assets.shadowColor)) {
				return qfalse;
			}
			uiInfo.uiDC.Assets.shadowFadeClamp = uiInfo.uiDC.Assets.shadowColor[3];
			continue;
		}

	}
	return qfalse;
}


void UI_Report() {
  String_Report();
  //Font_Report();

}

void UI_ParseMenu(const char *menuFile) {
	int handle;
	pc_token_t token;

	Com_Printf("Parsing menu file:%s\n", menuFile);

	handle = trap_PC_LoadSource(menuFile);
	if (!handle) {
		return;
	}

	while ( 1 ) {
		memset(&token, 0, sizeof(pc_token_t));
		if (!trap_PC_ReadToken( handle, &token )) {
			break;
		}

		//if ( Q_stricmp( token, "{" ) ) {
		//	Com_Printf( "Missing { in menu file\n" );
		//	break;
		//}

		//if ( menuCount == MAX_MENUS ) {
		//	Com_Printf( "Too many menus!\n" );
		//	break;
		//}

		if ( token.string[0] == '}' ) {
			break;
		}

		if (Q_stricmp(token.string, "assetGlobalDef") == 0) {
			if (Asset_Parse(handle)) {
				continue;
			} else {
				break;
			}
		}

		if (Q_stricmp(token.string, "menudef") == 0) {
			// start a new menu
			Menu_New(handle);
		}
	}
	trap_PC_FreeSource(handle);
}

qboolean Load_Menu(int handle) {
	pc_token_t token;

	if (!trap_PC_ReadToken(handle, &token))
		return qfalse;
	if (token.string[0] != '{') {
		return qfalse;
	}

	while ( 1 ) {

		if (!trap_PC_ReadToken(handle, &token))
			return qfalse;
    
		if ( token.string[0] == 0 ) {
			return qfalse;
		}

		if ( token.string[0] == '}' ) {
			return qtrue;
		}

		UI_ParseMenu(token.string); 
	}
	return qfalse;
}

void UI_LoadMenus(const char *menuFile, qboolean reset) {
	pc_token_t token;
	int handle;
	int start;

	start = trap_Milliseconds();

	trap_PC_LoadGlobalDefines ( "ui/jk2mp/menudef.h" );

	handle = trap_PC_LoadSource( menuFile );
	if (!handle) {
		Com_Printf( S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile );
		handle = trap_PC_LoadSource( "ui/jk2mpmenus.txt" );
		if (!handle) {
			Com_Error( ERR_DROP, "default menu file not found: ui/menus.txt, unable to continue!" );
		}
	}

	if (reset) {
		Menu_Reset();
	}

	while ( 1 ) {
		if (!trap_PC_ReadToken(handle, &token))
			break;
		if( token.string[0] == 0 || token.string[0] == '}') {
			break;
		}

		if ( token.string[0] == '}' ) {
			break;
		}

		if (Q_stricmp(token.string, "loadmenu") == 0) {
			if (Load_Menu(handle)) {
				continue;
			} else {
				break;
			}
		}
	}

	if ( !uiInfo.inGameLoad )
	{
		UI_ParseMenu("ui/jk2mv/download_popup.menu");
		UI_ParseMenu("ui/jk2mv/download_info.menu");
	}

	Com_Printf("UI menu load time = %d milli seconds\n", trap_Milliseconds() - start);

	trap_PC_FreeSource( handle );

	trap_PC_RemoveAllGlobalDefines ( );
}

void UI_Load() {
	char *menuSet;
	char lastName[1024];
	menuDef_t *menu = Menu_GetFocused();

	if (menu && menu->window.name) {
		strcpy(lastName, menu->window.name);
	}
	else
	{
		lastName[0] = 0;
	}

	if (uiInfo.inGameLoad)
	{
		menuSet= "ui/jk2mpingame.txt";
	}
	else
	{
		menuSet= UI_Cvar_VariableString("ui_menuFilesMP");
	}
	if (menuSet == NULL || menuSet[0] == '\0') {
		menuSet = "ui/jk2mpmenus.txt";
	}

	String_Init();

#ifdef PRE_RELEASE_TADEMO
	UI_ParseGameInfo("demogameinfo.txt");
#else
	UI_ParseGameInfo("ui/jk2mp/gameinfo.txt");
#endif
	UI_LoadArenas();
	UI_LoadBots();

	UI_LoadMenus(menuSet, qtrue);
	Menus_CloseAll();
	Menus_ActivateByName(lastName);

}

static const char *handicapValues[] = {"None","95","90","85","80","75","70","65","60","55","50","45","40","35","30","25","20","15","10","5",NULL};

static void UI_DrawHandicap(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
  int i, h;

  h = Com_Clamp( 5, 100, trap_Cvar_VariableValue("handicap") );
  i = 20 - h / 5;

  Text_Paint(rect->x, rect->y, scale, color, handicapValues[i], 0, 0, textStyle, iMenuFont);
}

static void UI_DrawClanName(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
  Text_Paint(rect->x, rect->y, scale, color, UI_Cvar_VariableString("ui_teamName"), 0, 0, textStyle, iMenuFont);
}


static void UI_SetCapFragLimits(qboolean uiVars) {
	int cap = 5;
	int frag = 10;

	if (uiVars) {
		trap_Cvar_Set("ui_captureLimit", va("%d", cap));
		trap_Cvar_Set("ui_fragLimit", va("%d", frag));
	} else {
		trap_Cvar_Set("capturelimit", va("%d", cap));
		trap_Cvar_Set("fraglimit", va("%d", frag));
	}
}
// ui_gameType assumes gametype 0 is -1 ALL and will not show
static void UI_DrawGameType(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
  Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[ui_gameType.integer].gameType, 0, 0, textStyle, iMenuFont);
}

static void UI_DrawNetGameType(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
	if (ui_netGameType.integer < 0 || ui_netGameType.integer >= uiInfo.numGameTypes) {
		trap_Cvar_Set("ui_netGameType", "0");
		trap_Cvar_Set("ui_actualNetGameType", "0");
	}
  Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[ui_netGameType.integer].gameType , 0, 0, textStyle, iMenuFont);
}

static void UI_DrawAutoSwitch(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
	int switchVal = trap_Cvar_VariableValue("cg_autoswitch");
	const char *switchString = "AUTOSWITCH1";
	const char *stripString = NULL;

	switch(switchVal)
	{
	case 2:
		switchString = "AUTOSWITCH2";
		break;
	case 3:
		switchString = "AUTOSWITCH3";
		break;
	case 0:
		switchString = "AUTOSWITCH0";
		break;
	default:
		break;
	}

	stripString = UI_GetStripEdString("INGAMETEXT", (char *)switchString);

	if (stripString)
	{
		Text_Paint(rect->x, rect->y, scale, color, stripString, 0, 0, textStyle, iMenuFont);
	}
}

static void UI_DrawJoinGameType(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
	if (ui_joinGameType.integer < 0 || ui_joinGameType.integer >= uiInfo.numJoinGameTypes) {
		trap_Cvar_Set("ui_joinGameType", "0");
	}
  Text_Paint(rect->x, rect->y, scale, color, uiInfo.joinGameTypes[ui_joinGameType.integer].gameType , 0, 0, textStyle, iMenuFont);
}



static int UI_TeamIndexFromName(const char *name) {
  int i;

  if (name && *name) {
    for (i = 0; i < uiInfo.teamCount; i++) {
      if (Q_stricmp(name, uiInfo.teamList[i].teamName) == 0) {
        return i;
      }
    }
  } 

  return 0;

}

static void UI_DrawClanLogo(rectDef_t *rect, float scale, vec4_t color) {
  int i;
  i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
  if (i >= 0 && i < uiInfo.teamCount) {
  	trap_R_SetColor( color );

		if (uiInfo.teamList[i].teamIcon == -1) {
      uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
      uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal",uiInfo.teamList[i].imageName));
      uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
		}

  	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
    trap_R_SetColor(NULL);
  }
}

static void UI_DrawClanCinematic(rectDef_t *rect, float scale, vec4_t color) {
  int i;
  i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
  if (i >= 0 && i < uiInfo.teamCount) {

		if (uiInfo.teamList[i].cinematic >= -2) {
			if (uiInfo.teamList[i].cinematic == -1) {
				uiInfo.teamList[i].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.teamList[i].imageName), 0, 0, 0, 0, (CIN_loop | CIN_silent) );
			}
			if (uiInfo.teamList[i].cinematic >= 0) {
			  trap_CIN_RunCinematic(uiInfo.teamList[i].cinematic);
				trap_CIN_SetExtents(uiInfo.teamList[i].cinematic, rect->x, rect->y, rect->w, rect->h);
	 			trap_CIN_DrawCinematic(uiInfo.teamList[i].cinematic);
			} else {
			  	trap_R_SetColor( color );
				UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal);
				trap_R_SetColor(NULL);
				uiInfo.teamList[i].cinematic = -2;
			}
		} else {
	  	trap_R_SetColor( color );
			UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
			trap_R_SetColor(NULL);
		}
	}

}

static void UI_DrawPreviewCinematic(rectDef_t *rect, float scale, vec4_t color) {
	if (uiInfo.previewMovie > -2) {
		uiInfo.previewMovie = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.movieList[uiInfo.movieIndex]), 0, 0, 0, 0, (CIN_loop | CIN_silent) );
		if (uiInfo.previewMovie >= 0) {
		  trap_CIN_RunCinematic(uiInfo.previewMovie);
			trap_CIN_SetExtents(uiInfo.previewMovie, rect->x, rect->y, rect->w, rect->h);
 			trap_CIN_DrawCinematic(uiInfo.previewMovie);
		} else {
			uiInfo.previewMovie = -2;
		}
	} 

}

static void UI_DrawSkill(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
  int i;
	i = trap_Cvar_VariableValue( "g_spSkill" );
  if (i < 1 || i > numSkillLevels) {
    i = 1;
  }
  Text_Paint(rect->x, rect->y, scale, color, (char *)UI_GetStripEdString("INGAMETEXT", (char *)skillLevels[i-1]),0, 0, textStyle, iMenuFont);
}


static void UI_DrawGenericNum(rectDef_t *rect, float scale, vec4_t color, int textStyle, int val, int min, int max, int type,int iMenuFont) 
{
	int i;
	char s[256];

	i = val;
	if (i < min || i > max) 
	{
		i = min;
	}

	Com_sprintf(s, sizeof(s), "%i", val);
	Text_Paint(rect->x, rect->y, scale, color, s,0, 0, textStyle, iMenuFont);
}

static void UI_DrawForceMastery(rectDef_t *rect, float scale, vec4_t color, int textStyle, int val, int min, int max, int iMenuFont)
{
	int i;
	const char *s;

	i = val;
	if (i < min || i > max) 
	{
		i = min;
	}

	s = (char *)UI_GetStripEdString("INGAMETEXT", forceMasteryLevels[i]);
	Text_Paint(rect->x, rect->y, scale, color, s, 0, 0, textStyle, iMenuFont);
}


static void UI_DrawSkinColor(rectDef_t *rect, float scale, vec4_t color, int textStyle, int val, int min, int max, int iMenuFont)
{
	int i;
	const char *s;

	i = val;
	if (i < min || i > max) 
	{
		i = min;
	}

	switch(val)
	{
	case TEAM_RED:
		s = "Red";
		break;
	case TEAM_BLUE:
		s = "Blue";
		break;
	default:
		s = "Default";
		break;
	}

	Text_Paint(rect->x, rect->y, scale, color, s,0, 0, textStyle, iMenuFont);
}

static void UI_DrawForceSide(rectDef_t *rect, float scale, vec4_t color, int textStyle, int val, int min, int max, int iMenuFont)
{
	int i;
	char s[256];
	menuDef_t *menu;
	
	char info[MAX_INFO_VALUE];

	i = val;
	if (i < min || i > max) 
	{
		i = min;
	}

	info[0] = '\0';
	trap_GetConfigString(CS_SERVERINFO, info, sizeof(info));

	if (atoi( Info_ValueForKey( info, "g_forceBasedTeams" ) ))
	{
		switch((int)(trap_Cvar_VariableValue("ui_myteam")))
		{
		case TEAM_RED:
			uiForceSide = FORCE_DARKSIDE;
			color[0] = 0.2f;
			color[1] = 0.2f;
			color[2] = 0.2f;
			break;
		case TEAM_BLUE:
			uiForceSide = FORCE_LIGHTSIDE;
			color[0] = 0.2f;
			color[1] = 0.2f;
			color[2] = 0.2f;
			break;
		default:
			break;
		}
	}

	if (val == FORCE_LIGHTSIDE)
	{
		trap_SP_GetStringTextString("MENUS3_FORCEDESC_LIGHT",s, sizeof(s));
		menu = Menus_FindByName("forcealloc");
		if (menu)
		{
			Menu_ShowItemByName(menu, "lightpowers", qtrue);
			Menu_ShowItemByName(menu, "darkpowers", qfalse);
			Menu_ShowItemByName(menu, "darkpowers_team", qfalse);

			Menu_ShowItemByName(menu, "lightpowers_team", qtrue);//(ui_gameType.integer >= GT_TEAM));

		}
		menu = Menus_FindByName("ingame_playerforce");
		if (menu)
		{
			Menu_ShowItemByName(menu, "lightpowers", qtrue);
			Menu_ShowItemByName(menu, "darkpowers", qfalse);
			Menu_ShowItemByName(menu, "darkpowers_team", qfalse);

			Menu_ShowItemByName(menu, "lightpowers_team", qtrue);//(ui_gameType.integer >= GT_TEAM));
		}
	}
	else
	{
		trap_SP_GetStringTextString("MENUS3_FORCEDESC_DARK",s, sizeof(s));
		menu = Menus_FindByName("forcealloc");
		if (menu)
		{
			Menu_ShowItemByName(menu, "lightpowers", qfalse);
			Menu_ShowItemByName(menu, "lightpowers_team", qfalse);
			Menu_ShowItemByName(menu, "darkpowers", qtrue);

			Menu_ShowItemByName(menu, "darkpowers_team", qtrue);//(ui_gameType.integer >= GT_TEAM));
		}
		menu = Menus_FindByName("ingame_playerforce");
		if (menu)
		{
			Menu_ShowItemByName(menu, "lightpowers", qfalse);
			Menu_ShowItemByName(menu, "lightpowers_team", qfalse);
			Menu_ShowItemByName(menu, "darkpowers", qtrue);

			Menu_ShowItemByName(menu, "darkpowers_team", qtrue);//(ui_gameType.integer >= GT_TEAM));
		}
	}

	Text_Paint(rect->x, rect->y, scale, color, s,0, 0, textStyle, iMenuFont);
}

qboolean UI_HasSetSaberOnly( void )
{
	char	info[MAX_INFO_STRING];
	int i = 0;
	int wDisable = 0;
	int	gametype = 0;

	trap_GetConfigString(CS_SERVERINFO, info, sizeof(info));

	gametype = atoi(Info_ValueForKey(info, "g_gametype"));

	if ( gametype == GT_JEDIMASTER )
	{ //set to 0 
		return qfalse;
	}

	if (gametype == GT_TOURNAMENT)
	{
		wDisable = atoi(Info_ValueForKey(info, "g_duelWeaponDisable"));
	}
	else
	{
		wDisable = atoi(Info_ValueForKey(info, "g_weaponDisable"));
	}

	while (i < WP_NUM_WEAPONS)
	{
		if (!(wDisable & (1 << i)) &&
			i != WP_SABER && i != WP_NONE)
		{
			return qfalse;
		}

		i++;
	}

	return qtrue;
}

static qboolean UI_AllForceDisabled(int force)
{
	int i;

	if (force)
	{
		for (i=0;i<NUM_FORCE_POWERS;i++)
		{
			if (!(force & (1<<i)))
			{
				return qfalse;
			}
		}

		return qtrue;
	}

	return qfalse;
}

qboolean UI_TrueJediEnabled( void )
{
	char	info[MAX_INFO_STRING];
	int		gametype = 0, disabledForce = 0, trueJedi = 0;
	qboolean saberOnly = qfalse, allForceDisabled = qfalse;

	trap_GetConfigString( CS_SERVERINFO, info, sizeof(info) );

	//already have serverinfo at this point for stuff below. Don't bother trying to use ui_forcePowerDisable.
	//if (ui_forcePowerDisable.integer)
	//if (atoi(Info_ValueForKey(info, "g_forcePowerDisable")))
	disabledForce = atoi(Info_ValueForKey(info, "g_forcePowerDisable"));
	allForceDisabled = UI_AllForceDisabled(disabledForce);
	gametype = atoi(Info_ValueForKey(info, "g_gametype"));
	saberOnly = UI_HasSetSaberOnly();

	if ( gametype == GT_HOLOCRON 
		|| gametype == GT_JEDIMASTER 
		|| saberOnly 
		|| allForceDisabled )
	{
		trueJedi = 0;
	}
	else
	{
		trueJedi = atoi( Info_ValueForKey( info, "g_jediVmerc" ) );
	}
	return (trueJedi != 0);
}

static void UI_DrawJediNonJedi(rectDef_t *rect, float scale, vec4_t color, int textStyle, int val, int min, int max, int iMenuFont)
{
	int i;
	char s[256];
	//menuDef_t *menu;
	
	char info[MAX_INFO_VALUE];

	i = val;
	if (i < min || i > max) 
	{
		i = min;
	}

	info[0] = '\0';
	trap_GetConfigString(CS_SERVERINFO, info, sizeof(info));

	if ( !UI_TrueJediEnabled() )
	{//true jedi mode is not on, do not draw this button type
		return;
	}

	if ( val == FORCE_NONJEDI )
	{
		trap_SP_GetStringTextString("MENUS0_NO",s, sizeof(s));
	}
	else
	{
		trap_SP_GetStringTextString("MENUS0_YES",s, sizeof(s));
	}

	Text_Paint(rect->x, rect->y, scale, color, s,0, 0, textStyle, iMenuFont);
}

static void UI_DrawTeamName(rectDef_t *rect, float scale, vec4_t color, qboolean blue, int textStyle, int iMenuFont) {
  int i;
  i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueTeam" : "ui_redTeam"));
  if (i >= 0 && i < uiInfo.teamCount) {
    Text_Paint(rect->x, rect->y, scale, color, va("%s: %s", (blue) ? "Blue" : "Red", uiInfo.teamList[i].teamName),0, 0, textStyle, iMenuFont);
  }
}

static void UI_DrawTeamMember(rectDef_t *rect, float scale, vec4_t color, qboolean blue, int num, int textStyle, int iMenuFont) 
{
	// 0 - None
	// 1 - Human
	// 2..NumCharacters - Bot
	int value = trap_Cvar_VariableValue(va(blue ? "ui_blueteam%i" : "ui_redteam%i", num));
	const char *text;
	int maxcl = trap_Cvar_VariableValue( "sv_maxClients" );
	vec4_t finalColor;
	int numval = num;

	numval *= 2;

	if (blue)
	{
		numval -= 1;
	}

	finalColor[0] = color[0];
	finalColor[1] = color[1];
	finalColor[2] = color[2];
	finalColor[3] = color[3];

	if (numval > maxcl)
	{
		finalColor[0] *= 0.2f;
		finalColor[1] *= 0.2f;
		finalColor[2] *= 0.2f;

		value = -1;
	}

	if (value <= 1) {
		if (value == -1)
		{
			//text = "Closed";
			text = UI_GetStripEdString("MENUS3", "CLOSED");
		}
		else
		{
			//text = "Human";
			text = UI_GetStripEdString("MENUS3", "HUMAN");
		}
	} else {
		value -= 2;

		/*if (ui_actualNetGameType.integer >= GT_TEAM) {
			if (value >= uiInfo.characterCount) {
				value = 0;
			}
			text = uiInfo.characterList[value].name;
		} else {*/
			if (value >= UI_GetNumBots()) {
				value = 1;
			}
			text = UI_GetBotNameByNumber(value);
		//}
	}

  Text_Paint(rect->x, rect->y, scale, finalColor, text, 0, 0, textStyle, iMenuFont);
}

static void UI_DrawEffects(rectDef_t *rect, float scale, vec4_t color) 
{
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiSaberColorShaders[uiInfo.effectsColor]);
}

static void UI_DrawMapPreview(rectDef_t *rect, float scale, vec4_t color, qboolean net) {
	int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
	if (map < 0 || map >= uiInfo.mapCount) {
		if (net) {
			ui_currentNetMap.integer = 0;
			trap_Cvar_Set("ui_currentNetMap", "0");
		} else {
			ui_currentMap.integer = 0;
			trap_Cvar_Set("ui_currentMap", "0");
		}
		map = 0;
	}

	if (uiInfo.mapList[map].levelShot == -1) {
		uiInfo.mapList[map].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[map].imageName);
	}

	if (uiInfo.mapList[map].levelShot > 0) {
		UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.mapList[map].levelShot);
	} else {
		UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, trap_R_RegisterShaderNoMip("menu/art/unknownmap"));
	}
}						 


static void UI_DrawMapTimeToBeat(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
	int minutes, seconds, time;
	if (ui_currentMap.integer < 0 || ui_currentMap.integer >= uiInfo.mapCount) {
		ui_currentMap.integer = 0;
		trap_Cvar_Set("ui_currentMap", "0");
	}

	time = uiInfo.mapList[ui_currentMap.integer].timeToBeat[uiInfo.gameTypes[ui_gameType.integer].gtEnum];

	minutes = time / 60;
	seconds = time % 60;

  Text_Paint(rect->x, rect->y, scale, color, va("%02i:%02i", minutes, seconds), 0, 0, textStyle, iMenuFont);
}



static void UI_DrawMapCinematic(rectDef_t *rect, float scale, vec4_t color, qboolean net) {

	int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer; 
	if (map < 0 || map >= uiInfo.mapCount) {
		if (net) {
			ui_currentNetMap.integer = 0;
			trap_Cvar_Set("ui_currentNetMap", "0");
		} else {
			ui_currentMap.integer = 0;
			trap_Cvar_Set("ui_currentMap", "0");
		}
		map = 0;
	}

	if (uiInfo.mapList[map].cinematic >= -1) {
		if (uiInfo.mapList[map].cinematic == -1) {
			uiInfo.mapList[map].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[map].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent) );
		}
		if (uiInfo.mapList[map].cinematic >= 0) {
		  trap_CIN_RunCinematic(uiInfo.mapList[map].cinematic);
		  trap_CIN_SetExtents(uiInfo.mapList[map].cinematic, rect->x, rect->y, rect->w, rect->h);
 			trap_CIN_DrawCinematic(uiInfo.mapList[map].cinematic);
		} else {
			uiInfo.mapList[map].cinematic = -2;
		}
	} else {
		UI_DrawMapPreview(rect, scale, color, net);
	}
}

static void UI_SetForceDisabled(int force)
{
	int i = 0;

	if (force)
	{
		while (i < NUM_FORCE_POWERS)
		{
			if (force & (1 << i))
			{
				uiForcePowersDisabled[i] = qtrue;

				if (i != FP_LEVITATION && i != FP_SABERATTACK && i != FP_SABERDEFEND)
				{
					uiForcePowersRank[i] = 0;
				}
				else
				{
					if (i == FP_LEVITATION)
					{
						uiForcePowersRank[i] = 1;
					}
					else
					{
						uiForcePowersRank[i] = 3;
					}
				}
			}
			else
			{
				uiForcePowersDisabled[i] = qfalse;
			}
			i++;
		}
	}
	else
	{
		i = 0;

		while (i < NUM_FORCE_POWERS)
		{
			uiForcePowersDisabled[i] = qfalse;
			i++;
		}
	}
}

void UpdateForceStatus()
{
	menuDef_t *menu;

	// Currently we don't make a distinction between those that wish to play Jedi of lower than maximum skill.
/*	if (ui_forcePowerDisable.integer)
	{
		uiForceRank = 0;
		uiForceAvailable = 0;
		uiForceUsed = 0;
	}
	else
	{
		uiForceRank = uiMaxRank;
		uiForceUsed = 0;
		uiForceAvailable = forceMasteryPoints[uiForceRank];
	}
*/
	menu = Menus_FindByName("ingame_player");
	if (menu)
	{
		char	info[MAX_INFO_STRING];
		int		disabledForce = 0;
		qboolean trueJedi = qfalse, allForceDisabled = qfalse;

		trap_GetConfigString( CS_SERVERINFO, info, sizeof(info) );

		//already have serverinfo at this point for stuff below. Don't bother trying to use ui_forcePowerDisable.
		//if (ui_forcePowerDisable.integer)
		//if (atoi(Info_ValueForKey(info, "g_forcePowerDisable")))
		disabledForce = atoi(Info_ValueForKey(info, "g_forcePowerDisable"));
		allForceDisabled = UI_AllForceDisabled(disabledForce);
		trueJedi = UI_TrueJediEnabled();

		if ( !trueJedi || allForceDisabled )
		{
			Menu_ShowItemByName(menu, "jedinonjedi", qfalse);
		}
		else
		{
			Menu_ShowItemByName(menu, "jedinonjedi", qtrue);
		}
		if ( allForceDisabled == qtrue || (trueJedi && uiJediNonJedi == FORCE_NONJEDI) )
		{	// No force stuff
			Menu_ShowItemByName(menu, "noforce", qtrue);
			Menu_ShowItemByName(menu, "yesforce", qfalse);
			// We don't want the saber explanation to say "configure saber attack 1" since we can't.
			Menu_ShowItemByName(menu, "sabernoneconfigme", qfalse);
		}
		else
		{
			UI_SetForceDisabled(disabledForce);
			Menu_ShowItemByName(menu, "noforce", qfalse);
			Menu_ShowItemByName(menu, "yesforce", qtrue);
		}

		//Moved this to happen after it's done with force power disabling stuff
		if (uiForcePowersRank[FP_SABERATTACK] > 0 || ui_freeSaber.integer)
		{	// Show lightsaber stuff.
			Menu_ShowItemByName(menu, "nosaber", qfalse);
			Menu_ShowItemByName(menu, "yessaber", qtrue);
		}
		else
		{
			Menu_ShowItemByName(menu, "nosaber", qtrue);
			Menu_ShowItemByName(menu, "yessaber", qfalse);
		}

		// The leftmost button should be "apply" unless you are in spectator, where you can join any team.
		if ((int)(trap_Cvar_VariableValue("ui_myteam")) != TEAM_SPECTATOR)
		{
			Menu_ShowItemByName(menu, "playerapply", qtrue);
			Menu_ShowItemByName(menu, "playerforcejoin", qfalse);
			Menu_ShowItemByName(menu, "playerforcered", qfalse);
			Menu_ShowItemByName(menu, "playerforceblue", qfalse);
			Menu_ShowItemByName(menu, "playerforcespectate", qtrue);
		}
		else
		{
			// Set or reset buttons based on choices
			if (atoi(Info_ValueForKey(info, "g_gametype")) >= GT_TEAM)
			{	// This is a team-based game.
				Menu_ShowItemByName(menu, "playerforcespectate", qtrue);
				
				// This is disabled, always show both sides from spectator.
				if ( 0 && atoi(Info_ValueForKey(info, "g_forceBasedTeams")))
				{	// Show red or blue based on what side is chosen.
					if (uiForceSide==FORCE_LIGHTSIDE)
					{
						Menu_ShowItemByName(menu, "playerforcered", qfalse);
						Menu_ShowItemByName(menu, "playerforceblue", qtrue);
					}
					else if (uiForceSide==FORCE_DARKSIDE)
					{
						Menu_ShowItemByName(menu, "playerforcered", qtrue);
						Menu_ShowItemByName(menu, "playerforceblue", qfalse);
					}
					else
					{
						Menu_ShowItemByName(menu, "playerforcered", qtrue);
						Menu_ShowItemByName(menu, "playerforceblue", qtrue);
					}
				}
				else
				{
					Menu_ShowItemByName(menu, "playerforcered", qtrue);
					Menu_ShowItemByName(menu, "playerforceblue", qtrue);
				}
			}
			else
			{
				Menu_ShowItemByName(menu, "playerforcered", qfalse);
				Menu_ShowItemByName(menu, "playerforceblue", qfalse);
			}

			Menu_ShowItemByName(menu, "playerapply", qfalse);
			Menu_ShowItemByName(menu, "playerforcejoin", qtrue);
			Menu_ShowItemByName(menu, "playerforcespectate", qtrue);
		}
	}


	if ( !UI_TrueJediEnabled() && serverGameType >= GT_TEAM )
	{// Take the current team and force a skin color based on it.
		int oldColor = uiSkinColor;
		switch((int)(trap_Cvar_VariableValue("ui_myteam")))
		{
		case TEAM_RED:
			uiSkinColor = SKINCOLOR_RED;
			uiInfo.effectsColor = SABER_RED;
			break;
		case TEAM_BLUE:
			uiSkinColor = SKINCOLOR_BLUE;
			uiInfo.effectsColor = SABER_BLUE;
			break;
		default:
			//uiSkinColor = SKINCOLOR_DEFAULT;
			UI_SetTeamColorFromModel(UI_GetModelWithSkin(ui_team_model.string));
			break;
		}

		if ( oldColor != uiSkinColor )
		{ // Scroll to the skin
			int selModel = UI_HeadIndexForModel(UI_GetModelWithTeamColor(ui_model.string));

			if ( selModel != -1 ) {
				Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS, selModel, NULL);
				UI_FeederScrollTo(FEEDER_Q3HEADS, selModel);
			} else {
				Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS, 0, NULL);
			}
		}
	}
}



static qboolean updateModel = qtrue;
/*
static qboolean q3Model = qfalse;

static void UI_DrawPlayerModel(rectDef_t *rect) {
  static playerInfo_t info;
  char model[MAX_QPATH];
  char team[256];
	char head[256];
	vec3_t	viewangles;
	vec3_t	moveangles;

	  if (trap_Cvar_VariableValue("ui_Q3Model")) {
	  strcpy(model, UI_Cvar_VariableString("model"));
		strcpy(head, UI_Cvar_VariableString("headmodel"));
		if (!q3Model) {
			q3Model = qtrue;
			updateModel = qtrue;
		}
		team[0] = '\0';
	} else {

		strcpy(team, UI_Cvar_VariableString("ui_teamName"));
		strcpy(model, UI_Cvar_VariableString("team_model"));
		strcpy(head, UI_Cvar_VariableString("team_headmodel"));
		if (q3Model) {
			q3Model = qfalse;
			updateModel = qtrue;
		}
	}
  if (updateModel) {
  	memset( &info, 0, sizeof(playerInfo_t) );
  	viewangles[YAW]   = 180 - 10;
  	viewangles[PITCH] = 0;
  	viewangles[ROLL]  = 0;
  	VectorClear( moveangles );
    UI_PlayerInfo_SetModel( &info, model, head, team);
    UI_PlayerInfo_SetInfo( &info, TORSO_WEAPONREADY3, TORSO_WEAPONREADY3, viewangles, vec3_origin, WP_BRYAR_PISTOL, qfalse );
//		UI_RegisterClientModelname( &info, model, head, team);
    updateModel = qfalse;
  }

  UI_DrawPlayer( rect->x, rect->y, rect->w, rect->h, &info, uiInfo.uiDC.realTime / 2);

}
*/
static void UI_DrawNetSource(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) 
{
	if (ui_netSource.integer < 0 || ui_netSource.integer >= numNetSources)
	{
		ui_netSource.integer = 0;
	}

	trap_SP_GetStringTextString("MENUS3_SOURCE", holdSPString, sizeof(holdSPString) );
	Text_Paint(rect->x, rect->y, scale, color, va("%s %s",holdSPString,
		 GetNetSourceString(ui_netSource.integer)), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawNetMapPreview(rectDef_t *rect, float scale, vec4_t color) {

	if (uiInfo.serverStatus.currentServerPreview > 0) {
		UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.serverStatus.currentServerPreview);
	} else {
		UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, trap_R_RegisterShaderNoMip("menu/art/unknownmap"));
	}
}

static void UI_DrawNetMapCinematic(rectDef_t *rect, float scale, vec4_t color) {
	if (ui_currentNetMap.integer < 0 || ui_currentNetMap.integer >= uiInfo.mapCount) {
		ui_currentNetMap.integer = 0;
		trap_Cvar_Set("ui_currentNetMap", "0");
	}

	if (uiInfo.serverStatus.currentServerCinematic >= 0) {
	  trap_CIN_RunCinematic(uiInfo.serverStatus.currentServerCinematic);
	  trap_CIN_SetExtents(uiInfo.serverStatus.currentServerCinematic, rect->x, rect->y, rect->w, rect->h);
 	  trap_CIN_DrawCinematic(uiInfo.serverStatus.currentServerCinematic);
	} else {
		UI_DrawNetMapPreview(rect, scale, color);
	}
}



static void UI_DrawNetFilter(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) 
{
	if (ui_serverFilterType.integer < 0 || ui_serverFilterType.integer > numServerFilters) 
	{
		ui_serverFilterType.integer = 0;
	}

	if (!uiInfo.inGameLoad)
		trap_SP_GetStringTextString("MV_GAME_VERSION", holdSPString, sizeof(holdSPString));
	else
		trap_SP_GetStringTextString("MENUS3_GAME", holdSPString, sizeof(holdSPString));

	Text_Paint(rect->x, rect->y, scale, color, va("%s %s",holdSPString,
		 serverFilters[ui_serverFilterType.integer].description), 0, 0, textStyle, iMenuFont);
}


static void UI_DrawTier(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
  int i;
	i = trap_Cvar_VariableValue( "ui_currentTier" );
  if (i < 0 || i >= uiInfo.tierCount) {
    i = 0;
  }
  Text_Paint(rect->x, rect->y, scale, color, va("Tier: %s", uiInfo.tierList[i].tierName),0, 0, textStyle, iMenuFont);
}

static void UI_DrawTierMap(rectDef_t *rect, int index) {
  int i;
	i = trap_Cvar_VariableValue( "ui_currentTier" );
  if (i < 0 || i >= uiInfo.tierCount) {
    i = 0;
  }

	if (uiInfo.tierList[i].mapHandles[index] == -1) {
		uiInfo.tierList[i].mapHandles[index] = trap_R_RegisterShaderNoMip(va("levelshots/%s", uiInfo.tierList[i].maps[index]));
	}
												 
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.tierList[i].mapHandles[index]);
}

static const char *UI_EnglishMapName(const char *map) {
	int i;
	for (i = 0; i < uiInfo.mapCount; i++) {
		if (Q_stricmp(map, uiInfo.mapList[i].mapLoadName) == 0) {
			return uiInfo.mapList[i].mapName;
		}
	}
	return "";
}

static void UI_DrawTierMapName(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
  int i, j;
	i = trap_Cvar_VariableValue( "ui_currentTier" );
  if (i < 0 || i >= uiInfo.tierCount) {
    i = 0;
  }
	j = trap_Cvar_VariableValue("ui_currentMap");
	if (j < 0 || j > MAPS_PER_TIER) {
		j = 0;
	}

  Text_Paint(rect->x, rect->y, scale, color, UI_EnglishMapName(uiInfo.tierList[i].maps[j]), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawTierGameType(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
  int i, j;
	i = trap_Cvar_VariableValue( "ui_currentTier" );
  if (i < 0 || i >= uiInfo.tierCount) {
    i = 0;
  }
	j = trap_Cvar_VariableValue("ui_currentMap");
	if (j < 0 || j > MAPS_PER_TIER) {
		j = 0;
	}

  Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[uiInfo.tierList[i].gameTypes[j]].gameType , 0, 0, textStyle,iMenuFont);
}


static const char *UI_AIFromName(const char *name) {
	int j;
	for (j = 0; j < uiInfo.aliasCount; j++) {
		if (Q_stricmp(uiInfo.aliasList[j].name, name) == 0) {
			return uiInfo.aliasList[j].ai;
		}
	}
	return "Kyle";
}


/*
static qboolean updateOpponentModel = qtrue;
static void UI_DrawOpponent(rectDef_t *rect) {
  static playerInfo_t info2;
  char model[MAX_QPATH];
  char headmodel[MAX_QPATH];
  char team[256];
	vec3_t	viewangles;
	vec3_t	moveangles;
  
	if (updateOpponentModel) {
		
		strcpy(model, UI_Cvar_VariableString("ui_opponentModel"));
	  strcpy(headmodel, UI_Cvar_VariableString("ui_opponentModel"));
		team[0] = '\0';

  	memset( &info2, 0, sizeof(playerInfo_t) );
  	viewangles[YAW]   = 180 - 10;
  	viewangles[PITCH] = 0;
  	viewangles[ROLL]  = 0;
  	VectorClear( moveangles );
    UI_PlayerInfo_SetModel( &info2, model, headmodel, "");
    UI_PlayerInfo_SetInfo( &info2, TORSO_WEAPONREADY3, TORSO_WEAPONREADY3, viewangles, vec3_origin, WP_BRYAR_PISTOL, qfalse );
		UI_RegisterClientModelname( &info2, model, headmodel, team);
    updateOpponentModel = qfalse;
  }

  UI_DrawPlayer( rect->x, rect->y, rect->w, rect->h, &info2, uiInfo.uiDC.realTime / 2);

}
*/
static void UI_NextOpponent() {
  int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
  int j = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
	i++;
	if (i >= uiInfo.teamCount) {
		i = 0;
	}
	if (i == j) {
		i++;
		if ( i >= uiInfo.teamCount) {
			i = 0;
		}
	}
 	trap_Cvar_Set( "ui_opponentName", uiInfo.teamList[i].teamName );
}

static void UI_PriorOpponent() {
  int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
  int j = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
	i--;
	if (i < 0) {
		i = uiInfo.teamCount - 1;
	}
	if (i == j) {
		i--;
		if ( i < 0) {
			i = uiInfo.teamCount - 1;
		}
	}
 	trap_Cvar_Set( "ui_opponentName", uiInfo.teamList[i].teamName );
}

static void	UI_DrawPlayerLogo(rectDef_t *rect, vec3_t color) {
  int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));

	if (uiInfo.teamList[i].teamIcon == -1) {
    uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
    uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal",uiInfo.teamList[i].imageName));
    uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
	}

 	trap_R_SetColor( color );
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon );
 	trap_R_SetColor( NULL );
}

static void	UI_DrawPlayerLogoMetal(rectDef_t *rect, vec3_t color) {
  int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
	if (uiInfo.teamList[i].teamIcon == -1) {
    uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
    uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal",uiInfo.teamList[i].imageName));
    uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
	}

 	trap_R_SetColor( color );
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal );
 	trap_R_SetColor( NULL );
}

static void	UI_DrawPlayerLogoName(rectDef_t *rect, vec3_t color) {
  int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
	if (uiInfo.teamList[i].teamIcon == -1) {
    uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
    uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal",uiInfo.teamList[i].imageName));
    uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
	}

 	trap_R_SetColor( color );
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Name );
 	trap_R_SetColor( NULL );
}

static void	UI_DrawOpponentLogo(rectDef_t *rect, vec3_t color) {
  int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
	if (uiInfo.teamList[i].teamIcon == -1) {
    uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
    uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal",uiInfo.teamList[i].imageName));
    uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
	}

 	trap_R_SetColor( color );
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon );
 	trap_R_SetColor( NULL );
}

static void	UI_DrawOpponentLogoMetal(rectDef_t *rect, vec3_t color) {
  int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
	if (uiInfo.teamList[i].teamIcon == -1) {
    uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
    uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal",uiInfo.teamList[i].imageName));
    uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
	}

 	trap_R_SetColor( color );
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal );
 	trap_R_SetColor( NULL );
}

static void	UI_DrawOpponentLogoName(rectDef_t *rect, vec3_t color) {
  int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
	if (uiInfo.teamList[i].teamIcon == -1) {
    uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
    uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal",uiInfo.teamList[i].imageName));
    uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
	}

 	trap_R_SetColor( color );
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Name );
 	trap_R_SetColor( NULL );
}

static void UI_DrawAllMapsSelection(rectDef_t *rect, float scale, vec4_t color, int textStyle, qboolean net, int iMenuFont) {
	int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
	if (map >= 0 && map < uiInfo.mapCount) {
	  Text_Paint(rect->x, rect->y, scale, color, uiInfo.mapList[map].mapName, 0, 0, textStyle, iMenuFont);
	}
}

static void UI_DrawOpponentName(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
  Text_Paint(rect->x, rect->y, scale, color, UI_Cvar_VariableString("ui_opponentName"), 0, 0, textStyle, iMenuFont);
}

static int UI_OwnerDrawWidth(int ownerDraw, float scale) {
	int i, h, value, findex, iUse = 0;
	const char *text;
	const char *s = NULL;


  switch (ownerDraw) {
    case UI_HANDICAP:
			  h = Com_Clamp( 5, 100, trap_Cvar_VariableValue("handicap") );
				i = 20 - h / 5;
				s = handicapValues[i];
      break;
    case UI_SKIN_COLOR:
		switch(uiSkinColor)
		{
		case SKINCOLOR_RED:
			s = "Red";
			break;
		case SKINCOLOR_BLUE:
			s = "Blue";
			break;
		case SKINCOLOR_OTHER:
			s = "Other";
			break;
		default:
			s = "Default";
			break;
		}
		break;
    case UI_FORCE_SIDE:
		i = uiForceSide;
		if (i < 1 || i > 2) {
			i = 1;
		}

		if (i == FORCE_LIGHTSIDE)
		{
//			s = "Light";
			s = (char *)UI_GetStripEdString("MENUS3", "FORCEDESC_LIGHT");
		}
		else
		{
//			s = "Dark";
			s = (char *)UI_GetStripEdString("MENUS3", "FORCEDESC_DARK");
		}
		break;
    case UI_JEDI_NONJEDI:
		i = uiJediNonJedi;
		if (i < 0 || i > 1)
		{
			i = 0;
		}

		if (i == FORCE_NONJEDI)
		{
//			s = "Non-Jedi";
			s = (char *)UI_GetStripEdString("MENUS0", "NO");
		}
		else
		{
//			s = "Jedi";
			s = (char *)UI_GetStripEdString("MENUS0", "YES");
		}
		break;
    case UI_FORCE_RANK:
		i = Com_Clampi(1, MAX_FORCE_RANK, uiForceRank);

		s = (char *)UI_GetStripEdString("INGAMETEXT", forceMasteryLevels[i]);
		break;
	case UI_FORCE_RANK_HEAL:
	case UI_FORCE_RANK_LEVITATION:
	case UI_FORCE_RANK_SPEED:
	case UI_FORCE_RANK_PUSH:
	case UI_FORCE_RANK_PULL:
	case UI_FORCE_RANK_TELEPATHY:
	case UI_FORCE_RANK_GRIP:
	case UI_FORCE_RANK_LIGHTNING:
	case UI_FORCE_RANK_RAGE:
	case UI_FORCE_RANK_PROTECT:
	case UI_FORCE_RANK_ABSORB:
	case UI_FORCE_RANK_TEAM_HEAL:
	case UI_FORCE_RANK_TEAM_FORCE:
	case UI_FORCE_RANK_DRAIN:
	case UI_FORCE_RANK_SEE:
	case UI_FORCE_RANK_SABERATTACK:
	case UI_FORCE_RANK_SABERDEFEND:
	case UI_FORCE_RANK_SABERTHROW:
		findex = (ownerDraw - UI_FORCE_RANK)-1;
		//this will give us the index as long as UI_FORCE_RANK is always one below the first force rank index
		i = uiForcePowersRank[findex];

		if (i < 0 || i > NUM_FORCE_POWER_LEVELS-1)
		{
			i = 0;
		}

		s = va("%i", uiForcePowersRank[findex]);
		break;
    case UI_CLANNAME:
				s = UI_Cvar_VariableString("ui_teamName");
      break;
    case UI_GAMETYPE:
				s = uiInfo.gameTypes[ui_gameType.integer].gameType;
      break;
    case UI_SKILL:
				i = trap_Cvar_VariableValue( "g_spSkill" );
				if (i < 1 || i > numSkillLevels) {
					i = 1;
				}
			  s = (char *)UI_GetStripEdString("INGAMETEXT", (char *)skillLevels[i-1]);
      break;
    case UI_BLUETEAMNAME:
			  i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_blueTeam"));
			  if (i >= 0 && i < uiInfo.teamCount) {
			    s = va("%s: %s", "Blue", uiInfo.teamList[i].teamName);
			  }
      break;
    case UI_REDTEAMNAME:
			  i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_redTeam"));
			  if (i >= 0 && i < uiInfo.teamCount) {
			    s = va("%s: %s", "Red", uiInfo.teamList[i].teamName);
			  }
      break;
    case UI_BLUETEAM1:
		case UI_BLUETEAM2:
		case UI_BLUETEAM3:
		case UI_BLUETEAM4:
		case UI_BLUETEAM5:
		case UI_BLUETEAM6:
		case UI_BLUETEAM7:
		case UI_BLUETEAM8:
			if (ownerDraw <= UI_BLUETEAM5)
			{
			  iUse = ownerDraw-UI_BLUETEAM1 + 1;
			}
			else
			{
			  iUse = ownerDraw-274; //unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
			}

			value = trap_Cvar_VariableValue(va("ui_blueteam%i", iUse));
			if (value <= 1) {
				text = "Human";
			} else {
				value -= 2;
				if (value >= uiInfo.aliasCount) {
					value = 1;
				}
				text = uiInfo.aliasList[value].name;
			}
			s = va("%i. %s", iUse, text);
      break;
    case UI_REDTEAM1:
		case UI_REDTEAM2:
		case UI_REDTEAM3:
		case UI_REDTEAM4:
		case UI_REDTEAM5:
		case UI_REDTEAM6:
		case UI_REDTEAM7:
		case UI_REDTEAM8:
			if (ownerDraw <= UI_REDTEAM5)
			{
			  iUse = ownerDraw-UI_REDTEAM1 + 1;
			}
			else
			{
			  iUse = ownerDraw-277; //unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
			}

			value = trap_Cvar_VariableValue(va("ui_redteam%i", iUse));
			if (value <= 1) {
				text = "Human";
			} else {
				value -= 2;
				if (value >= uiInfo.aliasCount) {
					value = 1;
				}
				text = uiInfo.aliasList[value].name;
			}
			s = va("%i. %s", iUse, text);
      break;
		case UI_NETSOURCE:
			if (ui_netSource.integer < 0 || ui_netSource.integer >= uiInfo.numJoinGameTypes) {
				ui_netSource.integer = 0;
			}
			trap_SP_GetStringTextString("MENUS3_SOURCE", holdSPString, sizeof(holdSPString));
			s = va("%s %s", holdSPString, GetNetSourceString(ui_netSource.integer));
			break;
		case UI_NETFILTER:
			if (ui_serverFilterType.integer < 0 || ui_serverFilterType.integer > numServerFilters) {
				ui_serverFilterType.integer = 0;
			}
			if (!uiInfo.inGameLoad)
				trap_SP_GetStringTextString("MV_GAME_VERSION", holdSPString, sizeof(holdSPString));
			else
				trap_SP_GetStringTextString("MENUS3_GAME", holdSPString, sizeof(holdSPString));
			s = va("%s %s", holdSPString, serverFilters[ui_serverFilterType.integer].description );
			break;
		case UI_TIER:
			break;
		case UI_TIER_MAPNAME:
			break;
		case UI_TIER_GAMETYPE:
			break;
		case UI_ALLMAPS_SELECTION:
			break;
		case UI_OPPONENT_NAME:
			break;
		case UI_KEYBINDSTATUS:
			if (Display_KeyBindPending()) {
				s = UI_GetStripEdString("INGAMETEXT", "WAITING_FOR_NEW_KEY");
			} else {
			//	s = "Press ENTER or CLICK to change, Press BACKSPACE to clear";
			}
			break;
		case UI_SERVERREFRESHDATE:
			s = UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource.integer));
			break;
    default:
      break;
  }

	if (s) {
		return Text_Width(s, scale, 0);
	}
	return 0;
}

static void UI_DrawBotName(rectDef_t *rect, float scale, vec4_t color, int textStyle,int iMenuFont) 
{
	int value = uiInfo.botIndex;
//	int game = trap_Cvar_VariableValue("g_gametype");
	const char *text = "";
	/*
	if (game >= GT_TEAM) {
		if (value >= uiInfo.characterCount) {
			value = 0;
		}
		text = uiInfo.characterList[value].name;
	} else {
	*/
		if (value >= UI_GetNumBots()) {
			value = 0;
		}
		text = UI_GetBotNameByNumber(value);
	//}
//  Text_Paint(rect->x, rect->y, scale, color, text, 0, 0, textStyle);
  Text_Paint(rect->x, rect->y, scale, color, text, 0, 0, textStyle,iMenuFont);
}

static void UI_DrawBotSkill(rectDef_t *rect, float scale, vec4_t color, int textStyle,int iMenuFont) 
{
	if (uiInfo.skillIndex >= 0 && uiInfo.skillIndex < numSkillLevels) 
	{
		Text_Paint(rect->x, rect->y, scale, color, (char *)UI_GetStripEdString("INGAMETEXT", (char *)skillLevels[uiInfo.skillIndex]), 0, 0, textStyle,iMenuFont);
	}
}

static void UI_DrawRedBlue(rectDef_t *rect, float scale, vec4_t color, int textStyle,int iMenuFont) 
{
	Text_Paint(rect->x, rect->y, scale, color, (uiInfo.redBlue == 0) ? "Red" : "Blue", 0, 0, textStyle,iMenuFont);
}

static void UI_DrawCrosshair(rectDef_t *rect, float scale, vec4_t color) {
 	trap_R_SetColor( color );
	if (uiInfo.currentCrosshair < 0 || uiInfo.currentCrosshair >= NUM_CROSSHAIRS) {
		uiInfo.currentCrosshair = 0;
	}
	UI_DrawHandlePic( rect->x, rect->y, rect->w, rect->h, uiInfo.uiDC.Assets.crosshairShader[uiInfo.currentCrosshair]);
 	trap_R_SetColor( NULL );
}

/*
===============
UI_BuildPlayerList
===============
*/
static void UI_BuildPlayerList() {
	uiClientState_t	cs;
	int		n, count, team, team2, playerTeamNumber;
	char	info[MAX_INFO_STRING];

	trap_GetClientState( &cs );
	trap_GetConfigString( CS_PLAYERS + cs.clientNum, info, MAX_INFO_STRING );
	uiInfo.playerNumber = cs.clientNum;
	uiInfo.teamLeader = atoi(Info_ValueForKey(info, "tl"));
	team = atoi(Info_ValueForKey(info, "t"));
	trap_GetConfigString( CS_SERVERINFO, info, sizeof(info) );
	count = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
	uiInfo.playerCount = 0;
	uiInfo.myTeamCount = 0;
	playerTeamNumber = 0;
	for( n = 0; n < count; n++ ) {
		trap_GetConfigString( CS_PLAYERS + n, info, MAX_INFO_STRING );

		if (info[0]) {
			Q_strncpyz( uiInfo.playerNames[uiInfo.playerCount], Info_ValueForKey( info, "n" ), MAX_NAME_LENGTH );
			Q_CleanStr( uiInfo.playerNames[uiInfo.playerCount], qtrue );
			uiInfo.playerIndexes[uiInfo.playerCount] = n;
			uiInfo.playerCount++;
			team2 = atoi(Info_ValueForKey(info, "t"));
			if (team2 == team && n != uiInfo.playerNumber) {
				Q_strncpyz( uiInfo.teamNames[uiInfo.myTeamCount], Info_ValueForKey( info, "n" ), MAX_NAME_LENGTH );
				Q_CleanStr( uiInfo.teamNames[uiInfo.myTeamCount], qtrue );
				uiInfo.teamClientNums[uiInfo.myTeamCount] = n;
				if (uiInfo.playerNumber == n) {
					playerTeamNumber = uiInfo.myTeamCount;
				}
				uiInfo.myTeamCount++;
			}
		}
	}

	if (!uiInfo.teamLeader) {
		trap_Cvar_Set("cg_selectedPlayer", va("%d", playerTeamNumber));
	}

	n = trap_Cvar_VariableValue("cg_selectedPlayer");
	if (n < 0 || n > uiInfo.myTeamCount) {
		n = 0;
	}


	if (n < uiInfo.myTeamCount) {
		trap_Cvar_Set("cg_selectedPlayerName", uiInfo.teamNames[n]);
	}
	else
	{
		trap_Cvar_Set("cg_selectedPlayerName", "Everyone");
	}

	if (!team || team == TEAM_SPECTATOR || !uiInfo.teamLeader)
	{
		n = uiInfo.myTeamCount;
		trap_Cvar_Set("cg_selectedPlayer", va("%d", n));
		trap_Cvar_Set("cg_selectedPlayerName", "N/A");
	}
}


static void UI_DrawSelectedPlayer(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) {
	if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
		uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
		UI_BuildPlayerList();
	}
  Text_Paint(rect->x, rect->y, scale, color, UI_Cvar_VariableString("cg_selectedPlayerName"), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawServerRefreshDate(rectDef_t *rect, float scale, vec4_t color, int textStyle, int iMenuFont) 
{
	if (uiInfo.serverStatus.refreshActive) 
	{
		vec4_t lowLight, newColor;
		lowLight[0] = 0.8f * color[0];
		lowLight[1] = 0.8f * color[1];
		lowLight[2] = 0.8f * color[2];
		lowLight[3] = 0.8f * color[3];
		LerpColor(color,lowLight,newColor,0.5f+0.5f*sin(uiInfo.uiDC.realTime / PULSE_DIVISOR));

		trap_SP_GetStringTextString("INGAMETEXT_GETTINGINFOFORSERVERS", holdSPString, sizeof(holdSPString));
		Text_Paint(rect->x, rect->y, scale, newColor, va((char *) holdSPString, trap_LAN_GetServerCount(ui_netSource.integer)), 0, 0, textStyle, iMenuFont);
	} 
	else 
	{
		char buff[64];
		char sServers[256];
		char sPlayers[256];

		Q_strncpyz(buff, UI_Cvar_VariableString(va("ui_lastServerRefresh_%i", ui_netSource.integer)), 64);
		trap_SP_GetStringTextString("INGAMETEXT_SERVER_REFRESHTIME", holdSPString, sizeof(holdSPString));
		trap_SP_GetStringTextString("MV_REFRESHDATA_SERVERS", sServers, sizeof(sServers));
		trap_SP_GetStringTextString("MV_REFRESHDATA_PLAYERS", sPlayers, sizeof(sPlayers));

		Text_Paint(rect->x - 8, rect->y, scale, color, va("%s: %s | %s %d, %s %d", holdSPString, buff, sServers, uiInfo.serverStatus.numDisplayServers, sPlayers, uiInfo.serverStatus.numPlayersOnServers), 0, 0, textStyle, iMenuFont); //new data
	}
}

static void UI_DrawServerMOTD(rectDef_t *rect, float scale, vec4_t color, int iMenuFont) {
	if (uiInfo.serverStatus.motdLen) {
		float maxX;
	 
		if (uiInfo.serverStatus.motdWidth == -1) {
			uiInfo.serverStatus.motdWidth = 0;
			uiInfo.serverStatus.motdPaintX = rect->x + 1;
			uiInfo.serverStatus.motdPaintX2 = -1;
		}

		if (uiInfo.serverStatus.motdOffset > uiInfo.serverStatus.motdLen) {
			uiInfo.serverStatus.motdOffset = 0;
			uiInfo.serverStatus.motdPaintX = rect->x + 1;
			uiInfo.serverStatus.motdPaintX2 = -1;
		}

		if (uiInfo.uiDC.realTime > uiInfo.serverStatus.motdTime) {
			uiInfo.serverStatus.motdTime = uiInfo.uiDC.realTime + 10;
			if (uiInfo.serverStatus.motdPaintX <= rect->x + 2) {
				if (uiInfo.serverStatus.motdOffset < uiInfo.serverStatus.motdLen) {
					uiInfo.serverStatus.motdPaintX += Text_Width(&uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset], scale, 1) - 1;
					uiInfo.serverStatus.motdOffset++;
				} else {
					uiInfo.serverStatus.motdOffset = 0;
					if (uiInfo.serverStatus.motdPaintX2 >= 0) {
						uiInfo.serverStatus.motdPaintX = uiInfo.serverStatus.motdPaintX2;
					} else {
						uiInfo.serverStatus.motdPaintX = rect->x + rect->w - 2;
					}
					uiInfo.serverStatus.motdPaintX2 = -1;
				}
			} else {
				//serverStatus.motdPaintX--;
				uiInfo.serverStatus.motdPaintX -= 2;
				if (uiInfo.serverStatus.motdPaintX2 >= 0) {
					//serverStatus.motdPaintX2--;
					uiInfo.serverStatus.motdPaintX2 -= 2;
				}
			}
		}

		maxX = rect->x + rect->w - 2;
		Text_Paint_Limit(&maxX, uiInfo.serverStatus.motdPaintX, rect->y + rect->h - 3, scale, color, &uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset], 0, 0, iMenuFont); 
		if (uiInfo.serverStatus.motdPaintX2 >= 0) {
			float maxX2 = rect->x + rect->w - 2;
			Text_Paint_Limit(&maxX2, uiInfo.serverStatus.motdPaintX2, rect->y + rect->h - 3, scale, color, uiInfo.serverStatus.motd, 0, uiInfo.serverStatus.motdOffset, iMenuFont); 
		}
		if (uiInfo.serverStatus.motdOffset && maxX > 0) {
			// if we have an offset ( we are skipping the first part of the string ) and we fit the string
			if (uiInfo.serverStatus.motdPaintX2 == -1) {
						uiInfo.serverStatus.motdPaintX2 = rect->x + rect->w - 2;
			}
		} else {
			uiInfo.serverStatus.motdPaintX2 = -1;
		}

	}
}

static void UI_DrawKeyBindStatus(rectDef_t *rect, float scale, vec4_t color, int textStyle,int iMenuFont) {
//	int ofs = 0; TTimo: unused
	if (Display_KeyBindPending()) 
	{
		Text_Paint(rect->x, rect->y, scale, color, UI_GetStripEdString("INGAMETEXT", "WAITING_FOR_NEW_KEY"), 0, 0, textStyle,iMenuFont);
	} else {
//		Text_Paint(rect->x, rect->y, scale, color, "Press ENTER or CLICK to change, Press BACKSPACE to clear", 0, 0, textStyle,iMenuFont);
	}
}

static void UI_DrawGLInfo(rectDef_t *rect, float scale, vec4_t color, int textStyle,int iMenuFont) 
{
	char * eptr;
	static char buff[32768]; // increased in jk2mv
	const char *lines[128*8];
	int y, numLines, i;

	Text_Paint(rect->x + 2, rect->y, scale, color, va("GL_VENDOR: %s", uiInfo.uiDC.glconfig.vendor_string), 0, 30, textStyle,iMenuFont);
	Text_Paint(rect->x + 2, rect->y + 15, scale, color, va("GL_VERSION: %s: %s", uiInfo.uiDC.glconfig.version_string,uiInfo.uiDC.glconfig.renderer_string), 0, 30, textStyle,iMenuFont);
	Text_Paint(rect->x + 2, rect->y + 30, scale, color, va ("GL_PIXELFORMAT: color(%d-bits) Z(%d-bits) stencil(%d-bits)", uiInfo.uiDC.glconfig.colorBits, uiInfo.uiDC.glconfig.depthBits, uiInfo.uiDC.glconfig.stencilBits), 0, 30, textStyle,iMenuFont);

	// build null terminated extension strings
	Q_strncpyz(buff, uiInfo.uiDC.glconfig.extensions_string, sizeof(buff));
	eptr = buff;
	y = rect->y + 45;
	numLines = 0;
	while ( y < rect->y + rect->h && *eptr )
	{
		while ( *eptr && *eptr == ' ' )
			*eptr++ = '\0';

		// track start of valid string
		if (*eptr && *eptr != ' ') 
		{
			lines[numLines++] = eptr;
		}

		while ( *eptr && *eptr != ' ' )
			eptr++;
	}

	i = 0;
	while (i < numLines) 
	{
		Text_Paint(rect->x + 2, y, scale, color, lines[i++], 0, 20, textStyle,iMenuFont);
		if (i < numLines) 
		{
			Text_Paint(rect->x + rect->w / 2, y, scale, color, lines[i++], 0, 20, textStyle,iMenuFont);
		}
		y += 10;
		if (y > rect->y + rect->h - 11) 
		{
			break;
		}
	}


}

/*
=================
UI_Version
=================
*/
static void UI_Version(rectDef_t *rect, float scale, vec4_t color, int iMenuFont) 
{
	int width;
	
	width = uiInfo.uiDC.textWidth(Q3_VERSION, scale, iMenuFont);

	uiInfo.uiDC.drawText(rect->x - width, rect->y, scale, color, Q3_VERSION, 0, 0, 0, iMenuFont);
}

/*
=================
UI_OwnerDraw
=================
*/
// FIXME: table drive
//
static void UI_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle,int iMenuFont) 
{
	rectDef_t rect;
	int findex;
	int drawRank = 0, iUse = 0;

	rect.x = x + text_x;
	rect.y = y + text_y;
	rect.w = w;
	rect.h = h;

  switch (ownerDraw) 
  {
    case UI_HANDICAP:
      UI_DrawHandicap(&rect, scale, color, textStyle, iMenuFont);
      break;
    case UI_SKIN_COLOR:
      UI_DrawSkinColor(&rect, scale, color, textStyle, uiSkinColor, TEAM_FREE, TEAM_BLUE, iMenuFont);
      break;
	case UI_FORCE_SIDE:
      UI_DrawForceSide(&rect, scale, color, textStyle, uiForceSide, 1, 2, iMenuFont);
      break;
	case UI_JEDI_NONJEDI:
      UI_DrawJediNonJedi(&rect, scale, color, textStyle, uiJediNonJedi, 0, 1, iMenuFont);
      break;
    case UI_FORCE_POINTS:
      UI_DrawGenericNum(&rect, scale, color, textStyle, uiForceAvailable, 1, forceMasteryPoints[MAX_FORCE_RANK], ownerDraw,iMenuFont);
      break;
	case UI_FORCE_MASTERY_SET:
      UI_DrawForceMastery(&rect, scale, color, textStyle, uiForceRank, 0, MAX_FORCE_RANK, iMenuFont);
      break;
    case UI_FORCE_RANK:
      UI_DrawForceMastery(&rect, scale, color, textStyle, uiForceRank, 0, MAX_FORCE_RANK, iMenuFont);
      break;
	case UI_FORCE_RANK_HEAL:
	case UI_FORCE_RANK_LEVITATION:
	case UI_FORCE_RANK_SPEED:
	case UI_FORCE_RANK_PUSH:
	case UI_FORCE_RANK_PULL:
	case UI_FORCE_RANK_TELEPATHY:
	case UI_FORCE_RANK_GRIP:
	case UI_FORCE_RANK_LIGHTNING:
	case UI_FORCE_RANK_RAGE:
	case UI_FORCE_RANK_PROTECT:
	case UI_FORCE_RANK_ABSORB:
	case UI_FORCE_RANK_TEAM_HEAL:
	case UI_FORCE_RANK_TEAM_FORCE:
	case UI_FORCE_RANK_DRAIN:
	case UI_FORCE_RANK_SEE:
	case UI_FORCE_RANK_SABERATTACK:
	case UI_FORCE_RANK_SABERDEFEND:
	case UI_FORCE_RANK_SABERTHROW:

//		uiForceRank
/*
		uiForceUsed
		// Only fields for white stars
		if (uiForceUsed<3)
		{
		    Menu_ShowItemByName(menu, "lightpowers_team", qtrue);
		}
		else if (uiForceUsed<6)
		{
		    Menu_ShowItemByName(menu, "lightpowers_team", qtrue);
		}
*/

		findex = (ownerDraw - UI_FORCE_RANK)-1;
		//this will give us the index as long as UI_FORCE_RANK is always one below the first force rank index
		if (uiForcePowerDarkLight[findex] && uiForceSide != uiForcePowerDarkLight[findex])
		{
			color[0] *= 0.5f;
			color[1] *= 0.5f;
			color[2] *= 0.5f;
		}
/*		else if (uiForceRank < UI_ForceColorMinRank[bgForcePowerCost[findex][FORCE_LEVEL_1]])
		{
			color[0] *= 0.5;
			color[1] *= 0.5;
			color[2] *= 0.5;
		}
*/		drawRank = uiForcePowersRank[findex];

		UI_DrawForceStars(&rect, scale, color, textStyle, findex, drawRank, 0, NUM_FORCE_POWER_LEVELS-1);
		break;
    case UI_EFFECTS:
      UI_DrawEffects(&rect, scale, color);
      break;
    case UI_PLAYERMODEL:
      //UI_DrawPlayerModel(&rect);
      break;
    case UI_CLANNAME:
      UI_DrawClanName(&rect, scale, color, textStyle, iMenuFont);
      break;
    case UI_CLANLOGO:
      UI_DrawClanLogo(&rect, scale, color);
      break;
    case UI_CLANCINEMATIC:
      UI_DrawClanCinematic(&rect, scale, color);
      break;
    case UI_PREVIEWCINEMATIC:
      UI_DrawPreviewCinematic(&rect, scale, color);
      break;
    case UI_GAMETYPE:
      UI_DrawGameType(&rect, scale, color, textStyle, iMenuFont);
      break;
    case UI_NETGAMETYPE:
      UI_DrawNetGameType(&rect, scale, color, textStyle, iMenuFont);
      break;
    case UI_AUTOSWITCHLIST:
      UI_DrawAutoSwitch(&rect, scale, color, textStyle, iMenuFont);
      break;
    case UI_JOINGAMETYPE:
	  UI_DrawJoinGameType(&rect, scale, color, textStyle, iMenuFont);
	  break;
    case UI_MAPPREVIEW:
      UI_DrawMapPreview(&rect, scale, color, qtrue);
      break;
    case UI_MAP_TIMETOBEAT:
      UI_DrawMapTimeToBeat(&rect, scale, color, textStyle, iMenuFont);
      break;
    case UI_MAPCINEMATIC:
      UI_DrawMapCinematic(&rect, scale, color, qfalse);
      break;
    case UI_STARTMAPCINEMATIC:
      UI_DrawMapCinematic(&rect, scale, color, qtrue);
      break;
    case UI_SKILL:
      UI_DrawSkill(&rect, scale, color, textStyle, iMenuFont);
      break;
    case UI_TOTALFORCESTARS:
//      UI_DrawTotalForceStars(&rect, scale, color, textStyle);
      break;
    case UI_BLUETEAMNAME:
      UI_DrawTeamName(&rect, scale, color, qtrue, textStyle, iMenuFont);
      break;
    case UI_REDTEAMNAME:
      UI_DrawTeamName(&rect, scale, color, qfalse, textStyle, iMenuFont);
      break;
    case UI_BLUETEAM1:
		case UI_BLUETEAM2:
		case UI_BLUETEAM3:
		case UI_BLUETEAM4:
		case UI_BLUETEAM5:
		case UI_BLUETEAM6:
		case UI_BLUETEAM7:
		case UI_BLUETEAM8:
	if (ownerDraw <= UI_BLUETEAM5)
	{
	  iUse = ownerDraw-UI_BLUETEAM1 + 1;
	}
	else
	{
	  iUse = ownerDraw-274; //unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
	}
      UI_DrawTeamMember(&rect, scale, color, qtrue, iUse, textStyle, iMenuFont);
      break;
    case UI_REDTEAM1:
		case UI_REDTEAM2:
		case UI_REDTEAM3:
		case UI_REDTEAM4:
		case UI_REDTEAM5:
		case UI_REDTEAM6:
		case UI_REDTEAM7:
		case UI_REDTEAM8:
	if (ownerDraw <= UI_REDTEAM5)
	{
	  iUse = ownerDraw-UI_REDTEAM1 + 1;
	}
	else
	{
	  iUse = ownerDraw-277; //unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
	}
      UI_DrawTeamMember(&rect, scale, color, qfalse, iUse, textStyle, iMenuFont);
      break;
		case UI_NETSOURCE:
      UI_DrawNetSource(&rect, scale, color, textStyle, iMenuFont);
			break;
    case UI_NETMAPPREVIEW:
      UI_DrawNetMapPreview(&rect, scale, color);
      break;
    case UI_NETMAPCINEMATIC:
      UI_DrawNetMapCinematic(&rect, scale, color);
      break;
		case UI_NETFILTER:
      UI_DrawNetFilter(&rect, scale, color, textStyle, iMenuFont);
			break;
		case UI_TIER:
			UI_DrawTier(&rect, scale, color, textStyle, iMenuFont);
			break;
		case UI_OPPONENTMODEL:
			//UI_DrawOpponent(&rect);
			break;
		case UI_TIERMAP1:
			UI_DrawTierMap(&rect, 0);
			break;
		case UI_TIERMAP2:
			UI_DrawTierMap(&rect, 1);
			break;
		case UI_TIERMAP3:
			UI_DrawTierMap(&rect, 2);
			break;
		case UI_PLAYERLOGO:
			UI_DrawPlayerLogo(&rect, color);
			break;
		case UI_PLAYERLOGO_METAL:
			UI_DrawPlayerLogoMetal(&rect, color);
			break;
		case UI_PLAYERLOGO_NAME:
			UI_DrawPlayerLogoName(&rect, color);
			break;
		case UI_OPPONENTLOGO:
			UI_DrawOpponentLogo(&rect, color);
			break;
		case UI_OPPONENTLOGO_METAL:
			UI_DrawOpponentLogoMetal(&rect, color);
			break;
		case UI_OPPONENTLOGO_NAME:
			UI_DrawOpponentLogoName(&rect, color);
			break;
		case UI_TIER_MAPNAME:
			UI_DrawTierMapName(&rect, scale, color, textStyle, iMenuFont);
			break;
		case UI_TIER_GAMETYPE:
			UI_DrawTierGameType(&rect, scale, color, textStyle, iMenuFont);
			break;
		case UI_ALLMAPS_SELECTION:
			UI_DrawAllMapsSelection(&rect, scale, color, textStyle, qtrue, iMenuFont);
			break;
		case UI_MAPS_SELECTION:
			UI_DrawAllMapsSelection(&rect, scale, color, textStyle, qfalse, iMenuFont);
			break;
		case UI_OPPONENT_NAME:
			UI_DrawOpponentName(&rect, scale, color, textStyle, iMenuFont);
			break;
		case UI_BOTNAME:
			UI_DrawBotName(&rect, scale, color, textStyle,iMenuFont);
			break;
		case UI_BOTSKILL:
			UI_DrawBotSkill(&rect, scale, color, textStyle,iMenuFont);
			break;
		case UI_REDBLUE:
			UI_DrawRedBlue(&rect, scale, color, textStyle,iMenuFont);
			break;
		case UI_CROSSHAIR:
			UI_DrawCrosshair(&rect, scale, color);
			break;
		case UI_SELECTEDPLAYER:
			UI_DrawSelectedPlayer(&rect, scale, color, textStyle, iMenuFont);
			break;
		case UI_SERVERREFRESHDATE:
			UI_DrawServerRefreshDate(&rect, scale, color, textStyle, iMenuFont);
			break;
		case UI_SERVERMOTD:
			UI_DrawServerMOTD(&rect, scale, color, iMenuFont);
			break;
		case UI_GLINFO:
			UI_DrawGLInfo(&rect,scale, color, textStyle, iMenuFont);
			break;
		case UI_KEYBINDSTATUS:
			UI_DrawKeyBindStatus(&rect,scale, color, textStyle,iMenuFont);
			break;
		case UI_VERSION:
			UI_Version(&rect, scale, color, iMenuFont);
			break;
    default:
      break;
  }

}

static qboolean UI_OwnerDrawVisible(int flags) {
	qboolean vis = qtrue;
	gametype_t gametype = trap_Cvar_VariableValue("g_gametype");

	while (flags) {

		if (flags & UI_SHOW_FFA) {
			if (gametype != GT_FFA && gametype != GT_HOLOCRON && gametype != GT_JEDIMASTER) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_FFA;
		}

		if (flags & UI_SHOW_NOTFFA) {
			if (gametype == GT_FFA || gametype == GT_HOLOCRON || gametype != GT_JEDIMASTER) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NOTFFA;
		}

		if (flags & UI_SHOW_LEADER) {
			// these need to show when this client can give orders to a player or a group
			if (!uiInfo.teamLeader) {
				vis = qfalse;
			} else {
				// if showing yourself
				if (ui_selectedPlayer.integer < uiInfo.myTeamCount && uiInfo.teamClientNums[ui_selectedPlayer.integer] == uiInfo.playerNumber) { 
					vis = qfalse;
				}
			}
			flags &= ~UI_SHOW_LEADER;
		} 
		if (flags & UI_SHOW_NOTLEADER) {
			// these need to show when this client is assigning their own status or they are NOT the leader
			if (uiInfo.teamLeader) {
				// if not showing yourself
				if (!(ui_selectedPlayer.integer < uiInfo.myTeamCount && uiInfo.teamClientNums[ui_selectedPlayer.integer] == uiInfo.playerNumber)) { 
					vis = qfalse;
				}
				// these need to show when this client can give orders to a player or a group
			}
			flags &= ~UI_SHOW_NOTLEADER;
		} 
		if (flags & UI_SHOW_FAVORITESERVERS) {
			// this assumes you only put this type of display flag on something showing in the proper context
			if (ui_netSource.integer != AS_FAVORITES) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_FAVORITESERVERS;
		} 
		if (flags & UI_SHOW_NOTFAVORITESERVERS) {
			// this assumes you only put this type of display flag on something showing in the proper context
			if (ui_netSource.integer == AS_FAVORITES) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NOTFAVORITESERVERS;
		} 
		if (flags & UI_SHOW_ANYTEAMGAME) {
			if (uiInfo.gameTypes[ui_gameType.integer].gtEnum <= GT_TEAM ) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_ANYTEAMGAME;
		} 
		if (flags & UI_SHOW_ANYNONTEAMGAME) {
			if (uiInfo.gameTypes[ui_gameType.integer].gtEnum > GT_TEAM ) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_ANYNONTEAMGAME;
		} 
		if (flags & UI_SHOW_NETANYTEAMGAME) {
			if (uiInfo.gameTypes[ui_netGameType.integer].gtEnum <= GT_TEAM ) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NETANYTEAMGAME;
		} 
		if (flags & UI_SHOW_NETANYNONTEAMGAME) {
			if (uiInfo.gameTypes[ui_netGameType.integer].gtEnum > GT_TEAM ) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NETANYNONTEAMGAME;
		} 
		if (flags & UI_SHOW_NEWHIGHSCORE) {
			if (uiInfo.newHighScoreTime < uiInfo.uiDC.realTime) {
				vis = qfalse;
			} else {
				if (uiInfo.soundHighScore) {
					if (trap_Cvar_VariableValue("sv_killserver") == 0) {
						// wait on server to go down before playing sound
						//trap_S_StartLocalSound(uiInfo.newHighScoreSound, CHAN_ANNOUNCER);
						uiInfo.soundHighScore = qfalse;
					}
				}
			}
			flags &= ~UI_SHOW_NEWHIGHSCORE;
		} 
		if (flags & UI_SHOW_NEWBESTTIME) {
			if (uiInfo.newBestTime < uiInfo.uiDC.realTime) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_NEWBESTTIME;
		} 
		if (flags & UI_SHOW_DEMOAVAILABLE) {
			if (!uiInfo.demoAvailable) {
				vis = qfalse;
			}
			flags &= ~UI_SHOW_DEMOAVAILABLE;
		} else {
			flags = 0;
		}
	}
  return vis;
}

static qboolean UI_Handicap_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
    int h;
    h = Com_Clamp( 5, 100, trap_Cvar_VariableValue("handicap") );
		if (key == A_MOUSE2) {
	    h -= 5;
		} else {
	    h += 5;
		}
    if (h > 100) {
      h = 5;
    } else if (h < 0) {
			h = 100;
		}
  	trap_Cvar_Set( "handicap", va( "%i", h) );
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_Effects_HandleKey(int flags, float *special, int key) {
	if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
		
		if ( !UI_TrueJediEnabled() )
		{
			int team = (int)(trap_Cvar_VariableValue("ui_myteam"));
					
			if (team == TEAM_RED || team==TEAM_BLUE)
			{
				return qfalse;
			}
		}
				
		if (key == A_MOUSE2) {
			uiInfo.effectsColor--;
		} else {
			uiInfo.effectsColor++;
		}
		
		if( uiInfo.effectsColor > 5 ) {
			uiInfo.effectsColor = 0;
		} else if (uiInfo.effectsColor < 0) {
			uiInfo.effectsColor = 5;
		}
		
		trap_Cvar_SetValue( "color1", /*uitogamecode[uiInfo.effectsColor]*/uiInfo.effectsColor );
		return qtrue;
	}
	return qfalse;
}

static qboolean UI_ClanName_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
		if (uiInfo.teamList[i].cinematic >= 0) {
		  trap_CIN_StopCinematic(uiInfo.teamList[i].cinematic);
			uiInfo.teamList[i].cinematic = -1;
		}
		if (key == A_MOUSE2) {
	    i--;
		} else {
	    i++;
		}
    if (i >= uiInfo.teamCount) {
      i = 0;
    } else if (i < 0) {
			i = uiInfo.teamCount - 1;
		}
  	trap_Cvar_Set( "ui_teamName", uiInfo.teamList[i].teamName);
	UI_HeadCountByTeam();
	UI_FeederSelection(FEEDER_HEADS, 0);
	updateModel = qtrue;
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_GameType_HandleKey(int flags, float *special, int key, qboolean resetMap) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
		int oldCount = UI_MapCountByGameType(qtrue);

		// hard coded mess here
		if (key == A_MOUSE2) {
			ui_gameType.integer--;
			if (ui_gameType.integer == 2) {
				ui_gameType.integer = 1;
			} else if (ui_gameType.integer < 2) {
				ui_gameType.integer = uiInfo.numGameTypes - 1;
			}
		} else {
			ui_gameType.integer++;
			if (ui_gameType.integer >= uiInfo.numGameTypes) {
				ui_gameType.integer = 1;
			} else if (ui_gameType.integer == 2) {
				ui_gameType.integer = 3;
			}
		}
    
		if (uiInfo.gameTypes[ui_gameType.integer].gtEnum == GT_TOURNAMENT) {
			trap_Cvar_Set("ui_Q3Model", "1");
		} else {
			trap_Cvar_Set("ui_Q3Model", "0");
		}

		trap_Cvar_Set("ui_gameType", va("%d", ui_gameType.integer));
		UI_SetCapFragLimits(qtrue);
		UI_LoadBestScores(uiInfo.mapList[ui_currentMap.integer].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum);
		if (resetMap && oldCount != UI_MapCountByGameType(qtrue)) {
	  	trap_Cvar_Set( "ui_currentMap", "0");
			Menu_SetFeederSelection(NULL, FEEDER_MAPS, 0, NULL);
		}
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_NetGameType_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {

		if (key == A_MOUSE2) {
			ui_netGameType.integer--;
		} else {
			ui_netGameType.integer++;
		}

    if (ui_netGameType.integer < 0) {
      ui_netGameType.integer = uiInfo.numGameTypes - 1;
		} else if (ui_netGameType.integer >= uiInfo.numGameTypes) {
      ui_netGameType.integer = 0;
    } 

  	trap_Cvar_Set( "ui_netGameType", va("%d", ui_netGameType.integer));
  	trap_Cvar_Set( "ui_actualnetGameType", va("%d", uiInfo.gameTypes[ui_netGameType.integer].gtEnum));
  	trap_Cvar_Set( "ui_currentNetMap", "0");
		UI_MapCountByGameType(qfalse);
		Menu_SetFeederSelection(NULL, FEEDER_ALLMAPS, 0, NULL);
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_AutoSwitch_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
	 int switchVal = trap_Cvar_VariableValue("cg_autoswitch");

		if (key == A_MOUSE2) {
			switchVal--;
		} else {
			switchVal++;
		}

    if (switchVal < 0)
	{
		switchVal = 2;
	}
	else if (switchVal >= 3)
	{
      switchVal = 0;
    } 

  	trap_Cvar_Set( "cg_autoswitch", va("%i", switchVal));
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_JoinGameType_HandleKey(int flags, float *special, int key) {
	if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {

		if (key == A_MOUSE2) {
			ui_joinGameType.integer--;
		} else {
			ui_joinGameType.integer++;
		}

		if (ui_joinGameType.integer < 0) {
			ui_joinGameType.integer = uiInfo.numJoinGameTypes - 1;
		} else if (ui_joinGameType.integer >= uiInfo.numJoinGameTypes) {
			ui_joinGameType.integer = 0;
		}

		trap_Cvar_Set( "ui_joinGameType", va("%d", ui_joinGameType.integer));
		UI_BuildServerDisplayList(1);
		return qtrue;
	}
	return qfalse;
}



static qboolean UI_Skill_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
  	int i = trap_Cvar_VariableValue( "g_spSkill" );

		if (key == A_MOUSE2) {
	    i--;
		} else {
	    i++;
		}

    if (i < 1) {
			i = numSkillLevels;
		} else if (i > numSkillLevels) {
      i = 1;
    }

    trap_Cvar_Set("g_spSkill", va("%i", i));
    return qtrue;
  }
  return qfalse;
}


static qboolean UI_TeamName_HandleKey(int flags, float *special, int key, qboolean blue) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueTeam" : "ui_redTeam"));

		if (key == A_MOUSE2) {
	    i--;
		} else {
	    i++;
		}

    if (i >= uiInfo.teamCount) {
      i = 0;
    } else if (i < 0) {
			i = uiInfo.teamCount - 1;
		}

    trap_Cvar_Set( (blue) ? "ui_blueTeam" : "ui_redTeam", uiInfo.teamList[i].teamName);

    return qtrue;
  }
  return qfalse;
}

static qboolean UI_TeamMember_HandleKey(int flags, float *special, int key, qboolean blue, int num) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
		// 0 - None
		// 1 - Human
		// 2..NumCharacters - Bot
		char *cvar = va(blue ? "ui_blueteam%i" : "ui_redteam%i", num);
		int value = trap_Cvar_VariableValue(cvar);
		int maxcl = trap_Cvar_VariableValue( "sv_maxClients" );
		int numval = num;

		numval *= 2;

		if (blue)
		{
			numval -= 1;
		}

		if (numval > maxcl)
		{
			return qfalse;
		}

		if (value < 1)
		{
			value = 1;
		}

		if (key == A_MOUSE2) {
			value--;
		} else {
			value++;
		}

		/*if (ui_actualNetGameType.integer >= GT_TEAM) {
			if (value >= uiInfo.characterCount + 2) {
				value = 0;
			} else if (value < 0) {
				value = uiInfo.characterCount + 2 - 1;
			}
		} else {*/
			if (value >= UI_GetNumBots() + 2) {
				value = 1;
			} else if (value < 1) {
				value = UI_GetNumBots() + 2 - 1;
			}
		//}

		trap_Cvar_Set(cvar, va("%i", value));
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_NetSource_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
		
		if (key == A_MOUSE2) {
			ui_netSource.integer--;
		} else {
			ui_netSource.integer++;
		}
    
		if (ui_netSource.integer >= numNetSources) {
      ui_netSource.integer = 0;
    } else if (ui_netSource.integer < 0) {
      ui_netSource.integer = numNetSources - 1;
		}

		UI_BuildServerDisplayList(1);
		if (ui_netSource.integer != AS_GLOBAL) {
			UI_StartServerRefresh(qtrue);
		}
  	trap_Cvar_Set( "ui_netSource", va("%d", ui_netSource.integer));
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_NetFilter_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {

		if (key == A_MOUSE2) {
			ui_serverFilterType.integer--;
		} else {
			ui_serverFilterType.integer++;
		}

    if (ui_serverFilterType.integer >= numServerFilters) {
      ui_serverFilterType.integer = 0;
    } else if (ui_serverFilterType.integer < 0) {
      ui_serverFilterType.integer = numServerFilters - 1;
		}

		trap_Cvar_Set("ui_serverFilterType", va("%i", ui_serverFilterType.integer));
		UI_BuildServerDisplayList(1);
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_OpponentName_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
		if (key == A_MOUSE2) {
			UI_PriorOpponent();
		} else {
			UI_NextOpponent();
		}
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_BotName_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
//		int game = trap_Cvar_VariableValue("g_gametype");
		int value = uiInfo.botIndex;

		if (key == A_MOUSE2) {
			value--;
		} else {
			value++;
		}

		/*
		if (game >= GT_TEAM) {
			if (value >= uiInfo.characterCount + 2) {
				value = 0;
			} else if (value < 0) {
				value = uiInfo.characterCount + 2 - 1;
			}
		} else {
		*/
			if (value >= UI_GetNumBots()/* + 2*/) {
				value = 0;
			} else if (value < 0) {
				value = UI_GetNumBots()/* + 2*/ - 1;
			}
		//}
		uiInfo.botIndex = value;
    return qtrue;
  }
  return qfalse;
}

static qboolean UI_BotSkill_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
		if (key == A_MOUSE2) {
			uiInfo.skillIndex--;
		} else {
			uiInfo.skillIndex++;
		}
		if (uiInfo.skillIndex >= numSkillLevels) {
			uiInfo.skillIndex = 0;
		} else if (uiInfo.skillIndex < 0) {
			uiInfo.skillIndex = numSkillLevels-1;
		}
    return qtrue;
  }
	return qfalse;
}

static qboolean UI_RedBlue_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
		uiInfo.redBlue ^= 1;
		return qtrue;
	}
	return qfalse;
}

static qboolean UI_Crosshair_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
		if (key == A_MOUSE2) {
			uiInfo.currentCrosshair--;
		} else {
			uiInfo.currentCrosshair++;
		}

		if (uiInfo.currentCrosshair >= NUM_CROSSHAIRS) {
			uiInfo.currentCrosshair = 0;
		} else if (uiInfo.currentCrosshair < 0) {
			uiInfo.currentCrosshair = NUM_CROSSHAIRS - 1;
		}
		trap_Cvar_Set("cg_drawCrosshair", va("%d", uiInfo.currentCrosshair)); 
		return qtrue;
	}
	return qfalse;
}



static qboolean UI_SelectedPlayer_HandleKey(int flags, float *special, int key) {
  if (key == A_MOUSE1 || key == A_MOUSE2 || key == A_ENTER || key == A_KP_ENTER) {
		int selected;

		UI_BuildPlayerList();
		if (!uiInfo.teamLeader) {
			return qfalse;
		}
		selected = trap_Cvar_VariableValue("cg_selectedPlayer");
		
		if (key == A_MOUSE2) {
			selected--;
		} else {
			selected++;
		}

		if (selected > uiInfo.myTeamCount) {
			selected = 0;
		} else if (selected < 0) {
			selected = uiInfo.myTeamCount;
		}

		if (selected == uiInfo.myTeamCount) {
		 	trap_Cvar_Set( "cg_selectedPlayerName", "Everyone");
		} else {
		 	trap_Cvar_Set( "cg_selectedPlayerName", uiInfo.teamNames[selected]);
		}
	 	trap_Cvar_Set( "cg_selectedPlayer", va("%d", selected));
	}
	return qfalse;
}


static qboolean UI_OwnerDrawHandleKey(int ownerDraw, int flags, float *special, int key) {
	int findex, iUse = 0;

  switch (ownerDraw) {
    case UI_HANDICAP:
      return UI_Handicap_HandleKey(flags, special, key);
      break;
    case UI_SKIN_COLOR:
      return UI_SkinColor_HandleKey(flags, special, key, uiSkinColor, TEAM_FREE, TEAM_BLUE, ownerDraw);
      break;
    case UI_FORCE_SIDE:
      return UI_ForceSide_HandleKey(flags, special, key, uiForceSide, 1, 2, ownerDraw);
      break;
    case UI_JEDI_NONJEDI:
      return UI_JediNonJedi_HandleKey(flags, special, key, uiJediNonJedi, 0, 1, ownerDraw);
      break;
	case UI_FORCE_MASTERY_SET:
      return UI_ForceMaxRank_HandleKey(flags, special, key, uiForceRank, 1, MAX_FORCE_RANK, ownerDraw);
      break;
    case UI_FORCE_RANK:
		break;		
	case UI_FORCE_RANK_HEAL:
	case UI_FORCE_RANK_LEVITATION:
	case UI_FORCE_RANK_SPEED:
	case UI_FORCE_RANK_PUSH:
	case UI_FORCE_RANK_PULL:
	case UI_FORCE_RANK_TELEPATHY:
	case UI_FORCE_RANK_GRIP:
	case UI_FORCE_RANK_LIGHTNING:
	case UI_FORCE_RANK_RAGE:
	case UI_FORCE_RANK_PROTECT:
	case UI_FORCE_RANK_ABSORB:
	case UI_FORCE_RANK_TEAM_HEAL:
	case UI_FORCE_RANK_TEAM_FORCE:
	case UI_FORCE_RANK_DRAIN:
	case UI_FORCE_RANK_SEE:
	case UI_FORCE_RANK_SABERATTACK:
	case UI_FORCE_RANK_SABERDEFEND:
	case UI_FORCE_RANK_SABERTHROW:
		findex = (ownerDraw - UI_FORCE_RANK)-1;
		//this will give us the index as long as UI_FORCE_RANK is always one below the first force rank index
		return UI_ForcePowerRank_HandleKey(flags, special, key, uiForcePowersRank[findex], 0, NUM_FORCE_POWER_LEVELS-1, ownerDraw);
		break;
    case UI_EFFECTS:
      return UI_Effects_HandleKey(flags, special, key);
      break;
    case UI_CLANNAME:
      return UI_ClanName_HandleKey(flags, special, key);
      break;
    case UI_GAMETYPE:
      return UI_GameType_HandleKey(flags, special, key, qtrue);
      break;
    case UI_NETGAMETYPE:
      return UI_NetGameType_HandleKey(flags, special, key);
      break;
    case UI_AUTOSWITCHLIST:
      return UI_AutoSwitch_HandleKey(flags, special, key);
      break;
    case UI_JOINGAMETYPE:
      return UI_JoinGameType_HandleKey(flags, special, key);
      break;
    case UI_SKILL:
      return UI_Skill_HandleKey(flags, special, key);
      break;
    case UI_BLUETEAMNAME:
      return UI_TeamName_HandleKey(flags, special, key, qtrue);
      break;
    case UI_REDTEAMNAME:
      return UI_TeamName_HandleKey(flags, special, key, qfalse);
      break;
    case UI_BLUETEAM1:
		case UI_BLUETEAM2:
		case UI_BLUETEAM3:
		case UI_BLUETEAM4:
		case UI_BLUETEAM5:
		case UI_BLUETEAM6:
		case UI_BLUETEAM7:
		case UI_BLUETEAM8:
	if (ownerDraw <= UI_BLUETEAM5)
	{
	  iUse = ownerDraw-UI_BLUETEAM1 + 1;
	}
	else
	{
	  iUse = ownerDraw-274; //unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
	}

      UI_TeamMember_HandleKey(flags, special, key, qtrue, iUse);
      break;
    case UI_REDTEAM1:
		case UI_REDTEAM2:
		case UI_REDTEAM3:
		case UI_REDTEAM4:
		case UI_REDTEAM5:
		case UI_REDTEAM6:
		case UI_REDTEAM7:
		case UI_REDTEAM8:
	if (ownerDraw <= UI_REDTEAM5)
	{
	  iUse = ownerDraw-UI_REDTEAM1 + 1;
	}
	else
	{
	  iUse = ownerDraw-277; //unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
	}
      UI_TeamMember_HandleKey(flags, special, key, qfalse, iUse);
      break;
		case UI_NETSOURCE:
      UI_NetSource_HandleKey(flags, special, key);
			break;
		case UI_NETFILTER:
      UI_NetFilter_HandleKey(flags, special, key);
			break;
		case UI_OPPONENT_NAME:
			UI_OpponentName_HandleKey(flags, special, key);
			break;
		case UI_BOTNAME:
			return UI_BotName_HandleKey(flags, special, key);
			break;
		case UI_BOTSKILL:
			return UI_BotSkill_HandleKey(flags, special, key);
			break;
		case UI_REDBLUE:
			UI_RedBlue_HandleKey(flags, special, key);
			break;
		case UI_CROSSHAIR:
			UI_Crosshair_HandleKey(flags, special, key);
			break;
		case UI_SELECTEDPLAYER:
			UI_SelectedPlayer_HandleKey(flags, special, key);
			break;
    default:
      break;
  }

  return qfalse;
}


static float UI_GetValue(int ownerDraw) {
  return 0;
}

/*
=================
UI_ServersQsortCompare
=================
*/
static int QDECL UI_ServersQsortCompare( const void *arg1, const void *arg2 ) {
	int fakeKey = uiInfo.serverStatus.sortKey;

	// fake over to new sorting without bots
	if (fakeKey == SORT_CLIENTS && trap_Cvar_VariableValue("ui_botfilter")) {
		fakeKey = MVSORT_CLIENTS_NOBOTS;
	}

	return trap_LAN_CompareServers(ui_netSource.integer, fakeKey, uiInfo.serverStatus.sortDir, *(const int*)arg1, *(const int*)arg2);
}


/*
=================
UI_ServersSort
=================
*/
void UI_ServersSort(int column, qboolean force) {

	if ( !force ) {
		if ( uiInfo.serverStatus.sortKey == column ) {
			return;
		}
	}

	uiInfo.serverStatus.sortKey = column;
	qsort( &uiInfo.serverStatus.displayServers[0], uiInfo.serverStatus.numDisplayServers, sizeof(int), UI_ServersQsortCompare);
}

/*
static void UI_StartSinglePlayer() {
	int i,j, k, skill;
	char buff[1024];
	i = trap_Cvar_VariableValue( "ui_currentTier" );
  if (i < 0 || i >= tierCount) {
    i = 0;
  }
	j = trap_Cvar_VariableValue("ui_currentMap");
	if (j < 0 || j > MAPS_PER_TIER) {
		j = 0;
	}

 	trap_Cvar_SetValue( "singleplayer", 1 );
 	trap_Cvar_SetValue( "g_gametype", Com_Clamp( 0, 7, tierList[i].gameTypes[j] ) );
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", tierList[i].maps[j] ) );
	skill = trap_Cvar_VariableValue( "g_spSkill" );

	if (j == MAPS_PER_TIER-1) {
		k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
		Com_sprintf( buff, sizeof(buff), "wait ; addbot %s %i %s 250 %s\n", UI_AIFromName(teamList[k].teamMembers[0]), skill, "", teamList[k].teamMembers[0]);
	} else {
		k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
		for (i = 0; i < PLAYERS_PER_TEAM; i++) {
			Com_sprintf( buff, sizeof(buff), "wait ; addbot %s %i %s 250 %s\n", UI_AIFromName(teamList[k].teamMembers[i]), skill, "Blue", teamList[k].teamMembers[i]);
			trap_Cmd_ExecuteText( EXEC_APPEND, buff );
		}

		k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
		for (i = 1; i < PLAYERS_PER_TEAM; i++) {
			Com_sprintf( buff, sizeof(buff), "wait ; addbot %s %i %s 250 %s\n", UI_AIFromName(teamList[k].teamMembers[i]), skill, "Red", teamList[k].teamMembers[i]);
			trap_Cmd_ExecuteText( EXEC_APPEND, buff );
		}
		trap_Cmd_ExecuteText( EXEC_APPEND, "wait 5; team Red\n" );
	}
	

}
*/

/*
===============
UI_LoadMods
===============
*/
static void UI_LoadMods() {
	int		numdirs;
	char	dirlist[2048];
	char	*dirptr;
  char  *descptr;
	int		i;
	size_t		dirlen;

	uiInfo.modCount = 0;
	numdirs = trap_FS_GetFileList( "$modlist", "", dirlist, sizeof(dirlist) );
	dirptr  = dirlist;
	for( i = 0; i < numdirs; i++ ) {
		dirlen = strlen( dirptr ) + 1;
    descptr = dirptr + dirlen;
		uiInfo.modList[uiInfo.modCount].modName = String_Alloc(dirptr);
		uiInfo.modList[uiInfo.modCount].modDescr = String_Alloc(descptr);
    dirptr += dirlen + strlen(descptr) + 1;
		uiInfo.modCount++;
		if (uiInfo.modCount >= MAX_MODS) {
			break;
		}
	}

}


/*
===============
UI_LoadMovies
===============
*/
static void UI_LoadMovies() {
	char	movielist[4096];
	char	*moviename;
	int		i;
	size_t	len;

	uiInfo.movieCount = trap_FS_GetFileList( "video", "roq", movielist, 4096 );

	if (uiInfo.movieCount) {
		if (uiInfo.movieCount > MAX_MOVIES) {
			uiInfo.movieCount = MAX_MOVIES;
		}
		moviename = movielist;
		for ( i = 0; i < uiInfo.movieCount; i++ ) {
			len = strlen( moviename );
			if (!Q_stricmp(moviename +  len - 4,".roq")) {
				moviename[len-4] = '\0';
			}
			Q_strupr(moviename);
			uiInfo.movieList[i] = String_Alloc(moviename);
			moviename += len + 1;
		}
	}

}

#ifdef JK2MV_MENU
int demosort(void const *a, void const *b) {
	char const *aa = (char const *)a;
	char const *bb = (char const *)b;

	return strcmp(aa, bb);
}
#endif

/*
===============
UI_LoadDemos
===============
*/
static void UI_LoadDemos() {
	char	demolist[4096];
	char demoExt[32];
	char	*demoname;
	int		i;
	size_t len;

	// Load "dm_15" and "dm_16" demos.
	mvprotocol_t	protocol;
	int		oldCount = 0;
	uiInfo.demoCount = 0;

	for ( protocol = PROTOCOL15; protocol <= PROTOCOL16; protocol++ )
	{
#ifndef JK2MV_MENU
		if ( (protocol == PROTOCOL15 && jk2startversion != VERSION_1_02 && jk2startversion != VERSION_1_03)
			|| (protocol == PROTOCOL16 && jk2startversion != VERSION_1_04)
			|| jk2startversion == VERSION_UNDEF )
		{
			continue;
		}
#endif
		Com_sprintf(demoExt, sizeof(demoExt), "dm_%d", (int)protocol);

		uiInfo.demoCount += trap_FS_GetFileList( "demos", demoExt, demolist, 4096 );

		if (uiInfo.demoCount - oldCount) {
			if (uiInfo.demoCount > MAX_DEMOS) {
				uiInfo.demoCount = MAX_DEMOS;
			}
			demoname = demolist;
			for ( i = oldCount; i < uiInfo.demoCount; i++ ) {
				len = strlen( demoname );
#ifndef JK2MV_MENU
				if (!Q_stricmp(demoname +  len - strlen(demoExt), demoExt)) {
					demoname[len-strlen(demoExt)] = '\0';
				}
				Q_strupr(demoname);
#endif
				uiInfo.demoList[i] = String_Alloc(demoname);
				demoname += len + 1;
			}
		}
		oldCount = uiInfo.demoCount;
	}

#ifdef JK2MV_MENU
	//Sort demos by name.
	qsort((void *)uiInfo.demoList, uiInfo.demoCount, sizeof(uiInfo.demoList[0]), demosort);
#endif
}

/*
===============
UI_LoadDLFiles
===============
*/
static void UI_LoadDLFiles() {
	int i;

	uiInfo.downloadsCount = trap_FS_GetDLList(uiInfo.downloadsList, MAX_DOWNLOADS);

	// red font for blacklisted entrys
	for (i = 0; i < uiInfo.downloadsCount; i++) {
		if (uiInfo.downloadsList[i].blacklisted) {
			char tmp[256];

			Com_sprintf(tmp, sizeof(tmp), S_COLOR_RED "%s", uiInfo.downloadsList[i].name);
			Q_strncpyz(uiInfo.downloadsList[i].name, tmp, sizeof(uiInfo.downloadsList[i].name));
		}
	}
}


static qboolean UI_SetNextMap(int actual, int index) {
	int i;
	for (i = actual + 1; i < uiInfo.mapCount; i++) {
		if (uiInfo.mapList[i].active) {
			Menu_SetFeederSelection(NULL, FEEDER_MAPS, index + 1, "skirmish");
			return qtrue;
		}
	}
	return qfalse;
}


static void UI_StartSkirmish(qboolean next) {
	int i, k, g, delay, temp;
	float skill;
	char buff[MAX_STRING_CHARS];

	temp = trap_Cvar_VariableValue( "g_gametype" );
	trap_Cvar_Set("ui_gameType", va("%i", temp));

	if (next) {
		int actual;
		int index = trap_Cvar_VariableValue("ui_mapIndex");
	 	UI_MapCountByGameType(qtrue);
		UI_SelectedMap(index, &actual);
		if (UI_SetNextMap(actual, index)) {
		} else {
			UI_GameType_HandleKey(0, 0, A_MOUSE1, qfalse);
			UI_MapCountByGameType(qtrue);
			Menu_SetFeederSelection(NULL, FEEDER_MAPS, 0, "skirmish");
		}
	}

	g = uiInfo.gameTypes[ui_gameType.integer].gtEnum;
	trap_Cvar_SetValue( "g_gametype", g );
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", uiInfo.mapList[ui_currentMap.integer].mapLoadName) );
	skill = trap_Cvar_VariableValue( "g_spSkill" );
	trap_Cvar_Set("ui_scoreMap", uiInfo.mapList[ui_currentMap.integer].mapName);

	k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));

	trap_Cvar_Set("ui_singlePlayerActive", "1");

	// set up sp overrides, will be replaced on postgame
	temp = trap_Cvar_VariableValue( "capturelimit" );
	trap_Cvar_Set("ui_saveCaptureLimit", va("%i", temp));
	temp = trap_Cvar_VariableValue( "fraglimit" );
	trap_Cvar_Set("ui_saveFragLimit", va("%i", temp));
	temp = trap_Cvar_VariableValue( "duel_fraglimit" );
	trap_Cvar_Set("ui_saveDuelLimit", va("%i", temp));

	UI_SetCapFragLimits(qfalse);

	temp = trap_Cvar_VariableValue( "cg_drawTimer" );
	trap_Cvar_Set("ui_drawTimer", va("%i", temp));
	temp = trap_Cvar_VariableValue( "g_doWarmup" );
	trap_Cvar_Set("ui_doWarmup", va("%i", temp));
	temp = trap_Cvar_VariableValue( "g_friendlyFire" );
	trap_Cvar_Set("ui_friendlyFire", va("%i", temp));
	temp = trap_Cvar_VariableValue( "sv_maxClients" );
	trap_Cvar_Set("ui_maxClients", va("%i", temp));
	temp = trap_Cvar_VariableValue( "g_warmup" );
	trap_Cvar_Set("ui_Warmup", va("%i", temp));
	temp = trap_Cvar_VariableValue( "sv_pure" );
	trap_Cvar_Set("ui_pure", va("%i", temp));

	trap_Cvar_Set("cg_cameraOrbit", "0");
	trap_Cvar_Set("cg_thirdPerson", "0");
	trap_Cvar_Set("cg_drawTimer", "1");
	trap_Cvar_Set("g_doWarmup", "1");
	trap_Cvar_Set("g_warmup", "15");
	trap_Cvar_Set("sv_pure", "0");
	trap_Cvar_Set("g_friendlyFire", "0");
	trap_Cvar_Set("g_redTeam", UI_Cvar_VariableString("ui_teamName"));
	trap_Cvar_Set("g_blueTeam", UI_Cvar_VariableString("ui_opponentName"));

	if (trap_Cvar_VariableValue("ui_recordSPDemo")) {
		Com_sprintf(buff, MAX_STRING_CHARS, "%s_%i", uiInfo.mapList[ui_currentMap.integer].mapLoadName, g);
		trap_Cvar_Set("ui_recordSPDemoName", buff);
	}

	delay = 500;

	if (g == GT_TOURNAMENT) {
		temp = uiInfo.mapList[ui_currentMap.integer].teamMembers * 2;
		trap_Cvar_Set("sv_maxClients", va("%d", temp));
		Com_sprintf( buff, sizeof(buff), "wait ; addbot %s %f "", %i \n", uiInfo.mapList[ui_currentMap.integer].opponentName, skill, delay);
		trap_Cmd_ExecuteText( EXEC_APPEND, buff );
	} else if (g == GT_HOLOCRON || g == GT_JEDIMASTER) {
		temp = uiInfo.mapList[ui_currentMap.integer].teamMembers * 2;
		trap_Cvar_Set("sv_maxClients", va("%d", temp));
		for (i =0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers; i++) {
			Com_sprintf( buff, sizeof(buff), "addbot %s %f %s %i %s\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == GT_HOLOCRON) ? "" : "Blue", delay, uiInfo.teamList[k].teamMembers[i]);
			trap_Cmd_ExecuteText( EXEC_APPEND, buff );
			delay += 500;
		}
		k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
		for (i =0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers-1; i++) {
			Com_sprintf( buff, sizeof(buff), "addbot %s %f %s %i %s\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == GT_HOLOCRON) ? "" : "Red", delay, uiInfo.teamList[k].teamMembers[i]);
			trap_Cmd_ExecuteText( EXEC_APPEND, buff );
			delay += 500;
		}
	} else {
		temp = uiInfo.mapList[ui_currentMap.integer].teamMembers * 2;
		trap_Cvar_Set("sv_maxClients", va("%d", temp));
		for (i =0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers; i++) {
			Com_sprintf( buff, sizeof(buff), "addbot %s %f %s %i %s\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == GT_FFA) ? "" : "Blue", delay, uiInfo.teamList[k].teamMembers[i]);
			trap_Cmd_ExecuteText( EXEC_APPEND, buff );
			delay += 500;
		}
		k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
		for (i =0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers-1; i++) {
			Com_sprintf( buff, sizeof(buff), "addbot %s %f %s %i %s\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == GT_FFA) ? "" : "Red", delay, uiInfo.teamList[k].teamMembers[i]);
			trap_Cmd_ExecuteText( EXEC_APPEND, buff );
			delay += 500;
		}
	}
	if (g >= GT_TEAM ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "wait 5; team Red\n" );
	}
}

static void UI_Update(const char *name) {
	int	val = trap_Cvar_VariableValue(name);

	if (Q_stricmp(name, "s_khz") == 0) 
	{
		trap_Cmd_ExecuteText( EXEC_APPEND, "snd_restart\n" );
		return;
	}

 	if (Q_stricmp(name, "ui_SetName") == 0) {
		trap_Cvar_Set( "name", UI_Cvar_VariableString("ui_Name"));
 	} else if (Q_stricmp(name, "ui_setRate") == 0) {
		float rate = trap_Cvar_VariableValue("rate");
		if (rate >= 5000) {
			trap_Cvar_Set("cl_maxpackets", "30");
			trap_Cvar_Set("cl_packetdup", "1");
		} else if (rate >= 4000) {
			trap_Cvar_Set("cl_maxpackets", "15");
			trap_Cvar_Set("cl_packetdup", "2");		// favor less prediction errors when there's packet loss
		} else {
			trap_Cvar_Set("cl_maxpackets", "15");
			trap_Cvar_Set("cl_packetdup", "1");		// favor lower bandwidth
		}
 	} 
	else if (Q_stricmp(name, "ui_GetName") == 0) 
	{
		trap_Cvar_Set( "ui_Name", UI_Cvar_VariableString("name"));
	}
	else if (Q_stricmp(name, "ui_r_colorbits") == 0) 
	{
		switch (val) 
		{
			case 0:
				trap_Cvar_SetValue( "ui_r_depthbits", 0 );
				break;

			case 16:
				trap_Cvar_SetValue( "ui_r_depthbits", 16 );
				break;

			case 32:
				trap_Cvar_SetValue( "ui_r_depthbits", 24 );
				break;
		}
	} 
	else if (Q_stricmp(name, "ui_r_lodbias") == 0) 
	{
		switch (val) 
		{
			case 0:
				trap_Cvar_SetValue( "ui_r_subdivisions", 4 );
				break;
			case 1:
				trap_Cvar_SetValue( "ui_r_subdivisions", 12 );
				break;

			case 2:
				trap_Cvar_SetValue( "ui_r_subdivisions", 20 );
				break;
		}
	} 
	else if (Q_stricmp(name, "ui_r_glCustom") == 0) 
	{
		switch (val) 
		{
			case 0:	// high quality

				trap_Cvar_SetValue( "ui_r_fullScreen", 1 );
				trap_Cvar_SetValue( "ui_r_subdivisions", 4 );
				trap_Cvar_SetValue( "ui_r_lodbias", 0 );
				trap_Cvar_SetValue( "ui_r_colorbits", 32 );
				trap_Cvar_SetValue( "ui_r_depthbits", 24 );
				trap_Cvar_SetValue( "ui_r_picmip", 0 );
				if ( mvapi )
				{
					trap_Cvar_SetValue( "ui_r_aspectratio", -1 );
					trap_Cvar_SetValue( "ui_r_mode", -2 );
				}
				else
				{
					trap_Cvar_SetValue( "ui_r_mode", 4 );
				}
				trap_Cvar_SetValue( "ui_r_texturebits", 32 );
				trap_Cvar_SetValue( "ui_r_fastSky", 0 );
				trap_Cvar_SetValue( "ui_r_inGameVideo", 1 );
				//trap_Cvar_SetValue( "ui_cg_shadows", 2 );//stencil
				trap_Cvar_Set( "ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
				break;

			case 1: // normal 
				trap_Cvar_SetValue( "ui_r_fullScreen", 1 );
				trap_Cvar_SetValue( "ui_r_subdivisions", 4 );
				trap_Cvar_SetValue( "ui_r_lodbias", 0 );
				trap_Cvar_SetValue( "ui_r_colorbits", 0 );
				trap_Cvar_SetValue( "ui_r_depthbits", 24 );
				trap_Cvar_SetValue( "ui_r_picmip", 1 );
				if ( mvapi )
				{
					trap_Cvar_SetValue( "ui_r_aspectratio", -1 );
					trap_Cvar_SetValue( "ui_r_mode", -2 );
				}
				else
				{
					trap_Cvar_SetValue( "ui_r_mode", 3 );
				}
				trap_Cvar_SetValue( "ui_r_texturebits", 0 );
				trap_Cvar_SetValue( "ui_r_fastSky", 0 );
				trap_Cvar_SetValue( "ui_r_inGameVideo", 1 );
				//trap_Cvar_SetValue( "ui_cg_shadows", 2 );
				trap_Cvar_Set( "ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
				break;

			case 2: // fast

				trap_Cvar_SetValue( "ui_r_fullScreen", 1 );
				trap_Cvar_SetValue( "ui_r_subdivisions", 12 );
				trap_Cvar_SetValue( "ui_r_lodbias", 1 );
				trap_Cvar_SetValue( "ui_r_colorbits", 0 );
				trap_Cvar_SetValue( "ui_r_depthbits", 0 );
				trap_Cvar_SetValue( "ui_r_picmip", 2 );
				if ( mvapi )
				{
					trap_Cvar_SetValue( "ui_r_aspectratio", -1 );
					trap_Cvar_SetValue( "ui_r_mode", -2 );
				}
				else
				{
					trap_Cvar_SetValue( "ui_r_mode", 3 );
				}
				trap_Cvar_SetValue( "ui_r_texturebits", 0 );
				trap_Cvar_SetValue( "ui_r_fastSky", 1 );
				trap_Cvar_SetValue( "ui_r_inGameVideo", 0 );
				//trap_Cvar_SetValue( "ui_cg_shadows", 1 );
				trap_Cvar_Set( "ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST" );
				break;

			case 3: // fastest

				trap_Cvar_SetValue( "ui_r_fullScreen", 1 );
				trap_Cvar_SetValue( "ui_r_subdivisions", 20 );
				trap_Cvar_SetValue( "ui_r_lodbias", 2 );
				trap_Cvar_SetValue( "ui_r_colorbits", 16 );
				trap_Cvar_SetValue( "ui_r_depthbits", 16 );
				if ( mvapi )
				{
					trap_Cvar_SetValue( "ui_r_aspectratio", -1 );
					trap_Cvar_SetValue( "ui_r_mode", -2 );
				}
				else
				{
					trap_Cvar_SetValue( "ui_r_mode", 3 );
				}
				trap_Cvar_SetValue( "ui_r_picmip", 3 );
				trap_Cvar_SetValue( "ui_r_texturebits", 16 );
				trap_Cvar_SetValue( "ui_r_fastSky", 1 );
				trap_Cvar_SetValue( "ui_r_inGameVideo", 0 );
				//trap_Cvar_SetValue( "ui_cg_shadows", 0 );
				trap_Cvar_Set( "ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST" );
			break;
		}
	} 
	else if (Q_stricmp(name, "ui_mousePitch") == 0) 
	{
		if (val == 0) 
		{
			trap_Cvar_SetValue( "m_pitch", 0.022f );
		} 
		else 
		{
			trap_Cvar_SetValue( "m_pitch", -0.022f );
		}
	}
	// screen resolutions
	else if (!Q_stricmp(name, "ui_r_aspectratio")) {
		int ui_r_mode = (int)trap_Cvar_VariableValue("ui_r_mode");

		if (val == 0 && (ui_r_mode < 0 || ui_r_mode >= 11)) {
			trap_Cvar_SetValue("ui_r_mode", 0);
		} else if (val == 1 && (ui_r_mode < 11 || ui_r_mode >= 27)) {
			trap_Cvar_SetValue("ui_r_mode", 11);
		} else if (val == 2 && (ui_r_mode < 27 || ui_r_mode >= 32)) {
			trap_Cvar_SetValue("ui_r_mode", 27);
		} else if (val == -1) {
			trap_Cvar_SetValue("ui_r_mode", -2);
			trap_Cvar_SetValue("ui_r_fullscreen", 1);
		}
	}
}

static int gUISelectedMap = 0;

/*
===============
UI_DeferMenuScript

Return true if the menu script should be deferred for later
===============
*/
static qboolean UI_DeferMenuScript ( const char **args )
{
	const char* name;

	// Whats the reason for being deferred?
	if (!String_Parse( args, &name))
	{
		return qfalse;
	}

	// Handle the custom cases
	if ( !Q_stricmp ( name, "VideoSetup" ) )
	{
		const char* warningMenuName;
		qboolean	deferred;

		// No warning menu specified
		if ( !String_Parse( args, &warningMenuName) )
		{
			return qfalse;
		}

		// Defer if the video options were modified
		deferred = trap_Cvar_VariableValue ( "ui_r_modified" ) ? qtrue : qfalse;

		if ( deferred )
		{
			// Open the warning menu
			Menus_OpenByName(warningMenuName);
		}

		return deferred;
	}
	else if ( !Q_stricmp ( name, "RulesBackout" ) )
	{
		qboolean deferred;
		
		deferred = trap_Cvar_VariableValue ( "ui_rules_backout" ) ? qtrue : qfalse ;

		trap_Cvar_Set ( "ui_rules_backout", "0" );

		return deferred;
	}

	return qfalse;
}

/*
=================
UI_UpdateVideoSetup

Copies the temporary user interface version of the video cvars into
their real counterparts.  This is to create a interface which allows 
you to discard your changes if you did something you didnt want
=================
*/
void UI_UpdateVideoSetup ( void )
{
	trap_Cvar_Set ( "r_mode", UI_Cvar_VariableString ( "ui_r_mode" ) );
	trap_Cvar_Set ( "r_aspectratio", UI_Cvar_VariableString("ui_r_aspectratio"));
	trap_Cvar_Set ( "r_fullscreen", UI_Cvar_VariableString ( "ui_r_fullscreen" ) );
	trap_Cvar_Set ( "r_colorbits", UI_Cvar_VariableString ( "ui_r_colorbits" ) );
	trap_Cvar_Set ( "r_lodbias", UI_Cvar_VariableString ( "ui_r_lodbias" ) );
	trap_Cvar_Set ( "r_picmip", UI_Cvar_VariableString ( "ui_r_picmip" ) );
	trap_Cvar_Set ( "r_texturebits", UI_Cvar_VariableString ( "ui_r_texturebits" ) );
	trap_Cvar_Set ( "r_texturemode", UI_Cvar_VariableString ( "ui_r_texturemode" ) );
	trap_Cvar_Set ( "r_detailtextures", UI_Cvar_VariableString ( "ui_r_detailtextures" ) );
	trap_Cvar_Set ( "r_ext_compress_textures", UI_Cvar_VariableString ( "ui_r_ext_compress_textures" ) );
	trap_Cvar_Set ( "r_ext_multisample", UI_Cvar_VariableString ( "ui_r_ext_multisample" ) );
	trap_Cvar_Set ( "r_depthbits", UI_Cvar_VariableString ( "ui_r_depthbits" ) );
	trap_Cvar_Set ( "r_subdivisions", UI_Cvar_VariableString ( "ui_r_subdivisions" ) );
	trap_Cvar_Set ( "r_fastSky", UI_Cvar_VariableString ( "ui_r_fastSky" ) );
	trap_Cvar_Set ( "r_inGameVideo", UI_Cvar_VariableString ( "ui_r_inGameVideo" ) );
	trap_Cvar_Set ( "r_allowExtensions", UI_Cvar_VariableString ( "ui_r_allowExtensions" ) );
	trap_Cvar_Set ( "cg_shadows", UI_Cvar_VariableString ( "ui_cg_shadows" ) );
	trap_Cvar_Set ( "ui_r_modified", "0" );

	trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart;" );
}

/*
=================
UI_GetVideoSetup

Retrieves the current actual video settings into the temporary user
interface versions of the cvars.
=================
*/
void UI_GetVideoSetup ( void )
{
	// Make sure the cvars are registered as read only.
	trap_Cvar_Register ( NULL, "ui_r_glCustom",				"4", CVAR_ROM|CVAR_INTERNAL|CVAR_ARCHIVE|CVAR_GLOBAL );

	trap_Cvar_Register ( NULL, "ui_r_mode",					"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_fullscreen",			"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_colorbits",			"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_lodbias",				"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_picmip",				"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_texturebits",			"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_texturemode",			"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_detailtextures",		"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_ext_compress_textures","0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_ext_multisample",		"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_depthbits",			"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_subdivisions",			"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_fastSky",				"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_inGameVideo",			"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_allowExtensions",		"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_cg_shadows",				"0", CVAR_ROM|CVAR_INTERNAL );
	trap_Cvar_Register ( NULL, "ui_r_modified",				"0", CVAR_ROM|CVAR_INTERNAL );
	
	// Copy over the real video cvars into their temporary counterparts
	trap_Cvar_Set ( "ui_r_mode",		UI_Cvar_VariableString ( "r_mode" ) );
	trap_Cvar_Set ( "ui_r_colorbits",	UI_Cvar_VariableString ( "r_colorbits" ) );
	trap_Cvar_Set ( "ui_r_fullscreen",	UI_Cvar_VariableString ( "r_fullscreen" ) );
	trap_Cvar_Set ( "ui_r_lodbias",		UI_Cvar_VariableString ( "r_lodbias" ) );
	trap_Cvar_Set ( "ui_r_picmip",		UI_Cvar_VariableString ( "r_picmip" ) );
	trap_Cvar_Set ( "ui_r_texturebits", UI_Cvar_VariableString ( "r_texturebits" ) );
	trap_Cvar_Set ( "ui_r_texturemode", UI_Cvar_VariableString ( "r_texturemode" ) );
	trap_Cvar_Set ( "ui_r_detailtextures", UI_Cvar_VariableString ( "r_detailtextures" ) );
	trap_Cvar_Set ( "ui_r_ext_compress_textures", UI_Cvar_VariableString ( "r_ext_compress_textures" ) );
	trap_Cvar_Set ( "ui_r_ext_multisample", UI_Cvar_VariableString ( "r_ext_multisample" ) );
	trap_Cvar_Set ( "ui_r_depthbits", UI_Cvar_VariableString ( "r_depthbits" ) );
	trap_Cvar_Set ( "ui_r_subdivisions", UI_Cvar_VariableString ( "r_subdivisions" ) );
	trap_Cvar_Set ( "ui_r_fastSky", UI_Cvar_VariableString ( "r_fastSky" ) );
	trap_Cvar_Set ( "ui_r_inGameVideo", UI_Cvar_VariableString ( "r_inGameVideo" ) );
	trap_Cvar_Set ( "ui_r_allowExtensions", UI_Cvar_VariableString ( "r_allowExtensions" ) );
	trap_Cvar_Set ( "ui_cg_shadows", UI_Cvar_VariableString ( "cg_shadows" ) );
	trap_Cvar_Set ( "ui_r_modified", "0" );

	// screen resolutions
	trap_Cvar_Register(NULL, "ui_r_aspectratio", "0", CVAR_ROM | CVAR_INTERNAL);
	trap_Cvar_Set("ui_r_aspectratio", UI_Cvar_VariableString("r_aspectratio"));
}

static void UI_RunMenuScript(const char **args)
{
	const char *name, *name2;
	char buff[1024];

	if (String_Parse(args, &name)) 
	{
		if (Q_stricmp(name, "StartServer") == 0) 
		{
			int i, added = 0;
			float skill;
			int warmupTime = 0;
			int doWarmup = 0;

			trap_Cvar_Set("cg_thirdPerson", "0");
			trap_Cvar_Set("cg_cameraOrbit", "0");
			trap_Cvar_Set("ui_singlePlayerActive", "0");
			trap_Cvar_SetValue( "dedicated", Com_Clamp( 0, 2, ui_dedicated.integer ) );
			trap_Cvar_SetValue( "g_gametype", Com_Clamp( 0, GT_MAX_GAME_TYPE, uiInfo.gameTypes[ui_netGameType.integer].gtEnum ) );
			trap_Cvar_Set("g_redTeam", UI_Cvar_VariableString("ui_teamName"));
			trap_Cvar_Set("g_blueTeam", UI_Cvar_VariableString("ui_opponentName"));
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName ) );
			skill = trap_Cvar_VariableValue( "g_spSkill" );

			//Cap the warmup values in case the user tries a dumb setting.
			warmupTime = trap_Cvar_VariableValue( "g_warmup" );
			doWarmup = trap_Cvar_VariableValue( "g_doWarmup" );

			if (doWarmup && warmupTime < 1)
			{
				trap_Cvar_Set("g_doWarmup", "0");
			}
			if (warmupTime < 5)
			{
				trap_Cvar_Set("g_warmup", "5");
			}
			if (warmupTime > 120)
			{
				trap_Cvar_Set("g_warmup", "120");
			}

			if (trap_Cvar_VariableValue( "g_gametype" ) == GT_TOURNAMENT)
			{ //always set fraglimit 1 when starting a duel game
				trap_Cvar_Set("fraglimit", "1");
			}

			for (i = 0; i < PLAYERS_PER_TEAM; i++) 
			{
				int bot = trap_Cvar_VariableValue( va("ui_blueteam%i", i+1));
				int maxcl = trap_Cvar_VariableValue( "sv_maxClients" );

				if (bot > 1) 
				{
					int numval = i+1;

					numval *= 2;

					numval -= 1;

					if (numval <= maxcl)
					{
						if (ui_actualNetGameType.integer >= GT_TEAM) {
							Com_sprintf( buff, sizeof(buff), "addbot %s %f %s\n", UI_GetBotNameByNumber(bot-2), skill, "Blue");
						} else {
							Com_sprintf( buff, sizeof(buff), "addbot %s %f \n", UI_GetBotNameByNumber(bot-2), skill);
						}
						trap_Cmd_ExecuteText( EXEC_APPEND, buff );
						added++;
					}
				}
				bot = trap_Cvar_VariableValue( va("ui_redteam%i", i+1));
				if (bot > 1) {
					int numval = i+1;

					numval *= 2;

					if (numval <= maxcl)
					{
						if (ui_actualNetGameType.integer >= GT_TEAM) {
							Com_sprintf( buff, sizeof(buff), "addbot %s %f %s\n", UI_GetBotNameByNumber(bot-2), skill, "Red");
						} else {
							Com_sprintf( buff, sizeof(buff), "addbot %s %f \n", UI_GetBotNameByNumber(bot-2), skill);
						}
						trap_Cmd_ExecuteText( EXEC_APPEND, buff );
						added++;
					}
				}
				if (added >= maxcl)
				{ //this means the client filled up all their slots in the UI with bots. So stretch out an extra slot for them, and then stop adding bots.
					trap_Cvar_Set("sv_maxClients", va("%i", added+1));
					break;
				}
			}
		} else if (Q_stricmp(name, "updateSPMenu") == 0) {
			UI_SetCapFragLimits(qtrue);
			UI_MapCountByGameType(qtrue);
			ui_mapIndex.integer = UI_GetIndexFromSelection(ui_currentMap.integer);
			trap_Cvar_Set("ui_mapIndex", va("%d", ui_mapIndex.integer));
			Menu_SetFeederSelection(NULL, FEEDER_MAPS, ui_mapIndex.integer, "skirmish");
			UI_GameType_HandleKey(0, 0, A_MOUSE1, qfalse);
			UI_GameType_HandleKey(0, 0, A_MOUSE2, qfalse);
		} else if (Q_stricmp(name, "resetDefaults") == 0) {
			trap_Cmd_ExecuteText( EXEC_APPEND, "cvar_restart\n");
			trap_Cmd_ExecuteText( EXEC_APPEND, "exec mpdefault.cfg\n");
			trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
			trap_Cvar_Set("com_introPlayed", "1" );
#ifdef USE_CD_KEY
		} else if (Q_stricmp(name, "getCDKey") == 0) {
			char out[17];
			trap_GetCDKey(buff, 17);
			trap_Cvar_Set("cdkey1", "");
			trap_Cvar_Set("cdkey2", "");
			trap_Cvar_Set("cdkey3", "");
			trap_Cvar_Set("cdkey4", "");
			if (strlen(buff) == CDKEY_LEN) {
				Q_strncpyz(out, buff, 5);
				trap_Cvar_Set("cdkey1", out);
				Q_strncpyz(out, buff + 4, 5);
				trap_Cvar_Set("cdkey2", out);
				Q_strncpyz(out, buff + 8, 5);
				trap_Cvar_Set("cdkey3", out);
				Q_strncpyz(out, buff + 12, 5);
				trap_Cvar_Set("cdkey4", out);
			}

		} else if (Q_stricmp(name, "verifyCDKey") == 0) {
			buff[0] = '\0';
			Q_strcat(buff, 1024, UI_Cvar_VariableString("cdkey1")); 
			Q_strcat(buff, 1024, UI_Cvar_VariableString("cdkey2")); 
			Q_strcat(buff, 1024, UI_Cvar_VariableString("cdkey3")); 
			Q_strcat(buff, 1024, UI_Cvar_VariableString("cdkey4")); 
			trap_Cvar_Set("cdkey", buff);
			if (trap_VerifyCDKey(buff, UI_Cvar_VariableString("cdkeychecksum"))) {
				trap_Cvar_Set("ui_cdkeyvalid", "CD Key Appears to be valid.");
				trap_SetCDKey(buff);
			} else {
				trap_Cvar_Set("ui_cdkeyvalid", "CD Key does not appear to be valid.");
			}
#endif // USE_CD_KEY
		} else if (Q_stricmp(name, "loadArenas") == 0) {
			UI_LoadArenas();
			UI_MapCountByGameType(qfalse);
			Menu_SetFeederSelection(NULL, FEEDER_ALLMAPS, gUISelectedMap, "createserver");
			//uiForceRank = Com_Clampi(0, MAX_FORCE_RANK, trap_Cvar_VariableValue("g_maxForceRank"));
		} else if (Q_stricmp(name, "saveControls") == 0) {
			Controls_SetConfig(qtrue);
		} else if (Q_stricmp(name, "loadControls") == 0) {
			Controls_GetConfig();
		} else if (Q_stricmp(name, "clearError") == 0) {
			trap_Cvar_Set("com_errorMessage", "");
		} else if (Q_stricmp(name, "loadGameInfo") == 0) {
#ifdef PRE_RELEASE_TADEMO
			UI_ParseGameInfo("demogameinfo.txt");
#else
			UI_ParseGameInfo("ui/jk2mp/gameinfo.txt");
#endif
			UI_LoadBestScores(uiInfo.mapList[ui_currentMap.integer].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum);
		} else if (Q_stricmp(name, "resetScores") == 0) {
			UI_ClearScores();
		} else if (Q_stricmp(name, "RefreshServers") == 0) {
			UI_StartServerRefresh(qtrue);
			UI_BuildServerDisplayList(1);
		} else if (Q_stricmp(name, "RefreshFilter") == 0) {
			UI_StartServerRefresh(qfalse);
			UI_BuildServerDisplayList(1);
		} else if (Q_stricmp(name, "RunSPDemo") == 0) {
			if (uiInfo.demoAvailable) {
			  trap_Cmd_ExecuteText( EXEC_APPEND, va("demo %s_%i\n", uiInfo.mapList[ui_currentMap.integer].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum));
			}
		} else if (Q_stricmp(name, "LoadDemos") == 0) {
			UI_LoadDemos();
		} else if (Q_stricmp(name, "LoadMovies") == 0) {
			UI_LoadMovies();
		} else if (Q_stricmp(name, "LoadMods") == 0) {
			UI_LoadMods();
		} else if (Q_stricmp(name, "LoadDLFiles") == 0) {
			UI_LoadDLFiles();
		} else if (Q_stricmp(name, "playMovie") == 0) {
			if (uiInfo.previewMovie >= 0) {
			  trap_CIN_StopCinematic(uiInfo.previewMovie);
			}
			trap_Cmd_ExecuteText( EXEC_APPEND, va("cinematic %s.roq 2\n", uiInfo.movieList[uiInfo.movieIndex]));
		} else if (Q_stricmp(name, "RunMod") == 0) {
			trap_Cvar_Set( "fs_game", uiInfo.modList[uiInfo.modIndex].modName);
			trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart;" );
		} else if (Q_stricmp(name, "RunDemo") == 0) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va("demo \"%s\"\n", uiInfo.demoList[uiInfo.demoIndex]));
		} else if (Q_stricmp(name, "Quake3") == 0) {
			trap_Cvar_Set( "fs_game", "");
			trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart;" );
		} else if (Q_stricmp(name, "closeJoin") == 0) {
			if (uiInfo.serverStatus.refreshActive) {
				UI_StopServerRefresh();
				uiInfo.serverStatus.nextDisplayRefresh = 0;
				uiInfo.nextServerStatusRefresh = 0;
				uiInfo.nextFindPlayerRefresh = 0;
				UI_BuildServerDisplayList(1);
			} else {
				Menus_CloseByName("joinserver");
				Menus_OpenByName("main");
			}
		} else if (Q_stricmp(name, "StopRefresh") == 0) {
			UI_StopServerRefresh();
			uiInfo.serverStatus.nextDisplayRefresh = 0;
			uiInfo.nextServerStatusRefresh = 0;
			uiInfo.nextFindPlayerRefresh = 0;
		} else if (Q_stricmp(name, "UpdateFilter") == 0) {
			if (ui_netSource.integer == AS_LOCAL) {
				UI_StartServerRefresh(qtrue);
			}
			UI_BuildServerDisplayList(1);
			UI_FeederSelection(FEEDER_SERVERS, 0);
		} else if (Q_stricmp(name, "ServerStatus") == 0) {
			trap_LAN_GetServerAddressString(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], uiInfo.serverStatusAddress, sizeof(uiInfo.serverStatusAddress));
			UI_BuildServerStatus(qtrue);
		} else if (Q_stricmp(name, "FoundPlayerServerStatus") == 0) {
			Q_strncpyz(uiInfo.serverStatusAddress, uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer], sizeof(uiInfo.serverStatusAddress));
			UI_BuildServerStatus(qtrue);
			Menu_SetFeederSelection(NULL, FEEDER_FINDPLAYER, 0, NULL);
		} else if (Q_stricmp(name, "FindPlayer") == 0) {
			UI_BuildFindPlayerList(qtrue);
			// clear the displayed server status info
			uiInfo.serverStatusInfo.numLines = 0;
			Menu_SetFeederSelection(NULL, FEEDER_FINDPLAYER, 0, NULL);
		} else if (Q_stricmp(name, "JoinServer") == 0) {
			trap_Cvar_Set("cg_thirdPerson", "0");
			trap_Cvar_Set("cg_cameraOrbit", "0");
			trap_Cvar_Set("ui_singlePlayerActive", "0");
			if (uiInfo.serverStatus.currentServer >= 0 && uiInfo.serverStatus.currentServer < uiInfo.serverStatus.numDisplayServers) {
				trap_LAN_GetServerAddressString(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], buff, 1024);
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", buff ) );
			}
		} else if (Q_stricmp(name, "FoundPlayerJoinServer") == 0) {
			trap_Cvar_Set("ui_singlePlayerActive", "0");
			if (uiInfo.currentFoundPlayerServer >= 0 && uiInfo.currentFoundPlayerServer < uiInfo.numFoundPlayerServers) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer] ) );
			}
		} else if (Q_stricmp(name, "Quit") == 0) {
			trap_Cvar_Set("ui_singlePlayerActive", "0");
			trap_Cmd_ExecuteText( EXEC_NOW, "quit");
		} else if (Q_stricmp(name, "Controls") == 0) {
		  trap_Cvar_Set( "cl_paused", "1" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			Menus_CloseAll();
			Menus_ActivateByName("setup_menu2");
		} 
		else if (Q_stricmp(name, "Leave") == 0) 
		{
			trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect\n" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			Menus_CloseAll();
			Menus_ActivateByName("main");
		} 
		else if (Q_stricmp(name, "getvideosetup") == 0) 
		{
			UI_GetVideoSetup ( );
		}
		else if (Q_stricmp(name, "updatevideosetup") == 0)
		{
			UI_UpdateVideoSetup ( );
		}
		else if (Q_stricmp(name, "ServerSort") == 0) 
		{
			int sortColumn;
			if (Int_Parse(args, &sortColumn)) {
				// if same column we're already sorting on then flip the direction
				if (sortColumn == uiInfo.serverStatus.sortKey) {
					uiInfo.serverStatus.sortDir = !uiInfo.serverStatus.sortDir;
				}
				// make sure we sort again
				UI_ServersSort(sortColumn, qtrue);
			}
		} else if (Q_stricmp(name, "nextSkirmish") == 0) {
			UI_StartSkirmish(qtrue);
		} else if (Q_stricmp(name, "SkirmishStart") == 0) {
			UI_StartSkirmish(qfalse);
		} else if (Q_stricmp(name, "closeingame") == 0) {
			trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
			trap_Key_ClearStates();
			trap_Cvar_Set( "cl_paused", "0" );
			Menus_CloseAll();
		} else if (Q_stricmp(name, "voteMap") == 0) {
			if (ui_currentNetMap.integer >=0 && ui_currentNetMap.integer < uiInfo.mapCount) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va("callvote map %s\n",uiInfo.mapList[ui_currentNetMap.integer].mapLoadName) );
			}
		} else if (Q_stricmp(name, "voteKick") == 0) {
			if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
				//trap_Cmd_ExecuteText( EXEC_APPEND, va("callvote kick \"%s\"\n",uiInfo.playerNames[uiInfo.playerIndex]) );
				trap_Cmd_ExecuteText( EXEC_APPEND, va("callvote clientkick \"%i\"\n",uiInfo.playerIndexes[uiInfo.playerIndex]) );
			}
		} else if (Q_stricmp(name, "voteGame") == 0) {
			if (ui_netGameType.integer >= 0 && ui_netGameType.integer < uiInfo.numGameTypes) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va("callvote g_gametype %i\n",uiInfo.gameTypes[ui_netGameType.integer].gtEnum) );
			}
		} else if (Q_stricmp(name, "voteLeader") == 0) {
			if (uiInfo.teamIndex >= 0 && uiInfo.teamIndex < uiInfo.myTeamCount) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va("callteamvote leader \"%s\"\n",uiInfo.teamNames[uiInfo.teamIndex]) );
			}
		} else if (Q_stricmp(name, "addBot") == 0) {
			if (trap_Cvar_VariableValue("g_gametype") >= GT_TEAM) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va("addbot %s %i %s\n", UI_GetBotNameByNumber(uiInfo.botIndex), uiInfo.skillIndex+1, (uiInfo.redBlue == 0) ? "Red" : "Blue") );
			} else {
				trap_Cmd_ExecuteText( EXEC_APPEND, va("addbot %s %i %s\n", UI_GetBotNameByNumber(uiInfo.botIndex), uiInfo.skillIndex+1, (uiInfo.redBlue == 0) ? "Red" : "Blue") );
			}
		} else if (Q_stricmp(name, "addFavorite") == 0) 
		{
			if (ui_netSource.integer != AS_FAVORITES) 
			{
				char name[MAX_NAME_LENGTH];
				char addr[MAX_NAME_LENGTH];
				int res;

				trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], buff, MAX_STRING_CHARS);
				name[0] = addr[0] = '\0';
				Q_strncpyz(name, 	Info_ValueForKey(buff, "hostname"), MAX_NAME_LENGTH);
				Q_strncpyz(addr, 	Info_ValueForKey(buff, "addr"), MAX_NAME_LENGTH);
				if (strlen(name) > 0 && strlen(addr) > 0) 
				{
					res = trap_LAN_AddServer(AS_FAVORITES, name, addr);
					if (res == 0) 
					{
						// server already in the list
						Com_Printf("Favorite already in list\n");
					}
					else if (res == -1) 
					{
						// list full
						Com_Printf("Favorite list full\n");
					}
					else 
					{
						// successfully added
						Com_Printf("Added favorite server %s\n", addr);


//						trap_SP_GetStringTextString((char *)va("%s_GETTINGINFOFORSERVERS",uiInfo.uiDC.Assets.stripedFile), holdSPString, sizeof(holdSPString));
//						Text_Paint(rect->x, rect->y, scale, newColor, va((char *) holdSPString, trap_LAN_GetServerCount(ui_netSource.integer)), 0, 0, textStyle);

					}
				}
			}
		} 
		else if (Q_stricmp(name, "deleteFavorite") == 0) 
		{
			if (ui_netSource.integer == AS_FAVORITES) 
			{
				char addr[MAX_NAME_LENGTH];
				trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], buff, MAX_STRING_CHARS);
				addr[0] = '\0';
				Q_strncpyz(addr, 	Info_ValueForKey(buff, "addr"), MAX_NAME_LENGTH);
				if (strlen(addr) > 0) 
				{
					trap_LAN_RemoveServer(AS_FAVORITES, addr);
				}
			}
		} 
		else if (Q_stricmp(name, "createFavorite") == 0) 
		{
		//	if (ui_netSource.integer == AS_FAVORITES) 
		//rww - don't know why this check was here.. why would you want to only add new favorites when the filter was favorites?
			{
				char name[MAX_NAME_LENGTH];
				char addr[MAX_NAME_LENGTH];
				int res;

				name[0] = addr[0] = '\0';
				Q_strncpyz(name, 	UI_Cvar_VariableString("ui_favoriteName"), MAX_NAME_LENGTH);
				Q_strncpyz(addr, 	UI_Cvar_VariableString("ui_favoriteAddress"), MAX_NAME_LENGTH);
				if (/*strlen(name) > 0 &&*/ strlen(addr) > 0) {
					res = trap_LAN_AddServer(AS_FAVORITES, name, addr);
					if (res == 0) {
						// server already in the list
						Com_Printf("Favorite already in list\n");
					}
					else if (res == -1) {
						// list full
						Com_Printf("Favorite list full\n");
					}
					else {
						// successfully added
						Com_Printf("Added favorite server %s\n", addr);
					}
				}
			}
		} else if (Q_stricmp(name, "orders") == 0) {
			const char *orders;
			if (String_Parse(args, &orders)) {
				int selectedPlayer = trap_Cvar_VariableValue("cg_selectedPlayer");
				if (selectedPlayer < uiInfo.myTeamCount) {
					strcpy(buff, orders);
					trap_Cmd_ExecuteText( EXEC_APPEND, va(buff, uiInfo.teamClientNums[selectedPlayer]) );
					trap_Cmd_ExecuteText( EXEC_APPEND, "\n" );
				} else {
					int i;
					for (i = 0; i < uiInfo.myTeamCount; i++) {
						if (Q_stricmp(UI_Cvar_VariableString("name"), uiInfo.teamNames[i]) == 0) {
							continue;
						}
						strcpy(buff, orders);
						trap_Cmd_ExecuteText( EXEC_APPEND, va(buff, uiInfo.teamNames[i]) );
						trap_Cmd_ExecuteText( EXEC_APPEND, "\n" );
					}
				}
				trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
				trap_Key_ClearStates();
				trap_Cvar_Set( "cl_paused", "0" );
				Menus_CloseAll();
			}
		} else if (Q_stricmp(name, "voiceOrdersTeam") == 0) {
			const char *orders;
			if (String_Parse(args, &orders)) {
				int selectedPlayer = trap_Cvar_VariableValue("cg_selectedPlayer");
				if (selectedPlayer == uiInfo.myTeamCount) {
					trap_Cmd_ExecuteText( EXEC_APPEND, orders );
					trap_Cmd_ExecuteText( EXEC_APPEND, "\n" );
				}
				trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
				trap_Key_ClearStates();
				trap_Cvar_Set( "cl_paused", "0" );
				Menus_CloseAll();
			}
		} else if (Q_stricmp(name, "voiceOrders") == 0) {
			const char *orders;
			if (String_Parse(args, &orders)) {
				int selectedPlayer = trap_Cvar_VariableValue("cg_selectedPlayer");

				if (selectedPlayer == uiInfo.myTeamCount)
				{
					selectedPlayer = -1;
					strcpy(buff, orders);
					trap_Cmd_ExecuteText( EXEC_APPEND, va(buff, selectedPlayer) );
				}
				else
				{
					strcpy(buff, orders);
					trap_Cmd_ExecuteText( EXEC_APPEND, va(buff, uiInfo.teamClientNums[selectedPlayer]) );
				}
				trap_Cmd_ExecuteText( EXEC_APPEND, "\n" );

				trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
				trap_Key_ClearStates();
				trap_Cvar_Set( "cl_paused", "0" );
				Menus_CloseAll();
			}
		}
		else if (Q_stricmp(name, "setForce") == 0)
		{
			const char *teamArg;

			if (String_Parse(args, &teamArg))
			{
				if ( Q_stricmp( "none", teamArg ) == 0 )
				{
					UI_UpdateClientForcePowers(NULL);
				}
				else if ( Q_stricmp( "same", teamArg ) == 0 )
				{//stay on current team
					int myTeam = (int)(trap_Cvar_VariableValue("ui_myteam"));
					if ( myTeam != TEAM_SPECTATOR )
					{
						UI_UpdateClientForcePowers(UI_TeamName(myTeam));//will cause him to respawn, if it's been 5 seconds since last one
					}
					else
					{
						UI_UpdateClientForcePowers(NULL);//just update powers
					}
				}
				else
				{
					UI_UpdateClientForcePowers(teamArg);
				}
			}
			else
			{
				UI_UpdateClientForcePowers(NULL);
			}
		}
		else if (Q_stricmp(name, "saveTemplate") == 0) {
			UI_SaveForceTemplate();
		} else if (Q_stricmp(name, "refreshForce") == 0) {
			UI_UpdateForcePowers();
		} else if (Q_stricmp(name, "glCustom") == 0) {
			trap_Cvar_Set("ui_r_glCustom", "4");
		} 
		else if (Q_stricmp(name, "forcePowersDisable") == 0) 
		{
			int	forcePowerDisable,i;

			forcePowerDisable = trap_Cvar_VariableValue("g_forcePowerDisable");

			// It was set to something, so might as well make sure it got all flags set.
			if (forcePowerDisable)
			{
				for (i=0;i<NUM_FORCE_POWERS;i++)
				{
					forcePowerDisable |= (1<<i);
				}

				trap_Cvar_Set("g_forcePowerDisable", va("%i",forcePowerDisable));
			}

		} 
		else if (Q_stricmp(name, "weaponDisable") == 0) 
		{
			int	weaponDisable,i;
			const char *cvarString;

			if (ui_netGameType.integer == GT_TOURNAMENT)
			{
				cvarString = "g_duelWeaponDisable";
			}
			else
			{
				cvarString = "g_weaponDisable";
			}

			weaponDisable = trap_Cvar_VariableValue(cvarString);

			// It was set to something, so might as well make sure it got all flags set.
			if (weaponDisable)
			{
				for (i=0;i<WP_NUM_WEAPONS;i++)
				{
					if (i!=WP_SABER)
					{
						weaponDisable |= (1<<i);
					}
				}

				trap_Cvar_Set(cvarString, va("%i",weaponDisable));
			}
		} 
		else if (Q_stricmp(name, "updateForceStatus") == 0)
		{
			UpdateForceStatus();
		}
		else if (Q_stricmp(name, "update") == 0) 
		{
			if (String_Parse(args, &name2)) 
			{
				UI_Update(name2);
			}
		} else if (Q_stricmp(name, "MVContinueDownload") == 0) {
			// download popup
			trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
			trap_Key_ClearStates();
			Menus_CloseAll();

			if (String_Parse(args, &name2)) {
				if (!Q_stricmp(name2, "accept")) {
					trap_CL_ContinueCurrentDownload(DL_ACCEPT);
				} else if (!Q_stricmp(name2, "deny")) {
					trap_CL_ContinueCurrentDownload(DL_ABORT);
				} else if (!Q_stricmp(name2, "deny_blacklist")) {
					trap_CL_ContinueCurrentDownload(DL_ABORT_BLACKLIST);
				}
			}
		} else if (Q_stricmp(name, "DLInfo") == 0) {
			if (uiInfo.downloadsCount) {
				trap_Cvar_Set("ui_dlinfo_name", uiInfo.downloadsList[uiInfo.downloadsIndex].name);

				Menus_ActivateByName("download_info");
			}
		} else if (Q_stricmp(name, "DLPermanent") == 0) {
			if (uiInfo.downloadsCount && !uiInfo.downloadsList[uiInfo.downloadsIndex].blacklisted) {
				trap_FS_RMDLPrefix(uiInfo.downloadsList[uiInfo.downloadsIndex].name);
				UI_LoadDLFiles();
			}
		} else if (Q_stricmp(name, "DLRemove") == 0) {
			if (uiInfo.downloadsCount) {
				trap_UI_DeleteDLFile(&uiInfo.downloadsList[uiInfo.downloadsIndex]);
				UI_LoadDLFiles();
			}
		} else {
			Com_Printf("unknown UI script %s\n", name);
		}
	}
}

static void UI_GetTeamColor(vec4_t *color) {
}

/*
==================
UI_MapCountByGameType
==================
*/
static int UI_MapCountByGameType(qboolean singlePlayer) {
	int i, c, game;
	c = 0;
	game = singlePlayer ? uiInfo.gameTypes[ui_gameType.integer].gtEnum : uiInfo.gameTypes[ui_netGameType.integer].gtEnum;
	if (game == GT_SINGLE_PLAYER) {
		game++;
	} 
	if (game == GT_TEAM) {
		game = GT_FFA;
	}
	if (game == GT_HOLOCRON || game == GT_JEDIMASTER) {
		game = GT_FFA;
	}

	for (i = 0; i < uiInfo.mapCount; i++) {
		uiInfo.mapList[i].active = qfalse;
		if ( uiInfo.mapList[i].typeBits & (1 << game)) {
			if (singlePlayer) {
				if (!(uiInfo.mapList[i].typeBits & (1 << GT_SINGLE_PLAYER))) {
					continue;
				}
			}
			c++;
			uiInfo.mapList[i].active = qtrue;
		}
	}
	return c;
}

qboolean UI_hasSkinForBase(const char *base, const char *team) {
	char	test[1024];
	fileHandle_t	f;
	
	Com_sprintf( test, sizeof( test ), "models/players/%s/%s/lower_default.skin", base, team );
	trap_FS_FOpenFile(test, &f, FS_READ);
	if (f != 0) {
		trap_FS_FCloseFile(f);
		return qtrue;
	}
	Com_sprintf( test, sizeof( test ), "models/players/characters/%s/%s/lower_default.skin", base, team );
	trap_FS_FOpenFile(test, &f, FS_READ);
	if (f != 0) {
		trap_FS_FCloseFile(f);
		return qtrue;
	}
	return qfalse;
}

/*
==================
UI_MapCountByTeam
==================
*/
static int UI_HeadCountByTeam() {
	static int init = 0;
	int i, j, k, c, tIndex;
	
	c = 0;
	if (!init) {
		for (i = 0; i < uiInfo.characterCount; i++) {
			uiInfo.characterList[i].reference = 0;
			for (j = 0; j < uiInfo.teamCount; j++) {
			  if (UI_hasSkinForBase(uiInfo.characterList[i].base, uiInfo.teamList[j].teamName)) {
					uiInfo.characterList[i].reference |= (1<<j);
			  }
			}
		}
		init = 1;
	}

	tIndex = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));

	// do names
	for (i = 0; i < uiInfo.characterCount; i++) {
		uiInfo.characterList[i].active = qfalse;
		for(j = 0; j < TEAM_MEMBERS; j++) {
			if (uiInfo.teamList[tIndex].teamMembers[j] != NULL) {
				if (uiInfo.characterList[i].reference&(1<<tIndex)) {// && Q_stricmp(uiInfo.teamList[tIndex].teamMembers[j], uiInfo.characterList[i].name)==0) {
					uiInfo.characterList[i].active = qtrue;
					c++;
					break;
				}
			}
		}
	}

	// and then aliases
	for(j = 0; j < TEAM_MEMBERS; j++) {
		for(k = 0; k < uiInfo.aliasCount; k++) {
			if (uiInfo.aliasList[k].name != NULL) {
				if (Q_stricmp(uiInfo.teamList[tIndex].teamMembers[j], uiInfo.aliasList[k].name)==0) {
					for (i = 0; i < uiInfo.characterCount; i++) {
						if (uiInfo.characterList[i].headImage != -1 && uiInfo.characterList[i].reference&(1<<tIndex) && Q_stricmp(uiInfo.aliasList[k].ai, uiInfo.characterList[i].name)==0) {
							if (uiInfo.characterList[i].active == qfalse) {
								uiInfo.characterList[i].active = qtrue;
								c++;
							}
							break;
						}
					}
				}
			}
		}
	}
	return c;
}


q3Head_t *UI_GetHeadByIndex( int index )
{
	q3Head_t *current = uiInfo.q3Heads;
	int i = 0;

	while ( current )
	{
		if ( i == index ) return current;
		i++;

		current = current->next;
	}
	return NULL;
}

static q3Head_t *UI_GetHeadByName( const char *name )
{
	q3Head_t *current = uiInfo.q3Heads;

	while ( current )
	{
		if ( !Q_stricmp(name, current->name) ) return current;
		current = current->next;
	}
	return NULL;
}

static void UI_InsertHeadRaw( q3Head_t *head )
{
	q3Head_t **current;

	// Find the end of the list
	current = &uiInfo.q3Heads;
	while ( *current ) current = &(*current)->next;
	*current = head;

	uiInfo.q3HeadCount++;
}

static void UI_InsertHead( const char *name )
{
	q3Head_t *newHead;

	// Prevent duplicates
	if ( UI_GetHeadByName(name) ) return;

	// Get the memory and copy the string
	newHead = BG_Alloc( sizeof(q3Head_t) );
	newHead->name = BG_Alloc( strlen(name) + 1 );
	strcpy( (char*)newHead->name, name );
	newHead->icon = 0;
	newHead->next = NULL;

	UI_InsertHeadRaw( newHead );
}

static const char *UI_GetSkinSuffixForTeamColor( void )
{
	switch(uiSkinColor)
	{
		case SKINCOLOR_RED:
			return "/red";
		case SKINCOLOR_BLUE:
			return "/blue";
		case SKINCOLOR_OTHER:
			return "";
		default:
			return "/default";
	}
}

static qboolean UI_HeadBelongsToCurrentTeamColor( q3Head_t *head )
{
	const char *teamname = UI_GetSkinSuffixForTeamColor();

	if ( !head || !head->name ) return qfalse;

	if ( uiSkinColor == SKINCOLOR_OTHER && !(strstr(head->name, "/red") || strstr(head->name, "/blue") || strstr(head->name, "/default")) )
		return qtrue;
	if ( uiSkinColor != SKINCOLOR_OTHER && strstr(head->name, teamname) )
		return qtrue;

	return qfalse;
}

/*
==================
UI_HeadCountByColor
==================
*/
static int UI_HeadCountByColor() {
	int c;
	q3Head_t *head = uiInfo.q3Heads;

	c = 0;

	// Count each head with this color
	while ( head )
	{
		if ( UI_HeadBelongsToCurrentTeamColor(head) )
		{
			c++;
		}
		head = head->next;
	}
	return c;
}

/*
==================
UI_InsertServerIntoDisplayList
==================
*/
static void UI_InsertServerIntoDisplayList(int num, int position) {
	int i;

	if (position < 0 || position > uiInfo.serverStatus.numDisplayServers ) {
		return;
	}
	//
	uiInfo.serverStatus.numDisplayServers++;
	for (i = uiInfo.serverStatus.numDisplayServers; i > position; i--) {
		uiInfo.serverStatus.displayServers[i] = uiInfo.serverStatus.displayServers[i-1];
	}
	uiInfo.serverStatus.displayServers[position] = num;
}

/*
==================
UI_RemoveServerFromDisplayList
==================
*/
static void UI_RemoveServerFromDisplayList(int num) {
	int i, j;

	for (i = 0; i < uiInfo.serverStatus.numDisplayServers; i++) {
		if (uiInfo.serverStatus.displayServers[i] == num) {
			uiInfo.serverStatus.numDisplayServers--;
			for (j = i; j < uiInfo.serverStatus.numDisplayServers; j++) {
				uiInfo.serverStatus.displayServers[j] = uiInfo.serverStatus.displayServers[j+1];
			}
			return;
		}
	}
}

/*
==================
UI_BinaryServerInsertion
==================
*/
static void UI_BinaryServerInsertion(int num) {
	int mid, offset, res, len, fakeKey;

	// use binary search to insert server
	len = uiInfo.serverStatus.numDisplayServers;
	mid = len;
	offset = 0;
	res = 0;
	while(mid > 0) {
		mid = len >> 1;
		//

		fakeKey = uiInfo.serverStatus.sortKey;

		// fake over to new sorting without bots
		if (fakeKey == SORT_CLIENTS && trap_Cvar_VariableValue("ui_botfilter")) {
			fakeKey = MVSORT_CLIENTS_NOBOTS;
		}

		res = trap_LAN_CompareServers(ui_netSource.integer, fakeKey,
					uiInfo.serverStatus.sortDir, num, uiInfo.serverStatus.displayServers[offset+mid]);
		// if equal
		if (res == 0) {
			UI_InsertServerIntoDisplayList(num, offset+mid);
			return;
		}
		// if larger
		else if (res == 1) {
			offset += mid;
			len -= mid;
		}
		// if smaller
		else {
			len -= mid;
		}
	}
	if (res == 1) {
		offset++;
	}
	UI_InsertServerIntoDisplayList(num, offset);
}

/*
==================
UI_BuildServerDisplayList
==================
*/
static void UI_BuildServerDisplayList(int force) {
	int i, count, maxClients, ping, game, visible;
	int len;
	char info[MAX_STRING_CHARS];
//	qboolean startRefresh = qtrue; TTimo: unused
	static int numinvisible;

	if (!(force || uiInfo.uiDC.realTime > uiInfo.serverStatus.nextDisplayRefresh)) {
		return;
	}
	// if we shouldn't reset
	if ( force == 2 ) {
		force = 0;
	}

	// do motd updates here too
	trap_Cvar_VariableStringBuffer( "cl_motdString", uiInfo.serverStatus.motd, sizeof(uiInfo.serverStatus.motd) );
	len = (int)strlen(uiInfo.serverStatus.motd);
	if (len == 0) {
		strcpy(uiInfo.serverStatus.motd, "Welcome to JK2MP!");
		len = (int)strlen(uiInfo.serverStatus.motd);
	} 
	if (len != uiInfo.serverStatus.motdLen) {
		uiInfo.serverStatus.motdLen = len;
		uiInfo.serverStatus.motdWidth = -1;
	} 

	if (force) {
		numinvisible = 0;
		// clear number of displayed servers
		uiInfo.serverStatus.numDisplayServers = 0;
		uiInfo.serverStatus.numPlayersOnServers = 0;
		// set list box index to zero
		Menu_SetFeederSelection(NULL, FEEDER_SERVERS, 0, NULL);
		// mark all servers as visible so we store ping updates for them
		trap_LAN_MarkServerVisible(ui_netSource.integer, -1, qtrue);
	}

	// get the server count (comes from the master)
	count = trap_LAN_GetServerCount(ui_netSource.integer);
	if (count == -1 || (ui_netSource.integer == AS_LOCAL && count == 0) ) {
		// still waiting on a response from the master
		uiInfo.serverStatus.numDisplayServers = 0;
		uiInfo.serverStatus.numPlayersOnServers = 0;
		uiInfo.serverStatus.nextDisplayRefresh = uiInfo.uiDC.realTime + 500;
		return;
	}

	visible = qfalse;
	for (i = 0; i < count; i++) {
		// if we already got info for this server
		if (!trap_LAN_ServerIsVisible(ui_netSource.integer, i)) {
			continue;
		}
		visible = qtrue;
		// get the ping for this server
		ping = trap_LAN_GetServerPing(ui_netSource.integer, i);
		if (ping > 0 || ui_netSource.integer == AS_FAVORITES) {
			// botfilter
			int realPlayers;
			int clients;
			int bots;

			trap_LAN_GetServerInfo(ui_netSource.integer, i, info, MAX_STRING_CHARS);

			clients = atoi(Info_ValueForKey(info, "clients"));
			bots = atoi(Info_ValueForKey(info, "bots"));

			if (trap_Cvar_VariableValue("ui_botfilter")) {
				realPlayers = clients - bots;
			} else {
				realPlayers = clients;
			}

			if (ui_browserShowEmpty.integer == 0) {
				if (realPlayers == 0) {
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}
			}

			if (ui_browserShowFull.integer == 0) {
				maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
				if (clients == maxClients) {
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}
			}

			if (uiInfo.joinGameTypes[ui_joinGameType.integer].gtEnum != -1) {
				game = atoi(Info_ValueForKey(info, "gametype"));
				if (game != uiInfo.joinGameTypes[ui_joinGameType.integer].gtEnum) {
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}
			}
				
			if (ui_serverFilterType.integer > 0) {
				if ( !uiInfo.inGameLoad ) {
					mvversion_t gameVersion; // Using the "gameVersion" now to support seperate lists for 1.02, 1.03 and 1.04

					if (ui_serverFilterType.integer == 1)
						gameVersion = VERSION_1_02;
					else if (ui_serverFilterType.integer == 2)
						gameVersion = VERSION_1_03;
					else
						gameVersion = VERSION_1_04;

					if (atoi(Info_ValueForKey(info, "gameVersion")) != (int)gameVersion) {
						trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
						continue;
					}
				}
				else if (Q_stricmp(Info_ValueForKey(info, "game"), serverFilters[ui_serverFilterType.integer].value) != 0) {
					trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
					continue;
				}
			}
			// make sure we never add a favorite server twice
			if (ui_netSource.integer == AS_FAVORITES) {
				UI_RemoveServerFromDisplayList(i);
			}
			// insert the server into the list
			UI_BinaryServerInsertion(i);
			uiInfo.serverStatus.numPlayersOnServers += realPlayers;
			// done with this server
			if (ping > 0) {
				trap_LAN_MarkServerVisible(ui_netSource.integer, i, qfalse);
				numinvisible++;
			}
		}
	}

	uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime;

	// if there were no servers visible for ping updates
	if (!visible) {
//		UI_StopServerRefresh();
//		uiInfo.serverStatus.nextDisplayRefresh = 0;
	}
}

typedef struct
{
	const char *name, *altName;
} serverStatusCvar_t;

static const serverStatusCvar_t serverStatusCvars[] = {
	{"sv_hostname", "Name"},
	{"Address", ""},
	{"gamename", "Game name"},
	{"g_gametype", "Game type"},
	{"mapname", "Map"},
	{"version", ""},
	{"JK2MV", ""},
	{"protocol", ""},
	{"timelimit", ""},
	{"fraglimit", ""},
	{NULL, NULL}
};

/*
==================
UI_SortServerStatusInfo
==================
*/
static void UI_SortServerStatusInfo( serverStatusInfo_t *info ) {
	int i, j, index;
	const char *tmp1, *tmp2;

	// FIXME: if "gamename" == "base" or "missionpack" then
	// replace the gametype number by FFA, CTF etc.
	//
	index = 0;
	for (i = 0; serverStatusCvars[i].name; i++) {
		for (j = 0; j < info->numLines; j++) {
			if ( !info->lines[j][1] || info->lines[j][1][0] ) {
				continue;
			}
			if ( !Q_stricmp(serverStatusCvars[i].name, info->lines[j][0]) ) {
				// swap lines
				tmp1 = info->lines[index][0];
				tmp2 = info->lines[index][3];
				info->lines[index][0] = info->lines[j][0];
				info->lines[index][3] = info->lines[j][3];
				info->lines[j][0] = tmp1;
				info->lines[j][3] = tmp2;
				//
				if ( strlen(serverStatusCvars[i].altName) ) {
					info->lines[index][0] = serverStatusCvars[i].altName;
				}
				index++;
			}
		}
	}
}

/*
==================
UI_GetServerStatusInfo
==================
*/
static int UI_GetServerStatusInfo( const char *serverAddress, serverStatusInfo_t *info ) {
	char *p, *score, *ping, *name;
	int i;
	size_t len;

	if (!info) {
		trap_LAN_ServerStatus( serverAddress, NULL, 0);
		return qfalse;
	}
	memset(info, 0, sizeof(*info));
	if ( trap_LAN_ServerStatus( serverAddress, info->text, sizeof(info->text)) ) {
		Q_strncpyz(info->address, serverAddress, sizeof(info->address));
		p = info->text;
		info->numLines = 0;
		info->lines[info->numLines][0] = "Address";
		info->lines[info->numLines][1] = "";
		info->lines[info->numLines][2] = "";
		info->lines[info->numLines][3] = info->address;
		info->numLines++;
		// get the cvars
		while (p && *p) {
			p = strchr(p, '\\');
			if (!p) break;
			*p++ = '\0';
			if (*p == '\\')
				break;
			info->lines[info->numLines][0] = p;
			info->lines[info->numLines][1] = "";
			info->lines[info->numLines][2] = "";
			p = strchr(p, '\\');
			if (!p) break;
			*p++ = '\0';
			info->lines[info->numLines][3] = p;

			info->numLines++;
			if (info->numLines >= MAX_SERVERSTATUS_LINES)
				break;
		}
		// get the player list
		if (info->numLines < MAX_SERVERSTATUS_LINES-3) {
			// empty line
			info->lines[info->numLines][0] = "";
			info->lines[info->numLines][1] = "";
			info->lines[info->numLines][2] = "";
			info->lines[info->numLines][3] = "";
			info->numLines++;
			// header
			info->lines[info->numLines][0] = "num";
			info->lines[info->numLines][1] = "score";
			info->lines[info->numLines][2] = "ping";
			info->lines[info->numLines][3] = "name";
			info->numLines++;
			// parse players
			i = 0;
			len = 0;
			while (p && *p) {
				if (*p == '\\')
					*p++ = '\0';
				if (!p)
					break;
				score = p;
				p = strchr(p, ' ');
				if (!p)
					break;
				*p++ = '\0';
				ping = p;
				p = strchr(p, ' ');
				if (!p)
					break;
				*p++ = '\0';
				name = p;
				Com_sprintf(&info->pings[len], sizeof(info->pings)-len, "%d", i);
				info->lines[info->numLines][0] = &info->pings[len];
				len += strlen(&info->pings[len]) + 1;
				info->lines[info->numLines][1] = score;
				info->lines[info->numLines][2] = ping;
				info->lines[info->numLines][3] = name;
				info->numLines++;
				if (info->numLines >= MAX_SERVERSTATUS_LINES)
					break;
				p = strchr(p, '\\');
				if (!p)
					break;
				*p++ = '\0';
				//
				i++;
			}
		}
		UI_SortServerStatusInfo( info );
		return qtrue;
	}
	return qfalse;
}

/*
==================
stristr
==================
*/
static char *stristr(char *str, char *charset) {
	int i;

	while(*str) {
		for (i = 0; charset[i] && str[i]; i++) {
			if (toupper(charset[i]) != toupper(str[i])) break;
		}
		if (!charset[i]) return str;
		str++;
	}
	return NULL;
}

/*
==================
UI_BuildFindPlayerList
==================
*/
static void UI_BuildFindPlayerList(qboolean force) {
	static int numFound, numTimeOuts;
	int i, j, resend;
	serverStatusInfo_t info;
	char name[MAX_NAME_LENGTH+2];
	char infoString[MAX_STRING_CHARS];

	if (!force) {
		if (!uiInfo.nextFindPlayerRefresh || uiInfo.nextFindPlayerRefresh > uiInfo.uiDC.realTime) {
			return;
		}
	}
	else {
		memset(&uiInfo.pendingServerStatus, 0, sizeof(uiInfo.pendingServerStatus));
		uiInfo.numFoundPlayerServers = 0;
		uiInfo.currentFoundPlayerServer = 0;
		trap_Cvar_VariableStringBuffer( "ui_findPlayer", uiInfo.findPlayerName, sizeof(uiInfo.findPlayerName));
		Q_CleanStr(uiInfo.findPlayerName, (qboolean)(jk2startversion == VERSION_1_02));
		// should have a string of some length
		if (!strlen(uiInfo.findPlayerName)) {
			uiInfo.nextFindPlayerRefresh = 0;
			return;
		}
		// set resend time
		resend = ui_serverStatusTimeOut.integer / 2 - 10;
		if (resend < 50) {
			resend = 50;
		}
		trap_Cvar_Set("cl_serverStatusResendTime", va("%d", resend));
		// reset all server status requests
		trap_LAN_ServerStatus( NULL, NULL, 0);
		//
		uiInfo.numFoundPlayerServers = 1;
		Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1],
						sizeof(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1]),
							"searching %d...", uiInfo.pendingServerStatus.num);
		numFound = 0;
		numTimeOuts++;
	}
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		// if this pending server is valid
		if (uiInfo.pendingServerStatus.server[i].valid) {
			// try to get the server status for this server
			if (UI_GetServerStatusInfo( uiInfo.pendingServerStatus.server[i].adrstr, &info ) ) {
				//
				numFound++;
				// parse through the server status lines
				for (j = 0; j < info.numLines; j++) {
					// should have ping info
					if ( !info.lines[j][2] || !info.lines[j][2][0] ) {
						continue;
					}
					// clean string first
					Q_strncpyz(name, info.lines[j][3], sizeof(name));
					Q_CleanStr(name, (qboolean)(jk2startversion == VERSION_1_02));
					// if the player name is a substring
					if (stristr(name, uiInfo.findPlayerName)) {
						// add to found server list if we have space (always leave space for a line with the number found)
						if (uiInfo.numFoundPlayerServers < MAX_FOUNDPLAYER_SERVERS-1) {
							//
							Q_strncpyz(uiInfo.foundPlayerServerAddresses[uiInfo.numFoundPlayerServers-1],
										uiInfo.pendingServerStatus.server[i].adrstr,
											sizeof(uiInfo.foundPlayerServerAddresses[0]));
							Q_strncpyz(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1],
										uiInfo.pendingServerStatus.server[i].name,
											sizeof(uiInfo.foundPlayerServerNames[0]));
							uiInfo.numFoundPlayerServers++;
						}
						else {
							// can't add any more so we're done
							uiInfo.pendingServerStatus.num = uiInfo.serverStatus.numDisplayServers;
						}
					}
				}
				Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1],
								sizeof(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1]),
									"searching %d/%d...", uiInfo.pendingServerStatus.num, numFound);
				// retrieved the server status so reuse this spot
				uiInfo.pendingServerStatus.server[i].valid = qfalse;
			}
		}
		// if empty pending slot or timed out
		if (!uiInfo.pendingServerStatus.server[i].valid ||
			uiInfo.pendingServerStatus.server[i].startTime < uiInfo.uiDC.realTime - ui_serverStatusTimeOut.integer) {
			if (uiInfo.pendingServerStatus.server[i].valid) {
				numTimeOuts++;
			}
			// reset server status request for this address
			UI_GetServerStatusInfo( uiInfo.pendingServerStatus.server[i].adrstr, NULL );
			// reuse pending slot
			uiInfo.pendingServerStatus.server[i].valid = qfalse;
			// if we didn't try to get the status of all servers in the main browser yet
			if (uiInfo.pendingServerStatus.num < uiInfo.serverStatus.numDisplayServers) {
				uiInfo.pendingServerStatus.server[i].startTime = uiInfo.uiDC.realTime;
				trap_LAN_GetServerAddressString(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.pendingServerStatus.num],
							uiInfo.pendingServerStatus.server[i].adrstr, sizeof(uiInfo.pendingServerStatus.server[i].adrstr));
				trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[uiInfo.pendingServerStatus.num], infoString, sizeof(infoString));
				Q_strncpyz(uiInfo.pendingServerStatus.server[i].name, Info_ValueForKey(infoString, "hostname"), sizeof(uiInfo.pendingServerStatus.server[0].name));
				uiInfo.pendingServerStatus.server[i].valid = qtrue;
				uiInfo.pendingServerStatus.num++;
				Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1],
								sizeof(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1]),
									"searching %d/%d...", uiInfo.pendingServerStatus.num, numFound);
			}
		}
	}
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if (uiInfo.pendingServerStatus.server[i].valid) {
			break;
		}
	}
	// if still trying to retrieve server status info
	if (i < MAX_SERVERSTATUSREQUESTS) {
		uiInfo.nextFindPlayerRefresh = uiInfo.uiDC.realTime + 25;
	}
	else {
		// add a line that shows the number of servers found
		if (!uiInfo.numFoundPlayerServers) 
		{
			Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1], sizeof(uiInfo.foundPlayerServerAddresses[0]), "no servers found");
		}
		else 
		{
			trap_SP_GetStringTextString("MENUS3_SERVERS_FOUNDWITH", holdSPString, sizeof(holdSPString));
			Com_sprintf(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1], sizeof(uiInfo.foundPlayerServerAddresses[0]),
						holdSPString, uiInfo.numFoundPlayerServers-1,
						uiInfo.numFoundPlayerServers == 2 ? "":"s", uiInfo.findPlayerName);
		}
		uiInfo.nextFindPlayerRefresh = 0;
		// show the server status info for the selected server
		UI_FeederSelection(FEEDER_FINDPLAYER, uiInfo.currentFoundPlayerServer);
	}
}

/*
==================
UI_BuildServerStatus
==================
*/
static void UI_BuildServerStatus(qboolean force) {

	if (uiInfo.nextFindPlayerRefresh) {
		return;
	}
	if (!force) {
		if (!uiInfo.nextServerStatusRefresh || uiInfo.nextServerStatusRefresh > uiInfo.uiDC.realTime) {
			return;
		}
	}
	else {
		Menu_SetFeederSelection(NULL, FEEDER_SERVERSTATUS, 0, NULL);
		uiInfo.serverStatusInfo.numLines = 0;
		// reset all server status requests
		trap_LAN_ServerStatus( NULL, NULL, 0);
	}
	if (uiInfo.serverStatus.currentServer < 0 || uiInfo.serverStatus.currentServer > uiInfo.serverStatus.numDisplayServers || uiInfo.serverStatus.numDisplayServers == 0) {
		return;
	}
	if (UI_GetServerStatusInfo( uiInfo.serverStatusAddress, &uiInfo.serverStatusInfo ) ) {
		uiInfo.nextServerStatusRefresh = 0;
		UI_GetServerStatusInfo( uiInfo.serverStatusAddress, NULL );
	}
	else {
		uiInfo.nextServerStatusRefresh = uiInfo.uiDC.realTime + 500;
	}
}

/*
==================
UI_FeederCount
==================
*/
static int UI_FeederCount(float feederID) 
{
	switch ( (int)feederID )
	{
//		case FEEDER_HEADS:
//			return UI_HeadCountByTeam();

		case FEEDER_Q3HEADS:
			return UI_HeadCountByColor();

		case FEEDER_FORCECFG:
			if (uiForceSide == FORCE_LIGHTSIDE)
			{
				return uiInfo.forceConfigCount-uiInfo.forceConfigLightIndexBegin;
			}
			else
			{
				return uiInfo.forceConfigLightIndexBegin+1;
			}
			//return uiInfo.forceConfigCount;

		case FEEDER_CINEMATICS:
			return uiInfo.movieCount;

		case FEEDER_MAPS:
		case FEEDER_ALLMAPS:
			return UI_MapCountByGameType(feederID == FEEDER_MAPS ? qtrue : qfalse);
	
		case FEEDER_SERVERS:
			return uiInfo.serverStatus.numDisplayServers;
	
		case FEEDER_SERVERSTATUS:
			return uiInfo.serverStatusInfo.numLines;
	
		case FEEDER_FINDPLAYER:
			return uiInfo.numFoundPlayerServers;

		case FEEDER_PLAYER_LIST:
			if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) 
			{
				uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
				UI_BuildPlayerList();
			}
			return uiInfo.playerCount;

		case FEEDER_TEAM_LIST:
			if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) 
			{
				uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
				UI_BuildPlayerList();
			}
			return uiInfo.myTeamCount;

		case FEEDER_MODS:
			return uiInfo.modCount;

		case FEEDER_DOWNLOADS:
			return uiInfo.downloadsCount;


		case FEEDER_DEMOS:
			return uiInfo.demoCount;
	}

	return 0;
}

static const char *UI_SelectedMap(int index, int *actual) {
	int i, c;
	c = 0;
	*actual = 0;

	for (i = 0; i < uiInfo.mapCount; i++) {
		if (uiInfo.mapList[i].active) {
			if (c == index) {
				*actual = i;
				return uiInfo.mapList[i].mapName;
			} else {
				c++;
			}
		}
	}
	return "";
}

static const char *UI_SelectedHead(int index, int *actual) {
	int i, c;
	c = 0;
	*actual = 0;
	for (i = 0; i < uiInfo.characterCount; i++) {
		if (uiInfo.characterList[i].active) {
			if (c == index) {
				*actual = i;
				return uiInfo.characterList[i].name;
			} else {
				c++;
			}
		}
	}
	return "";
}

/*
==================
UI_HeadCountByColor
==================
*/
static const char *UI_SelectedTeamHead(int index, int *actual) {
	q3Head_t *head;
	int i,c=0;
	*actual = 0;

	// Count each head with this color

	for (i=0; i<uiInfo.q3HeadCount; i++)
	{
		head = UI_GetHeadByIndex(i);
		if (head && head->name && UI_HeadBelongsToCurrentTeamColor(head))
		{
			if (c==index)
			{
				*actual = i;
				return head->name;
			}
			else
			{
				c++;
			}
		}
	}

	*actual = -1;
	return "";
}

const char *UI_GetModelWithSkin(const char *model) {
	static char modelWithSkin[MAX_STRING_CHARS];
	int currentColor;

	if ( serverGameType >= GT_TEAM ) {
		int myTeam = (int)trap_Cvar_VariableValue("ui_myteam");
		const char *ptr;
		if ( strstr(model, "/blue") ) {
			currentColor = SKINCOLOR_BLUE;
		} else if ( strstr(model, "/red") ) {
			currentColor = SKINCOLOR_RED;
		} else if ( strstr(model, "/default") ) {
			currentColor = SKINCOLOR_DEFAULT;
		} else {
			currentColor = SKINCOLOR_OTHER;
		}

		if ( currentColor == uiSkinColor || (myTeam != TEAM_RED && myTeam != TEAM_BLUE) ) return model;
		ptr = strchr(model, '/');

		if ( !ptr ) Q_strncpyz( modelWithSkin, model, sizeof(modelWithSkin) );
		else if ( (unsigned long)(ptr-model+1) <= sizeof(modelWithSkin) ) Q_strncpyz( modelWithSkin, model, ptr-model+1 );
		else return model; // Error

		Q_strcat( modelWithSkin, sizeof(modelWithSkin), UI_GetSkinSuffixForTeamColor() );
	} else {
		if ( strchr(model, '/') ) return model;
		Q_strncpyz( modelWithSkin, model, sizeof(modelWithSkin) );
		Q_strcat( modelWithSkin, sizeof(modelWithSkin), "/default" );
	}

	return modelWithSkin;
}

int UI_HeadIndexForModel(const char *model) {
	q3Head_t *head = uiInfo.q3Heads;
	int c = 0;

	if ( !model || !model[0] ) {
		return -1;
	}

	while ( head ) {
		if ( UI_HeadBelongsToCurrentTeamColor(head) ) {
			if (!Q_stricmp(head->name, model)) {
				return c;
			}
			c++;
		}
		head = head->next;
	}
	return -1;
}

qboolean UI_SetTeamColorFromModel(const char *model) {
	int oldSkinColor = uiSkinColor;

	if ( strstr(model, "/blue") ) {
		uiSkinColor = SKINCOLOR_BLUE;
	} else if ( strstr(model, "/red") ) {
		uiSkinColor = SKINCOLOR_RED;
	} else if ( strstr(model, "/default") ) {
		uiSkinColor = SKINCOLOR_DEFAULT;
	} else {
		uiSkinColor = SKINCOLOR_OTHER;
	}

	if ( uiSkinColor != oldSkinColor ) {
		return qtrue;
	}
	return qfalse;
}

const char *UI_GetModelWithTeamColor(const char *model) {
	static char newModel[MAX_STRING_CHARS];
	const char *teamname = UI_GetSkinSuffixForTeamColor();
	char *ptr;

	ptr = strchr(model, '/');

	if ( model[0] && ptr ) {
		Q_strncpyz( newModel, model, ptr-model+1 );
		Q_strcat( newModel, sizeof(newModel), teamname );
		return newModel;
	}

	return model;
}


static int UI_GetIndexFromSelection(int actual) {
	int i, c;
	c = 0;
	for (i = 0; i < uiInfo.mapCount; i++) {
		if (uiInfo.mapList[i].active) {
			if (i == actual) {
				return c;
			}
				c++;
		}
	}
  return 0;
}

static void UI_UpdatePendingPings() { 
	trap_LAN_ResetPings(ui_netSource.integer);
	uiInfo.serverStatus.refreshActive = qtrue;
	uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;

}

static const char *UI_FeederItemText(float feederID, int index, int column, 
	qhandle_t *handle1, qhandle_t *handle2, qhandle_t *handle3, qhandle_t *handle4, qhandle_t *handle5, qhandle_t *handle6) {
	static char info[MAX_STRING_CHARS];
	static char hostname[1024];
	static char clientBuff[32];
	static char needPass[32];
	static int lastColumn = -1;
	static int lastTime = 0;
	*handle1 = *handle2 = *handle3 = *handle4 = -1;
	if (feederID == FEEDER_HEADS) {
		int actual;
		return UI_SelectedHead(index, &actual);
	} else if (feederID == FEEDER_Q3HEADS) {
		int actual;
		return UI_SelectedTeamHead(index, &actual);
	} else if (feederID == FEEDER_FORCECFG) {
		if (index >= 0 && index < uiInfo.forceConfigCount) {
			if (index == 0)
			{ //always show "custom"
				return uiInfo.forceConfigNames[index];
			}
			else
			{
				if (uiForceSide == FORCE_LIGHTSIDE)
				{
					index += uiInfo.forceConfigLightIndexBegin;
					if (index < 0)
					{
						return NULL;
					}
					if (index >= uiInfo.forceConfigCount)
					{
						return NULL;
					}
					return uiInfo.forceConfigNames[index];
				}
				else if (uiForceSide == FORCE_DARKSIDE)
				{
					index += uiInfo.forceConfigDarkIndexBegin;
					if (index < 0)
					{
						return NULL;
					}
					if (index > uiInfo.forceConfigLightIndexBegin)
					{ //dark gets read in before light
						return NULL;
					}
					if (index >= uiInfo.forceConfigCount)
					{
						return NULL;
					}
					return uiInfo.forceConfigNames[index];
				}
				else
				{
					return NULL;
				}
			}
		}
	} else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS) {
		int actual;
		return UI_SelectedMap(index, &actual);
	} else if (feederID == FEEDER_SERVERS) {
		if (index >= 0 && index < uiInfo.serverStatus.numDisplayServers) {
			int ping, game;
			if (lastColumn != column || lastTime > uiInfo.uiDC.realTime + 5000) {
				trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[index], info, MAX_STRING_CHARS);
				lastColumn = column;
				lastTime = uiInfo.uiDC.realTime;
			}
			ping = atoi(Info_ValueForKey(info, "ping"));
			if (ping == -1) {
				// if we ever see a ping that is out of date, do a server refresh
				// UI_UpdatePendingPings();
			}
			switch (column) {
				case SORT_HOST : 
					if (ping <= 0) {
						return Info_ValueForKey(info, "addr");
					} else {
						int gametype = 0;
						int int_protocol;
						int int_gameVersion;
						//check for password
						if ( atoi(Info_ValueForKey(info, "needpass")) )
						{
							*handle3 = trap_R_RegisterShaderNoMip( "gfx/menus/needpass" );
						}

						// serverversion icon
						if ( handle5 )
						{ // protocol
							int_protocol = atoi(Info_ValueForKey(info, "protocol"));
							switch ((mvprotocol_t)int_protocol) {
							case PROTOCOL15:
							case PROTOCOL16:
								*handle5 = (mvprotocol_t)int_protocol;
								break;
							default:
								*handle5 = PROTOCOL_UNDEF;
								break;
							}
						}

						if ( handle6 )
						{ // gameVersion
							int_gameVersion = atoi(Info_ValueForKey(info, "gameVersion"));
							switch((mvversion_t)int_gameVersion) {
							case VERSION_1_02:
							case VERSION_1_03:
							case VERSION_1_04:
								*handle6 = (mvversion_t)int_gameVersion;
								break;
							default:
								*handle6 = VERSION_UNDEF;
								break;
							}
						}

						// do this only if no gameversion is selected
						if (!ui_serverFilterType.integer && handle5 && handle6) {
							if ( *handle5 == PROTOCOL16 || *handle6 == VERSION_1_04 ) {
								*handle4 = trap_R_RegisterShaderNoMip("gfx/menus/srv_104");
							} else {
								if ( *handle6 == VERSION_1_03 )
									*handle4 = trap_R_RegisterShaderNoMip("gfx/menus/srv_103");
								else if ( *handle6 == VERSION_1_02 )
									*handle4 = trap_R_RegisterShaderNoMip("gfx/menus/srv_102");
								else
									*handle4 = -1;
							}
						}

						//check for saberonly and restricted force powers
						gametype = atoi(Info_ValueForKey(info, "gametype"));
						if ( gametype != GT_JEDIMASTER )
						{
							qboolean saberOnly = qtrue;
							qboolean restrictedForce = qfalse;
							qboolean allForceDisabled = qfalse;
							int wDisable, i = 0;

							//check force
							restrictedForce = atoi(Info_ValueForKey(info, "fdisable"));
							if ( UI_AllForceDisabled( restrictedForce ) )
							{//all force powers are disabled
								allForceDisabled = qtrue;
								*handle2 = trap_R_RegisterShaderNoMip( "gfx/menus/noforce" );
							}
							else if ( restrictedForce )
							{//at least one force power is disabled
								*handle2 = trap_R_RegisterShaderNoMip( "gfx/menus/forcerestrict" );
							}
							
							//check weaps
							wDisable = atoi(Info_ValueForKey(info, "wdisable"));

							while ( i < WP_NUM_WEAPONS )
							{
								if ( !(wDisable & (1 << i)) && i != WP_SABER && i != WP_NONE )
								{
									saberOnly = qfalse;
								}

								i++;
							}
							if ( saberOnly )
							{
								*handle1 = trap_R_RegisterShaderNoMip( "gfx/menus/saberonly" );
							}
							else if ( atoi(Info_ValueForKey(info, "truejedi")) != 0 )
							{
								if ( gametype != GT_HOLOCRON 
									&& gametype != GT_JEDIMASTER 
									&& !saberOnly 
									&& !allForceDisabled )
								{//truejedi is on and allowed in this mode
									*handle1 = trap_R_RegisterShaderNoMip( "gfx/menus/truejedi" );
								}
							}
						}
						if ( ui_netSource.integer == AS_LOCAL ) {
							Com_sprintf( hostname, sizeof(hostname), "%s [%s]",
											Info_ValueForKey(info, "hostname"),
											netnames[atoi(Info_ValueForKey(info, "nettype"))] );
							return hostname;
						}
						else {
							if (atoi(Info_ValueForKey(info, "sv_allowAnonymous")) != 0) {				// anonymous server
								Com_sprintf( hostname, sizeof(hostname), "(A) %s",
												Info_ValueForKey(info, "hostname"));
							} else {
								Com_sprintf( hostname, sizeof(hostname), "%s",
												Info_ValueForKey(info, "hostname"));
							}
							return hostname;
						}
					}
				case SORT_MAP : 
					return Info_ValueForKey(info, "mapname");
				case SORT_CLIENTS : 
				{
					// botfilter
					int clients = atoi(Info_ValueForKey(info, "clients"));
					int bots = atoi(Info_ValueForKey(info, "bots"));
					int maxclients = atoi(Info_ValueForKey(info, "sv_maxclients"));

					int realPlayers;
					if (trap_Cvar_VariableValue("ui_botfilter")) {
						realPlayers = clients - bots;
					} else {
						realPlayers = clients;
					}

					Com_sprintf(clientBuff, sizeof(clientBuff), "%i (%i)", realPlayers, maxclients);
					return clientBuff;
				}
				case SORT_GAME : 
					game = atoi(Info_ValueForKey(info, "gametype"));
					if (game >= 0 && game < numTeamArenaGameTypes) {
						strcpy(needPass,teamArenaGameTypes[game]);
					} else {
						if (ping <= 0)
						{
							strcpy(needPass,"Inactive");
						}
						strcpy(needPass,"Unknown");
					}

					return needPass;
				case SORT_PING : 
					if (ping <= 0) {
						return "...";
					} else {
						return Info_ValueForKey(info, "ping");
					}
			}
		}
	} else if (feederID == FEEDER_SERVERSTATUS) {
		if ( index >= 0 && index < uiInfo.serverStatusInfo.numLines ) {
			if ( column >= 0 && column < 4 ) {
				return uiInfo.serverStatusInfo.lines[index][column];
			}
		}
	} else if (feederID == FEEDER_FINDPLAYER) {
		if ( index >= 0 && index < uiInfo.numFoundPlayerServers ) {
			//return uiInfo.foundPlayerServerAddresses[index];
			return uiInfo.foundPlayerServerNames[index];
		}
	} else if (feederID == FEEDER_PLAYER_LIST) {
		if (index >= 0 && index < uiInfo.playerCount) {
			return uiInfo.playerNames[index];
		}
	} else if (feederID == FEEDER_TEAM_LIST) {
		if (index >= 0 && index < uiInfo.myTeamCount) {
			return uiInfo.teamNames[index];
		}
	} else if (feederID == FEEDER_MODS) {
		if (index >= 0 && index < uiInfo.modCount) {
			if (uiInfo.modList[index].modDescr && *uiInfo.modList[index].modDescr) {
				return uiInfo.modList[index].modDescr;
			} else {
				return uiInfo.modList[index].modName;
			}
		}
	} else if (feederID == FEEDER_DOWNLOADS) {
		if (index >= 0 && index < uiInfo.downloadsCount) {
			return uiInfo.downloadsList[index].name;
		}
	} else if (feederID == FEEDER_CINEMATICS) {
		if (index >= 0 && index < uiInfo.movieCount) {
			return uiInfo.movieList[index];
		}
	} else if (feederID == FEEDER_DEMOS) {
		if (index >= 0 && index < uiInfo.demoCount) {
			return uiInfo.demoList[index];
		}
	} 
	return "";
}

void UI_FeederScrollTo(float feederId, int scrollTo) {
	menuDef_t *menu = Menu_GetFocused();
	int i;

	if (menu) {
		for (i = 0; i < menu->itemCount; i++) {
			if (menu->items[i]->special == feederId) {
				itemDef_t *item = menu->items[i];
				listBoxDef_t *listPtr = (listBoxDef_t*)item->typeData;

				int endPos = Item_ListBox_MaxScroll(item);
				int viewmax = Display_GetContext()->feederCount(item->special) - endPos;

				if ( viewmax > 1 ) {
					if ( scrollTo >= viewmax/2 )
						scrollTo -= viewmax/2-1;
					else
						scrollTo = 0;
				}

				if ( scrollTo > endPos )
					listPtr->startPos = endPos;
				else
					listPtr->startPos = scrollTo;

				break;
			}
		}
	}
}

qboolean uiUpdateModel = qtrue;
static qhandle_t UI_FeederItemImage(float feederID, int index) {
	if (feederID == FEEDER_HEADS) 
	{
		int actual;
		UI_SelectedHead(index, &actual);
		index = actual;
		if (index >= 0 && index < uiInfo.characterCount) 
		{
			if (uiInfo.characterList[index].headImage == -1) 
			{
				uiInfo.characterList[index].headImage = trap_R_RegisterShaderNoMip(uiInfo.characterList[index].imageName);
			}
			return uiInfo.characterList[index].headImage;
		}
	} 
	else if (feederID == FEEDER_Q3HEADS) 
	{
		int actual;
		UI_SelectedTeamHead(index, &actual);
		index = actual;

		if (index >= 0 && index < uiInfo.q3HeadCount)
		{ //we want it to load them as it draws them, like the TA feeder
		      //return uiInfo.q3HeadIcons[index];
			q3Head_t *head;
			const char *playerModel = NULL;
			static int selModel;

			if (uiUpdateModel)
			{
				if (serverGameType >= GT_TEAM)
				{
					playerModel = UI_GetModelWithSkin(ui_team_model.string);
					if ( UI_SetTeamColorFromModel(playerModel))
					{ // Also doing this for team gametypes now, as spectators would otherwise get default skins.
					  // This should generally be enough for proper skin selection (if team_model is set to a team skin) unless
					  // someone has a custom skin that only exists for one of the teams. In that case they have to adjust their
					  // skin after joining the team, cause we don't know their team, yet.
						uiInfo.q3SelectedHead = -1;
					}
				}
				else
				{
					playerModel = UI_GetModelWithSkin(ui_model.string);
					if (UI_SetTeamColorFromModel(playerModel))
					{
						uiInfo.q3SelectedHead = -1;
					}
				}
				selModel = UI_HeadIndexForModel(playerModel);
				uiUpdateModel = qfalse;
			}

			//if ( selModel == -1 ) selModel = trap_Cvar_VariableValue("ui_selectedModelIndex");
			if (selModel == -1 && playerModel)
			{
				static q3Head_t noIconHead;
				static char noIconHeadName[128];

				if (!noIconHead.name)
				{
					noIconHead.next = NULL;
					noIconHead.icon = trap_R_RegisterShaderNoMip("menu/art/unknownmap");
					noIconHead.name = noIconHeadName;

					UI_InsertHeadRaw(&noIconHead);
				}

				Q_strncpyz( noIconHeadName, playerModel, sizeof(noIconHeadName) );
				selModel = UI_HeadIndexForModel(playerModel);
			}

			if (selModel != -1)
			{
				if (uiInfo.q3SelectedHead != selModel)
				{
					uiInfo.q3SelectedHead = selModel;
					//UI_FeederSelection(FEEDER_Q3HEADS, uiInfo.q3SelectedHead);
					Menu_SetFeederSelection(NULL, FEEDER_Q3HEADS, selModel, NULL);

					// Make sure the model is in view
					UI_FeederScrollTo(FEEDER_Q3HEADS, selModel);
				}
			}

			head = UI_GetHeadByIndex(index);
			if ( !head ) return 0;
			if ( !head->icon && head->name )
			{ //this isn't the best way of doing this I guess, but I didn't want a whole seperate string array
			  //for storing shader names. I can't just replace q3HeadNames with the shader name, because we
			  //print what's in q3HeadNames and the icon name would look funny.
				char iconNameFromSkinName[256];
				int i = 0;
				int skinPlace;

				i = (int)strlen(head->name);

				while (head->name[i] != '/')
				{
					i--;
				}

				i++;
				skinPlace = i; //remember that this is where the skin name begins

				//now, build a full path out of what's in q3HeadNames, into iconNameFromSkinName
				Com_sprintf(iconNameFromSkinName, sizeof(iconNameFromSkinName), "models/players/%s", head->name);

				i = (int)strlen(iconNameFromSkinName);

				while (iconNameFromSkinName[i] != '/')
				{
					i--;
				}
				
				i++;
				iconNameFromSkinName[i] = 0; //terminate, and append..
				Q_strcat(iconNameFromSkinName, 256, "icon_");

				//and now, for the final step, append the skin name from q3HeadNames onto the end of iconNameFromSkinName
				i = (int)strlen(iconNameFromSkinName);

				while (head->name[skinPlace])
				{
					iconNameFromSkinName[i] = head->name[skinPlace];
					i++;
					skinPlace++;
				}
				iconNameFromSkinName[i] = 0;

				//and now we are ready to register (thankfully this will only happen once)
				head->icon = trap_R_RegisterShaderNoMip(iconNameFromSkinName);
			}
			return head->icon;
		}
    }
	else if (feederID == FEEDER_ALLMAPS || feederID == FEEDER_MAPS) 
	{
		int actual;
		UI_SelectedMap(index, &actual);
		index = actual;
		if (index >= 0 && index < uiInfo.mapCount) {
			if (uiInfo.mapList[index].levelShot == -1) {
				uiInfo.mapList[index].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[index].imageName);
			}
			return uiInfo.mapList[index].levelShot;
		}
	}
  return 0;
}

qboolean UI_FeederSelection(float feederID, int index) {
	static char info[MAX_STRING_CHARS];
	if (feederID == FEEDER_HEADS) 
	{
		int actual;
		UI_SelectedHead(index, &actual);
		index = actual;
		if (index >= 0 && index < uiInfo.characterCount) 
		{
			trap_Cvar_Set( "team_model", va("%s", uiInfo.characterList[index].base));
			//trap_Cvar_Set( "team_headmodel", va("*%s", uiInfo.characterList[index].name)); 
			updateModel = qtrue;
		}
	} 
	else if (feederID == FEEDER_Q3HEADS) 
	{
		int actual;
		UI_SelectedTeamHead(index, &actual);
		uiInfo.q3SelectedHead = index;
		trap_Cvar_Set("ui_selectedModelIndex", va("%i", index));
		index = actual;
		if (index >= 0 && index < uiInfo.q3HeadCount) 
		{
			q3Head_t *head = UI_GetHeadByIndex( index );

			if ( head && head->name )
			{
				trap_Cvar_Set( "model", head->name);
				//trap_Cvar_Set( "headmodel", head->name);

				//Update team_model for now here also, because we're using a different team skin system
				trap_Cvar_Set( "team_model", head->name);
				//trap_Cvar_Set( "team_headmodel", head->name);
			}

			updateModel = qtrue;
		}
	} 
	else if (feederID == FEEDER_FORCECFG) 
	{
		int newindex = index;

		if (uiForceSide == FORCE_LIGHTSIDE)
		{
			newindex += uiInfo.forceConfigLightIndexBegin;
			if (newindex >= uiInfo.forceConfigCount)
			{
				return qfalse;
			}
		}
		else
		{ //else dark
			newindex += uiInfo.forceConfigDarkIndexBegin;
			if (newindex >= uiInfo.forceConfigCount || newindex > uiInfo.forceConfigLightIndexBegin)
			{ //dark gets read in before light
				return qfalse;
			}
		}

		if (index >= 0 && index < uiInfo.forceConfigCount) 
		{
				UI_ForceConfigHandle(uiInfo.forceConfigSelected, index);
				uiInfo.forceConfigSelected = index;
		}
	} 
	else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS) 
	{
		int actual, map;
		const char *checkValid = NULL;

		map = (feederID == FEEDER_ALLMAPS) ? ui_currentNetMap.integer : ui_currentMap.integer;
		if (uiInfo.mapList[map].cinematic >= 0) {
		  trap_CIN_StopCinematic(uiInfo.mapList[map].cinematic);
		  uiInfo.mapList[map].cinematic = -1;
		}
		checkValid = UI_SelectedMap(index, &actual);

		if (!checkValid || !checkValid[0])
		{ //this isn't a valid map to select, so reselect the current
			index = ui_mapIndex.integer;
			UI_SelectedMap(index, &actual);
		}

		trap_Cvar_Set("ui_mapIndex", va("%d", index));
		gUISelectedMap = index;
		ui_mapIndex.integer = index;

		if (feederID == FEEDER_MAPS) {
			ui_currentMap.integer = actual;
			trap_Cvar_Set("ui_currentMap", va("%d", actual));
		uiInfo.mapList[ui_currentMap.integer].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[ui_currentMap.integer].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent) );
			UI_LoadBestScores(uiInfo.mapList[ui_currentMap.integer].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum);
			//trap_Cvar_Set("ui_opponentModel", uiInfo.mapList[ui_currentMap.integer].opponentName);
			//updateOpponentModel = qtrue;
		} else {
			ui_currentNetMap.integer = actual;
			trap_Cvar_Set("ui_currentNetMap", va("%d", actual));
		uiInfo.mapList[ui_currentNetMap.integer].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent) );
		}

	} else if (feederID == FEEDER_SERVERS) {
		uiInfo.serverStatus.currentServer = index;
		trap_LAN_GetServerInfo(ui_netSource.integer, uiInfo.serverStatus.displayServers[index], info, MAX_STRING_CHARS);
		uiInfo.serverStatus.currentServerPreview = trap_R_RegisterShaderNoMip(va("levelshots/%s", Info_ValueForKey(info, "mapname")));
		if (uiInfo.serverStatus.currentServerCinematic >= 0) {
		  trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
			uiInfo.serverStatus.currentServerCinematic = -1;
		}
	} else if (feederID == FEEDER_SERVERSTATUS) {
		//
	} else if (feederID == FEEDER_FINDPLAYER) {
	  uiInfo.currentFoundPlayerServer = index;
	  //
	  if ( index < uiInfo.numFoundPlayerServers-1) {
			// build a new server status for this server
			Q_strncpyz(uiInfo.serverStatusAddress, uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer], sizeof(uiInfo.serverStatusAddress));
			Menu_SetFeederSelection(NULL, FEEDER_SERVERSTATUS, 0, NULL);
			UI_BuildServerStatus(qtrue);
	  }
	} else if (feederID == FEEDER_PLAYER_LIST) {
		uiInfo.playerIndex = index;
	} else if (feederID == FEEDER_TEAM_LIST) {
		uiInfo.teamIndex = index;
	} else if (feederID == FEEDER_MODS) {
		uiInfo.modIndex = index;
	} else if (feederID == FEEDER_DOWNLOADS) {
		uiInfo.downloadsIndex = index;
	} else if (feederID == FEEDER_CINEMATICS) {
		uiInfo.movieIndex = index;
		if (uiInfo.previewMovie >= 0) {
		  trap_CIN_StopCinematic(uiInfo.previewMovie);
		}
		uiInfo.previewMovie = -1;
	} else if (feederID == FEEDER_DEMOS) {
		uiInfo.demoIndex = index;
	}

	return qtrue;
}


static qboolean GameType_Parse(const char **p, qboolean join) {
	const char *token;

	token = COM_ParseExt(p, qtrue);

	if (token[0] != '{') {
		return qfalse;
	}

	if (join) {
		uiInfo.numJoinGameTypes = 0;
	} else {
		uiInfo.numGameTypes = 0;
	}

	while ( 1 ) {
		token = COM_ParseExt(p, qtrue);

		if (Q_stricmp(token, "}") == 0) {
			return qtrue;
		}

		if ( !token || token[0] == 0 ) {
			return qfalse;
		}

		if (token[0] == '{') {
			// two tokens per line, character name and sex
			if (join) {
				if (!String_Parse(p, &uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gameType) || !Int_Parse(p, &uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gtEnum)) {
					return qfalse;
				}
			} else {
				if (!String_Parse(p, &uiInfo.gameTypes[uiInfo.numGameTypes].gameType) || !Int_Parse(p, &uiInfo.gameTypes[uiInfo.numGameTypes].gtEnum)) {
					return qfalse;
				}
			}
    
			if (join) {
				if (uiInfo.numJoinGameTypes < MAX_GAMETYPES) {
					uiInfo.numJoinGameTypes++;
				} else {
					Com_Printf("Too many net game types, last one replace!\n");
				}		
			} else {
				if (uiInfo.numGameTypes < MAX_GAMETYPES) {
					uiInfo.numGameTypes++;
				} else {
					Com_Printf("Too many game types, last one replace!\n");
				}		
			}
     
			token = COM_ParseExt((const char **)p, qtrue);
			if (token[0] != '}') {
				return qfalse;
			}
		}
	}
	return qfalse;
}

static qboolean MapList_Parse(const char **p) {
	const char *token;

	token = COM_ParseExt(p, qtrue);

	if (token[0] != '{') {
		return qfalse;
	}

	uiInfo.mapCount = 0;

	while ( 1 ) {
		token = COM_ParseExt(p, qtrue);

		if (Q_stricmp(token, "}") == 0) {
			return qtrue;
		}

		if ( !token || token[0] == 0 ) {
			return qfalse;
		}

		if (token[0] == '{') {
			if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapName) || !String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapLoadName) 
				||!Int_Parse(p, &uiInfo.mapList[uiInfo.mapCount].teamMembers) ) {
				return qfalse;
			}

			if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].opponentName)) {
				return qfalse;
			}

			uiInfo.mapList[uiInfo.mapCount].typeBits = 0;

			while (1) {
				token = COM_ParseExt(p, qtrue);
				if (token[0] >= '0' && token[0] <= '9') {
					uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << (token[0] - 0x030));
					if (!Int_Parse(p, &uiInfo.mapList[uiInfo.mapCount].timeToBeat[token[0] - 0x30])) {
						return qfalse;
					}
				} else {
					break;
				} 
			}

			//mapList[mapCount].imageName = String_Alloc(va("levelshots/%s", mapList[mapCount].mapLoadName));
			//if (uiInfo.mapCount == 0) {
			  // only load the first cinematic, selection loads the others
  			//  uiInfo.mapList[uiInfo.mapCount].cinematic = trap_CIN_PlayCinematic(va("%s.roq",uiInfo.mapList[uiInfo.mapCount].mapLoadName), qfalse, qfalse, qtrue, 0, 0, 0, 0);
			//}
  		uiInfo.mapList[uiInfo.mapCount].cinematic = -1;
			uiInfo.mapList[uiInfo.mapCount].levelShot = trap_R_RegisterShaderNoMip(va("levelshots/%s_small", uiInfo.mapList[uiInfo.mapCount].mapLoadName));

			if (uiInfo.mapCount < MAX_MAPS) {
				uiInfo.mapCount++;
			} else {
				Com_Printf("Too many maps, last one replaced!\n");
			}
		}
	}
	return qfalse;
}

static void UI_ParseGameInfo(const char *teamFile) {
	char	*token;
	const char *p;
	const char *buff = NULL;
	//int mode = 0; TTimo: unused

	buff = GetMenuBuffer(teamFile);
	if (!buff) {
		return;
	}

	p = buff;

	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if( !token || token[0] == 0 || token[0] == '}') {
			break;
		}

		if ( Q_stricmp( token, "}" ) == 0 ) {
			break;
		}

		if (Q_stricmp(token, "gametypes") == 0) {

			if (GameType_Parse(&p, qfalse)) {
				continue;
			} else {
				break;
			}
		}

		if (Q_stricmp(token, "joingametypes") == 0) {

			if (GameType_Parse(&p, qtrue)) {
				continue;
			} else {
				break;
			}
		}

		if (Q_stricmp(token, "maps") == 0) {
			// start a new menu
			MapList_Parse(&p);
		}

	}
}

static void UI_Pause(qboolean b) {
	if (b) {
		// pause the game and set the ui keycatcher
	  trap_Cvar_Set( "cl_paused", "1" );
		trap_Key_SetCatcher( KEYCATCH_UI );
	} else {
		// unpause the game and clear the ui keycatcher
		trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
		trap_Key_ClearStates();
		trap_Cvar_Set( "cl_paused", "0" );
	}
}

static int UI_PlayCinematic(const char *name, float x, float y, float w, float h) {
  return trap_CIN_PlayCinematic(name, x, y, w, h, (CIN_loop | CIN_silent));
}

static void UI_StopCinematic(int handle) {
	if (handle >= 0) {
	  trap_CIN_StopCinematic(handle);
	} else {
		handle = abs(handle);
		if (handle == UI_MAPCINEMATIC) {
			if (uiInfo.mapList[ui_currentMap.integer].cinematic >= 0) {
			  trap_CIN_StopCinematic(uiInfo.mapList[ui_currentMap.integer].cinematic);
			  uiInfo.mapList[ui_currentMap.integer].cinematic = -1;
			}
		} else if (handle == UI_NETMAPCINEMATIC) {
			if (uiInfo.serverStatus.currentServerCinematic >= 0) {
			  trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
				uiInfo.serverStatus.currentServerCinematic = -1;
			}
		} else if (handle == UI_CLANCINEMATIC) {
		  int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
		  if (i >= 0 && i < uiInfo.teamCount) {
				if (uiInfo.teamList[i].cinematic >= 0) {
				  trap_CIN_StopCinematic(uiInfo.teamList[i].cinematic);
					uiInfo.teamList[i].cinematic = -1;
				}
			}
		}
	}
}

static void UI_DrawCinematic(int handle, float x, float y, float w, float h) {
	trap_CIN_SetExtents(handle, x, y, w, h);
  trap_CIN_DrawCinematic(handle);
}

static void UI_RunCinematicFrame(int handle) {
  trap_CIN_RunCinematic(handle);
}


/*
=================
UI_LoadForceConfig_List
=================
Looks in the directory for force config files (.fcf) and loads the name in
*/
void UI_LoadForceConfig_List( void )
{
	int			numfiles = 0;
	char		filelist[2048];
	char		configname[128];
	char		*fileptr = NULL;
	int			j = 0;
	size_t			filelen = 0;
	qboolean	lightSearch = qfalse;

	uiInfo.forceConfigCount = 0;
	Com_sprintf( uiInfo.forceConfigNames[uiInfo.forceConfigCount], sizeof(uiInfo.forceConfigNames[uiInfo.forceConfigCount]), "Custom");
	uiInfo.forceConfigCount++;
	//Always reserve index 0 as the "custom" config

nextSearch:
	if (lightSearch)
	{ //search light side folder
		numfiles = trap_FS_GetFileList("forcecfg/light", "fcf", filelist, 2048 );
		uiInfo.forceConfigLightIndexBegin = uiInfo.forceConfigCount-1;
	}
	else
	{ //search dark side folder
		numfiles = trap_FS_GetFileList("forcecfg/dark", "fcf", filelist, 2048 );
		uiInfo.forceConfigDarkIndexBegin = uiInfo.forceConfigCount-1;
	}

	fileptr = filelist;

	for (j=0; j<numfiles && uiInfo.forceConfigCount < MAX_FORCE_CONFIGS;j++,fileptr+=filelen+1)
	{
		filelen = strlen(fileptr);
		COM_StripExtension(fileptr, configname, sizeof(configname));

		if (lightSearch)
		{
			uiInfo.forceConfigSide[uiInfo.forceConfigCount] = qtrue; //light side config
		}
		else
		{
			uiInfo.forceConfigSide[uiInfo.forceConfigCount] = qfalse; //dark side config
		}

		Com_sprintf( uiInfo.forceConfigNames[uiInfo.forceConfigCount], sizeof(uiInfo.forceConfigNames[uiInfo.forceConfigCount]), "%s", configname);
		uiInfo.forceConfigCount++;
	}

	if (!lightSearch)
	{
		lightSearch = qtrue;
		goto nextSearch;
	}
}


/*
=================
PlayerModel_BuildList
=================
*/
static int UI_GetFileList_Fallback( const char *path, const char *extension, char *listbuf, int bufsize, char **listptr, size_t *memSize )
{
	if ( listptr ) *listptr = listbuf;
	if ( memSize ) *memSize = 0;

	// If we have no fallback return 0
	if ( !listbuf || !bufsize ) return 0;

	return trap_FS_GetFileList( path, extension, listbuf, bufsize );
}
static int UI_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize, char **listptr, size_t *memSize )
{
	int amount, amount2;

	// If we were given no listptr or memSize ptr just use the fallback buffer
	if ( !listptr || !memSize )
	{
		return UI_GetFileList_Fallback( path, extension, listbuf, bufsize, listptr, memSize );
	}

	// Figure out an initial memory size
	if ( *memSize < MAX_QPATH )
	{
		if ( bufsize > MAX_QPATH )
		{
			*memSize = bufsize;
		}
		else
		{
			*memSize = MAX_QPATH;
		}
	}

	// Get the inital file list
	*listptr = BG_TempAllocTry( *memSize );
	if ( !*listptr ) return UI_GetFileList_Fallback( path, extension, listbuf, bufsize, listptr, memSize );
	amount = trap_FS_GetFileList( path, extension, *listptr, *memSize );

	// Retry with more memory until two attempts return the same amount
	while( 1 )
	{
		// Free the previous list
		BG_TempFree( *memSize );

		// Try to get one with size * 2
		*memSize *= 2;
		*listptr = BG_TempAllocTry( *memSize );

		if ( !*listptr )
		{
			// We just freed the last memory from our last attempt and couldn't allocate twice the size. So we're going to
			// just allocate the same size we just freed. And as we should get the same chunk of memory we had before all our
			// data should still be set, so we can just return the amount. Ugly, but we don't have to call the engine again.
			*memSize /= 2;
			*listptr = BG_TempAlloc( *memSize );
			return amount;
		}
		amount2 = trap_FS_GetFileList( path, extension, *listptr, *memSize );

		if ( amount == amount2 )
		{
#if 1
			// If the amounts matched we have at least twice as much memory as we need, so we can free half the memory. As
			// temp memory is growing from the end of our memory pool, we can free our current memory, allocate half of
			// it (we get the second half which does not contain any of our data) and then move the memory from
			// the now unallocated, but not overwrriten first half to the valid second half.
			// This is ugly, but does the trick, unless someone alters the way BG_Temp* works internally.
			char *oldMem = *listptr;                          // Remember the old pointer
			BG_TempFree( *memSize );                          // Free the memory (this invalidates the memory above, but it's not reallocated, so we can still use it)
			*memSize /= 2;                                    // Half memory
			*listptr = BG_TempAlloc( *memSize );              // Get the new memory (this should give us the second half of our old memory)
			memmove( *listptr, oldMem, *memSize );            // Copy from the old reference to the new one
#endif
			return amount;
		}
		else
		{
			amount = amount2;
		}
	}
	return 0; // Make lcc happy
}
static void UI_BuildQ3Model_List( void )
{
	int    numfiles;
	char   fpath[2048];
	char   filelist[8192];
	char   *fileptr;
	char   *skin;
	char   *model;
	int    i;
	int    k, p;
	int    f = 0;
	int    skinLen;

	size_t filelen;
	size_t fileBufSize = 8192;

	uiInfo.q3HeadCount = 0;

	numfiles = UI_GetFileList( "models/players/", ".skin", filelist, sizeof(filelist), &fileptr, &fileBufSize );
	for ( i = 0; i < numfiles; i++, fileptr += filelen+1 )
	{
		filelen = strlen(fileptr);

		if ( !filelen ) continue;

		model = fileptr;
		skin = strchr(fileptr, '/');
		if ( !skin ) continue;
		*skin = 0;
		skin++;

		if ( !*skin ) continue;

		if ( !strcmp(model,".") || !strcmp(model,"..") )
			continue;

		for ( skinLen = (int)strlen(skin)-1; skinLen >= 0; skinLen-- )
		{
			if ( skin[skinLen] == '.' )
			{
				skin[skinLen] = 0;
				break;
			}
		}
		if ( skinLen == -1 ) skinLen = (int)strlen(skin);

		k = 0;
		while ( k < skinLen && skin[k] && skin[k] != '_' ) k++;

		if ( skin[k] == '_' )
		{
			p = 0;
			while ( skin[k] ) skin[p++] = skin[k++];
			skin[p] = '\0';
		}

		Com_sprintf( fpath, sizeof(fpath), "models/players/%s/icon%s.jpg", model, skin );
		trap_FS_FOpenFile( fpath, &f, FS_READ );

		if ( f )
		{ //if it exists
			trap_FS_FCloseFile( f );

			if ( skin[0] == '_' )
			{ //change character to append properly
				skin[0] = '/';
			}

			UI_InsertHead( va("%s%s", model, skin) );
		}
	}
	if ( fileBufSize ) BG_TempFree( fileBufSize );
}


/*
=================
UI_Init
=================
*/
void _UI_Init( qboolean inGameLoad ) {
	int i;
	const char *menuSet;
	uiClientState_t cstate;

	uiInfo.inGameLoad = inGameLoad;

	trap_GetClientState( &cstate );
	if ( cstate.connState > CA_LOADING ) {
		// This is probably a vid_restart, so copy "_ui_serverForceRank" over to "ui_rankChange"
		if ( !ui_rankChange.integer ) {
			trap_Cvar_Set( "ui_rankChange", UI_Cvar_VariableString("_ui_serverForceRank") );
		}
	}

	UI_SetServerFilter();

	// Do NOT initialize force powers, yet. See "UI_UpdateForce" for details.
	//UI_UpdateForcePowers();

	UI_RegisterCvars();
	UI_InitMemory();

	// cache redundant calulations
	trap_GetGlconfig( &uiInfo.uiDC.glconfig );

	UI_UpdateWidescreen();

	// for 640x480 virtualized screen
	uiInfo.uiDC.yscale = uiInfo.uiDC.glconfig.vidHeight * (1.0/480.0);
	uiInfo.uiDC.xscale = uiInfo.uiDC.glconfig.vidWidth * (1.0/640.0);
	if ( uiInfo.uiDC.glconfig.vidWidth * 480 > uiInfo.uiDC.glconfig.vidHeight * 640 ) {
		// wide screen
		uiInfo.uiDC.bias = 0.5 * ( uiInfo.uiDC.glconfig.vidWidth - ( uiInfo.uiDC.glconfig.vidHeight * (640.0/480.0) ) );
	}
	else {
		// no wide screen
		uiInfo.uiDC.bias = 0;
	}

  //UI_Load();
	uiInfo.uiDC.registerShaderNoMip = &trap_R_RegisterShaderNoMip;
	uiInfo.uiDC.setColor = &UI_SetColor;
	uiInfo.uiDC.drawHandlePic = &UI_DrawHandlePic;
	uiInfo.uiDC.drawStretchPic = &trap_R_DrawStretchPic;
	uiInfo.uiDC.drawText = &Text_Paint;
	uiInfo.uiDC.textWidth = &Text_Width;
	uiInfo.uiDC.textHeight = &Text_Height;
	uiInfo.uiDC.registerModel = &trap_R_RegisterModel;
	uiInfo.uiDC.modelBounds = trap_R_ModelBounds;
	uiInfo.uiDC.fillRect = &UI_FillRect;
	uiInfo.uiDC.drawRect = &_UI_DrawRect;
	uiInfo.uiDC.drawSides = &_UI_DrawSides;
	uiInfo.uiDC.drawTopBottom = &_UI_DrawTopBottom;
	uiInfo.uiDC.clearScene = &trap_R_ClearScene;
	uiInfo.uiDC.drawSides = &_UI_DrawSides;
	uiInfo.uiDC.addRefEntityToScene = &trap_R_AddRefEntityToScene;
	uiInfo.uiDC.renderScene = &trap_R_RenderScene;
	uiInfo.uiDC.RegisterFont = &trap_R_RegisterFont;
	uiInfo.uiDC.Font_StrLenPixels = trap_R_Font_StrLenPixels;
	uiInfo.uiDC.Font_StrLenChars = trap_R_Font_StrLenChars;
	uiInfo.uiDC.Font_HeightPixels = trap_R_Font_HeightPixels;
	uiInfo.uiDC.Font_DrawString = trap_R_Font_DrawString;
	uiInfo.uiDC.Language_IsAsian = trap_Language_IsAsian;
	uiInfo.uiDC.Language_UsesSpaces = trap_Language_UsesSpaces;
	//uiInfo.uiDC.AnyLanguage_ReadCharFromString = *trap_AnyLanguage_ReadCharFromString*/;
	uiInfo.uiDC.ownerDrawItem = &UI_OwnerDraw;
	uiInfo.uiDC.getValue = &UI_GetValue;
	uiInfo.uiDC.ownerDrawVisible = &UI_OwnerDrawVisible;
	uiInfo.uiDC.runScript = &UI_RunMenuScript;
	uiInfo.uiDC.deferScript = &UI_DeferMenuScript;
	uiInfo.uiDC.getTeamColor = &UI_GetTeamColor;
	uiInfo.uiDC.setCVar = trap_Cvar_Set;
	uiInfo.uiDC.getCVarString = trap_Cvar_VariableStringBuffer;
	uiInfo.uiDC.getCVarValue = trap_Cvar_VariableValue;
	uiInfo.uiDC.drawTextWithCursor = &Text_PaintWithCursor;
	uiInfo.uiDC.setOverstrikeMode = &trap_Key_SetOverstrikeMode;
	uiInfo.uiDC.getOverstrikeMode = &trap_Key_GetOverstrikeMode;
	uiInfo.uiDC.startLocalSound = &trap_S_StartLocalSound;
	uiInfo.uiDC.ownerDrawHandleKey = &UI_OwnerDrawHandleKey;
	uiInfo.uiDC.feederCount = &UI_FeederCount;
	uiInfo.uiDC.feederItemImage = &UI_FeederItemImage;
	uiInfo.uiDC.feederItemText = &UI_FeederItemText;
	uiInfo.uiDC.feederSelection = &UI_FeederSelection;
	uiInfo.uiDC.setBinding = &trap_Key_SetBinding;
	uiInfo.uiDC.getBindingBuf = &trap_Key_GetBindingBuf;
	uiInfo.uiDC.keynumToStringBuf = &trap_Key_KeynumToStringBuf;
	uiInfo.uiDC.executeText = &trap_Cmd_ExecuteText;
	uiInfo.uiDC.Error = &Com_Error; 
	uiInfo.uiDC.Print = &Com_Printf; 
	uiInfo.uiDC.Pause = &UI_Pause;
	uiInfo.uiDC.ownerDrawWidth = &UI_OwnerDrawWidth;
	uiInfo.uiDC.registerSound = &trap_S_RegisterSound;
	uiInfo.uiDC.startBackgroundTrack = trap_S_StartBackgroundTrack;
	uiInfo.uiDC.stopBackgroundTrack = trap_S_StopBackgroundTrack;
	uiInfo.uiDC.playCinematic = &UI_PlayCinematic;
	uiInfo.uiDC.stopCinematic = &UI_StopCinematic;
	uiInfo.uiDC.drawCinematic = &UI_DrawCinematic;
	uiInfo.uiDC.runCinematicFrame = &UI_RunCinematicFrame;
	uiInfo.uiDC.getClipboardData = &trap_GetClipboardData;
	uiInfo.uiDC.isDown = &trap_Key_IsDown;

	for (i=0; i<10; i++)
	{
		if (!trap_SP_Register(va("menus%d",i)))	//, /*SP_REGISTER_REQUIRED|*/SP_REGISTER_MENU))
			break;
	}
	trap_SP_Register("mv"); // language file


	Init_Display(&uiInfo.uiDC);

	String_Init();
  
	uiInfo.uiDC.cursor	= trap_R_RegisterShaderNoMip( "menu/art/3_cursor2" );
	uiInfo.uiDC.whiteShader = trap_R_RegisterShaderNoMip( "white" );

	AssetCache();

  uiInfo.teamCount = 0;
  uiInfo.characterCount = 0;
  uiInfo.aliasCount = 0;

#ifdef PRE_RELEASE_TADEMO
//	UI_ParseTeamInfo("demoteaminfo.txt");
	UI_ParseGameInfo("demogameinfo.txt");
#else
//	UI_ParseTeamInfo("ui/jk2mp/teaminfo.txt");
//	UI_LoadTeams();
	UI_ParseGameInfo("ui/jk2mp/gameinfo.txt");
#endif


	menuSet = UI_Cvar_VariableString("ui_menuFilesMP");
	if (menuSet == NULL || menuSet[0] == '\0') {
		menuSet = "ui/jk2mpmenus.txt";
	}

#if 1
	if (inGameLoad)
	{
		UI_LoadMenus("ui/jk2mpingame.txt", qtrue);
	}
	else
	{
		UI_LoadMenus(menuSet, qtrue);
	}
#else //this was adding quite a giant amount of time to the load time
	UI_LoadMenus(menuSet, qtrue);
	UI_LoadMenus("ui/jk2mpingame.txt", qtrue);
#endif
	
	Menus_CloseAll();

	trap_LAN_LoadCachedServers();
	UI_LoadBestScores(uiInfo.mapList[ui_currentMap.integer].mapLoadName, uiInfo.gameTypes[ui_gameType.integer].gtEnum);

	UI_BuildQ3Model_List();
	UI_LoadBots();

	UI_LoadForceConfig_List();

	UI_InitForceShaders();

	// sets defaults for ui temp cvars
	uiInfo.effectsColor = /*gamecodetoui[*/(int)trap_Cvar_VariableValue("color1");//-1];
	uiInfo.currentCrosshair = (int)trap_Cvar_VariableValue("cg_drawCrosshair");
	trap_Cvar_Set("ui_mousePitch", (trap_Cvar_VariableValue("m_pitch") >= 0) ? "0" : "1");

	uiInfo.serverStatus.currentServerCinematic = -1;
	uiInfo.previewMovie = -1;

	trap_Cvar_Register(NULL, "debug_protocol", "", 0 );
	trap_Cvar_Register(NULL, "ui_hidelang",	"0", CVAR_INTERNAL );

	trap_Cvar_Set("ui_actualNetGameType", va("%d", ui_netGameType.integer));

	trap_Cvar_Register(&ui_serverFilterType, "ui_serverFilterType", "0", CVAR_ARCHIVE | CVAR_GLOBAL);

	// botfilter
	trap_Cvar_Register(&ui_botfilter, "ui_botfilter", "0", CVAR_ARCHIVE | CVAR_GLOBAL);
}


/*
=================
UI_KeyEvent
=================
*/
void _UI_KeyEvent( int key, qboolean down ) {

  if (Menu_Count() > 0) {
    menuDef_t *menu = Menu_GetFocused();
		if (menu) {
			if (key == A_ESCAPE && down && !Menus_AnyFullScreenVisible() && strcmp(menu->window.name, "download_popup")) {
				Menus_CloseAll();
			} else {
				Menu_HandleKey(menu, key, down );
			}
		} else {
			trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
			trap_Key_ClearStates();
			trap_Cvar_Set( "cl_paused", "0" );
		}
  }

  //if ((s > 0) && (s != menu_null_sound)) {
	//  trap_S_StartLocalSound( s, CHAN_LOCAL_SOUND );
  //}
}

/*
=================
UI_MouseEvent
=================
*/
void _UI_MouseEvent( int dx, int dy )
{
	// update mouse screen position
	uiInfo.uiDC.cursorx += dx * uiInfo.screenXFactor;
	if (uiInfo.uiDC.cursorx < 0)
		uiInfo.uiDC.cursorx = 0;
	else if (uiInfo.uiDC.cursorx > SCREEN_WIDTH)
		uiInfo.uiDC.cursorx = SCREEN_WIDTH;

	if (ui_widescreen.integer && uiInfo.portraitMode && isMainMenu)
		uiInfo.uiDC.cursory += dy * uiInfo.screenYFactor;
	else
		uiInfo.uiDC.cursory += dy;
	if (uiInfo.uiDC.cursory < 0)
		uiInfo.uiDC.cursory = 0;
	else if (uiInfo.uiDC.cursory > uiInfo.screenHeight)
		uiInfo.uiDC.cursory = uiInfo.screenHeight;

	if (Menu_Count() > 0) {
		Display_MouseMove(NULL, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
	}
}

void UI_LoadNonIngame() {
	const char *menuSet = UI_Cvar_VariableString("ui_menuFilesMP");
	if (menuSet == NULL || menuSet[0] == '\0') {
		menuSet = "ui/jk2mpmenus.txt";
	}
	UI_LoadMenus(menuSet, qfalse);
	uiInfo.inGameLoad = qfalse;
	UI_SetServerFilter();
}

void UI_UpdateForce( qboolean update ) {
	static int forceInitialized = 0;
	// Workaround for force powers getting mixed up when joining most servers. That's actually caused by the engine not
	// loading new settings in time during fs_game switches. The engine adds "exec jk2mvconfig.cfg" to the end of the
	// command buffer to load the new config, but the command does NOT get executed right away. That means the UI module
	// is getting initialized BEFORE the correct config has been executed and we try to apply the previous force to the
	// menu and we overwrite the force powers. Additionally we don't even have the server's force rank on initialization,
	// so we can't even properly verify the powers (which can lead to force powers being removed even though they would be
	// valid on the server).

	// As workaround we don't update our force powers until the first time we open some of the ingame menus, hoping
	// that we have all the information by that time (at that time we need SOMETHING, so we can't wait anylonger anyway).
	if ( !forceInitialized ) {
		forceInitialized = 1;
		UI_UpdateForcePowers();

		// Until we get a valid force rank from the server we use "-1" to allow all force powers, so don't try to validate
		// our powers if we don't know what rank the server permits.
		if ( uiServerForceRank != -1 ) UI_ReadLegalForce();
	} else if ( update ) {
		UpdateForceUsed();
	}
}

void _UI_SetActiveMenu( uiMenuCommand_t menu ) {
	char buf[256];

	// this should be the ONLY way the menu system is brought up
	// enusure minumum menu data is cached
  if (Menu_Count() > 0) {
		vec3_t v;
		v[0] = v[1] = v[2] = 0;
	  switch ( menu ) {
	  case UIMENU_NONE:
			trap_Key_SetCatcher( trap_Key_GetCatcher() & ~KEYCATCH_UI );
			trap_Key_ClearStates();
			trap_Cvar_Set( "cl_paused", "0" );
			Menus_CloseAll();

		  return;
	  case UIMENU_MAIN:
		{
			qboolean active = qfalse;

			//trap_Cvar_Set( "sv_killserver", "1" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			//trap_S_StartLocalSound( trap_S_RegisterSound("sound/misc/menu_background.wav", qfalse) , CHAN_LOCAL_SOUND );
			//trap_S_StartBackgroundTrack("sound/misc/menu_background.wav", NULL);
			if (uiInfo.inGameLoad) 
			{
//				UI_LoadNonIngame();
			}
			
			Menus_CloseAll();
			Menus_ActivateByName("main");
			trap_Cvar_VariableStringBuffer("com_errorMessage", buf, sizeof(buf));
			
			if (strlen(buf)) 
			{
				if (!ui_singlePlayerActive.integer) 
				{
					Menus_ActivateByName("error_popmenu");
					active = qtrue;
				} 
				else 
				{
					trap_Cvar_Set("com_errorMessage", "");
				}
			}

			if ( !active && (int)trap_Cvar_VariableValue ( "com_othertasks" ) )
			{
				trap_Cvar_Set("com_othertasks", "0");
				if ( !(int)trap_Cvar_VariableValue ( "com_ignoreothertasks" ) )
				{
					Menus_ActivateByName("backgroundtask_popmenu");
					active = qtrue;
				}
			}

			return;
		}

	  case UIMENU_TEAM:
			trap_Key_SetCatcher( KEYCATCH_UI );
      Menus_ActivateByName("team");
		  return;
	  case UIMENU_POSTGAME:
			//trap_Cvar_Set( "sv_killserver", "1" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			if (uiInfo.inGameLoad) {
//				UI_LoadNonIngame();
			}
			Menus_CloseAll();
			Menus_ActivateByName("endofgame");
		  //UI_ConfirmMenu( "Bad CD Key", NULL, NeedCDKeyAction );
		  return;
	  case UIMENU_INGAME:
		  trap_Cvar_Set( "cl_paused", "1" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			UI_BuildPlayerList();
			Menus_CloseAll();
			Menus_ActivateByName("ingame");
			UI_UpdateForce(qfalse);
		  return;
	  case UIMENU_PLAYERCONFIG:
		 // trap_Cvar_Set( "cl_paused", "1" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			UI_BuildPlayerList();
			Menus_CloseAll();
			Menus_ActivateByName("ingame_player");
			UI_UpdateForce(qtrue);
		  return;
	  case UIMENU_PLAYERFORCE:
		 // trap_Cvar_Set( "cl_paused", "1" );
			trap_Key_SetCatcher( KEYCATCH_UI );
			UI_BuildPlayerList();
			Menus_CloseAll();
			Menus_ActivateByName("ingame_playerforce");
			UI_UpdateForce(qtrue);
		  return;
		  // download popup
	  case UIMENU_MV_DOWNLOAD_POPUP:
			trap_Key_SetCatcher(KEYCATCH_UI);
			Menus_CloseAll();
			Menus_ActivateByName("download_popup");
			return;
	  }
  }
}

qboolean _UI_IsFullscreen( void ) {
	return Menus_AnyFullScreenVisible();
}



static connstate_t	lastConnState;
static char			lastLoadingText[MAX_INFO_VALUE];

static void UI_ReadableSize ( char *buf, size_t bufsize, int value )
{
	if (value > 1024*1024*1024 ) { // gigs
		Com_sprintf( buf, bufsize, "%d", value / (1024*1024*1024) );
		Com_sprintf( buf+strlen(buf), bufsize-strlen(buf), ".%02d GB", 
			(value % (1024*1024*1024))*100 / (1024*1024*1024) );
	} else if (value > 1024*1024 ) { // megs
		Com_sprintf( buf, bufsize, "%d", value / (1024*1024) );
		Com_sprintf( buf+strlen(buf), bufsize-strlen(buf), ".%02d MB", 
			(value % (1024*1024))*100 / (1024*1024) );
	} else if (value > 1024 ) { // kilos
		Com_sprintf( buf, bufsize, "%d KB", value / 1024 );
	} else { // bytes
		Com_sprintf( buf, bufsize, "%d bytes", value );
	}
}

// Assumes time is in msec
static void UI_PrintTime ( char *buf, size_t bufsize, int time ) {
	time /= 1000;  // change to seconds

	if (time > 3600) { // in the hours range
		Com_sprintf( buf, bufsize, "%d hr %2d min", time / 3600, (time % 3600) / 60 );
	} else if (time > 60) { // mins
		Com_sprintf( buf, bufsize, "%2d min %2d sec", time / 60, time % 60 );
	} else  { // secs
		Com_sprintf( buf, bufsize, "%2d sec", time );
	}
}

static void Text_PaintCenter(float x, float y, float scale, const vec4_t color, const char *text, float adjust, int iMenuFont) {
	int len = Text_Width(text, scale, iMenuFont);
	Text_Paint(x - len / 2, y, scale, color, text, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE, iMenuFont);
}

// made this look a little bit better
// also prints the download method now
static void UI_DisplayDownloadInfo( const char *downloadName, float centerPoint, float yStart, float scale, int iMenuFont) {
	char sDownLoading[256];
	char sEstimatedTimeLeft[256];
	char sTransferRate[256];
	char sOf[20];
	char sCopied[256];
	char sSec[20];
	char downloadProtocol[32];
	int downloadSize, downloadCount, downloadTime;
	double percent;
	char dlSizeBuf[64], totalSizeBuf[64], xferRateBuf[64], dlTimeBuf[64];
	int xferRate;
	int leftWidth;
	const char *s;

#ifdef JK2MV_MENU
	vec4_t colorLtGreyAlpha = {0, 0, 0, .8f};
#else
	vec4_t colorLtGreyAlpha = {0, 0, 0, .5};
#endif

	UI_FillRect( 0, 0, uiInfo.screenWidth, uiInfo.screenHeight, colorLtGreyAlpha );

	downloadSize = trap_Cvar_VariableValue("cl_downloadSize");
	if (downloadSize) {
		s = GetCRDelineatedString("MENUS3","DOWNLOAD_STUFF", 0);	// "Downloading:"
		strcpy(sDownLoading,s?s:"");
		s = GetCRDelineatedString("MENUS3","DOWNLOAD_STUFF", 1);	// "Estimated time left:"
		strcpy(sEstimatedTimeLeft,s?s:"");
		s = GetCRDelineatedString("MENUS3","DOWNLOAD_STUFF", 2);	// "Transfer rate:"
		strcpy(sTransferRate,s?s:"");
		s = GetCRDelineatedString("MENUS3","DOWNLOAD_STUFF", 3);	// "of"
		strcpy(sOf,s?s:"");
		s = GetCRDelineatedString("MENUS3","DOWNLOAD_STUFF", 4);	// "copied"
		strcpy(sCopied,s?s:"");
		s = GetCRDelineatedString("MENUS3","DOWNLOAD_STUFF", 5);	// "sec."
		strcpy(sSec,s?s:"");

		downloadCount = trap_Cvar_VariableValue( "cl_downloadCount" );
		downloadTime = trap_Cvar_VariableValue( "cl_downloadTime" );
		trap_Cvar_VariableStringBuffer("cl_downloadProtocol", downloadProtocol, sizeof(downloadProtocol));

		if ( !downloadProtocol[0] )
		{
			Q_strncpyz( downloadProtocol, "UDP", sizeof(downloadProtocol) );
		}

		UI_SetColor(colorWhite);

		// round() = floor(x + 0.5)
		// microsoft has no round() in vs2010, wtf?
		percent = (double)downloadCount * 100 / downloadSize + 0.5;

		Text_PaintCenter(centerPoint, yStart + 100, scale, colorWhite, va("%s ^1[^7%s^1] ^1(^7%i%%^1)", sDownLoading, downloadProtocol, (int) percent), 0, iMenuFont);

		Text_PaintCenter(centerPoint, yStart + 125, scale, colorWhite, va("%s", downloadName), 0, iMenuFont);

		//download bar
		UI_DrawRect( centerPoint - 175, yStart + 157, 350.0f, 20.0f, colorRed );
		UI_FillRect( centerPoint - 170, yStart + 162, (percent * 3.4), 10.0f, colorRed );

		UI_ReadableSize( dlSizeBuf,		sizeof dlSizeBuf,		downloadCount );
		UI_ReadableSize( totalSizeBuf,	sizeof totalSizeBuf,	downloadSize );
		leftWidth = 320;

		if (downloadCount < 4096 || !downloadTime) {
			Text_PaintCenter(leftWidth, yStart + 205, scale, colorWhite, "estimating", 0, iMenuFont);
			Text_PaintCenter(leftWidth, yStart + 180, scale, colorWhite, va("(%s %s %s %s)", dlSizeBuf, sOf, totalSizeBuf, sCopied), 0, iMenuFont);
		} else {
			static int last_sec = 0;
			static int last_xferRate = 0;
			static int last_downloadCount = 0;
			int dlSec = (uiInfo.uiDC.realTime - downloadTime) / 1000;

#define XFER_RATE_SECS 1
			if (dlSec >= XFER_RATE_SECS && (dlSec % XFER_RATE_SECS) == 0 && last_sec < dlSec) {
				xferRate = (downloadCount - last_downloadCount) / XFER_RATE_SECS;
				last_xferRate = xferRate;
				last_downloadCount = downloadCount;
				last_sec = dlSec;
			} else if (dlSec < XFER_RATE_SECS) {
				xferRate = last_sec = last_xferRate = 0;
				last_downloadCount = 0;
			} else {
				xferRate = last_xferRate;
			}

			UI_ReadableSize( xferRateBuf, sizeof xferRateBuf, xferRate );

			// Extrapolate estimated completion time
			if (downloadSize && xferRate) {
				int n = downloadSize / xferRate; // estimated time for entire d/l in secs

				// We do it in K (/1024) because we'd overflow around 4MB
				UI_PrintTime ( dlTimeBuf, sizeof dlTimeBuf,
					(n - (((downloadCount/1024) * n) / (downloadSize/1024))) * 1000);

				Text_PaintCenter(leftWidth, yStart + 205, scale, colorWhite, dlTimeBuf, 0, iMenuFont);
				Text_PaintCenter(leftWidth, yStart + 180, scale, colorWhite, va("(%s %s %s %s)", dlSizeBuf, sOf, totalSizeBuf, sCopied), 0, iMenuFont);
			} else {
				Text_PaintCenter(leftWidth, yStart + 205, scale, colorWhite, "estimating", 0, iMenuFont);
				Text_PaintCenter(leftWidth, yStart + 180, scale, colorWhite, va("(%s %s %s %s)", dlSizeBuf, sOf, totalSizeBuf, sCopied), 0, iMenuFont);
			}

			if (xferRate) {
				Text_PaintCenter(leftWidth, yStart+272, scale, colorWhite, va("%s/%s", xferRateBuf,sSec), 0, iMenuFont);
			}
		}
	}
}

/*
========================
UI_DrawConnectScreen

This will also be overlaid on the cgame info screen during loading
to prevent it from blinking away too rapidly on local or lan games.
========================
*/
void UI_DrawConnectScreen( qboolean overlay ) {
	const char *s;
	uiClientState_t	cstate;
	char			info[MAX_INFO_VALUE];
	char text[256];
	float centerPoint, yStart, scale;

	char sStripEdTemp[256];

	menuDef_t *menu = Menus_FindByName("Connect");


	if ( !overlay && menu ) {
		Menu_Paint(menu, qtrue);
	}

	if (!overlay) {
		centerPoint = 320;
		yStart = 130;
		scale = 1.0f;	// -ste
	} else {
		centerPoint = 320;
		yStart = 32;
		scale = 1.0f;	// -ste
		return;
	}

	// see what information we should display
	trap_GetClientState( &cstate );


	info[0] = '\0';
	if( trap_GetConfigString( CS_SERVERINFO, info, sizeof(info) ) ) {
		trap_SP_GetStringTextString("MENUS3_LOADING_MAPNAME", sStripEdTemp, sizeof(sStripEdTemp));
		Text_PaintCenter(centerPoint, yStart, scale, colorWhite, va( /*"Loading %s"*/sStripEdTemp, Info_ValueForKey( info, "mapname" )), 0, FONT_MEDIUM);
	}

	if (!Q_stricmp(cstate.servername,"localhost")) {
		trap_SP_GetStringTextString("MENUS3_STARTING_UP", sStripEdTemp, sizeof(sStripEdTemp));
		Text_PaintCenter(centerPoint, yStart + 48, scale, colorWhite, sStripEdTemp, ITEM_TEXTSTYLE_SHADOWEDMORE, FONT_MEDIUM);
	} else {
		trap_SP_GetStringTextString("MENUS3_CONNECTING_TO", sStripEdTemp, sizeof(sStripEdTemp));
		strcpy(text, va(/*"Connecting to %s"*/sStripEdTemp, cstate.servername));
		Text_PaintCenter(centerPoint, yStart + 48, scale, colorWhite,text , ITEM_TEXTSTYLE_SHADOWEDMORE, FONT_MEDIUM);
	}

	//UI_DrawProportionalString( 320, 96, "Press Esc to abort", UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, menu_text_color );

	// display global MOTD at bottom
	Text_PaintCenter(centerPoint, uiInfo.screenHeight-55, scale, colorWhite, Info_ValueForKey( cstate.updateInfoString, "motd" ), 0, FONT_MEDIUM);
	// print any server info (server full, bad version, etc)
	if ( cstate.connState < CA_CONNECTED ) {
		Text_PaintCenter(centerPoint, yStart + 176, scale, colorWhite, cstate.messageString, 0, FONT_MEDIUM);
	}

	if ( lastConnState > cstate.connState ) {
		lastLoadingText[0] = '\0';
	}
	lastConnState = cstate.connState;

	switch ( cstate.connState ) {
	case CA_CONNECTING:
		{
			trap_SP_GetStringTextString("MENUS3_AWAITING_CONNECTION", sStripEdTemp, sizeof(sStripEdTemp));
			s = va(/*"Awaiting connection...%i"*/sStripEdTemp, cstate.connectPacketCount);
		}
		break;
	case CA_CHALLENGING:
		{
			trap_SP_GetStringTextString("MENUS3_AWAITING_CHALLENGE", sStripEdTemp, sizeof(sStripEdTemp));
			s = va(/*"Awaiting challenge...%i"*/sStripEdTemp, cstate.connectPacketCount);
		}
		break;
	case CA_CONNECTED: {
		char downloadName[MAX_INFO_VALUE];

			trap_Cvar_VariableStringBuffer( "cl_downloadName", downloadName, sizeof(downloadName) );
			if (*downloadName) {
				UI_DisplayDownloadInfo( downloadName, centerPoint, yStart, scale, FONT_MEDIUM );
				return;
			}
		}
		trap_SP_GetStringTextString("MENUS3_AWAITING_GAMESTATE", sStripEdTemp, sizeof(sStripEdTemp));
		s = /*"Awaiting gamestate..."*/sStripEdTemp;
		break;
	case CA_LOADING:
		return;
	case CA_PRIMED:
		return;
	default:
		return;
	}

	if (Q_stricmp(cstate.servername,"localhost")) {
		Text_PaintCenter(centerPoint, yStart + 80, scale, colorWhite, s, 0, FONT_MEDIUM);
	}

	// password required / connection rejected information goes here
}


/*
================
cvars
================
*/

typedef struct {
	vmCvar_t	*vmCvar;
	const char	*cvarName;
	const char	*defaultString;
	int			cvarFlags;
} cvarTable_t;

vmCvar_t	ui_ffa_fraglimit;
vmCvar_t	ui_ffa_timelimit;

vmCvar_t	ui_tourney_fraglimit;
vmCvar_t	ui_tourney_timelimit;

vmCvar_t	ui_selectedModelIndex;

vmCvar_t	ui_team_fraglimit;
vmCvar_t	ui_team_timelimit;
vmCvar_t	ui_team_friendly;

vmCvar_t	ui_ctf_capturelimit;
vmCvar_t	ui_ctf_timelimit;
vmCvar_t	ui_ctf_friendly;

vmCvar_t	ui_arenasFile;
vmCvar_t	ui_botsFile;
vmCvar_t	ui_spScores1;
vmCvar_t	ui_spScores2;
vmCvar_t	ui_spScores3;
vmCvar_t	ui_spScores4;
vmCvar_t	ui_spScores5;
vmCvar_t	ui_spAwards;
vmCvar_t	ui_spVideos;
vmCvar_t	ui_spSkill;

vmCvar_t	ui_spSelection;

vmCvar_t	ui_browserMaster;
vmCvar_t	ui_browserGameType;
vmCvar_t	ui_browserSortKey;
vmCvar_t	ui_browserShowFull;
vmCvar_t	ui_browserShowEmpty;

vmCvar_t	ui_drawCrosshair;
vmCvar_t	ui_drawCrosshairNames;
vmCvar_t	ui_marks;

vmCvar_t	ui_server1;
vmCvar_t	ui_server2;
vmCvar_t	ui_server3;
vmCvar_t	ui_server4;
vmCvar_t	ui_server5;
vmCvar_t	ui_server6;
vmCvar_t	ui_server7;
vmCvar_t	ui_server8;
vmCvar_t	ui_server9;
vmCvar_t	ui_server10;
vmCvar_t	ui_server11;
vmCvar_t	ui_server12;
vmCvar_t	ui_server13;
vmCvar_t	ui_server14;
vmCvar_t	ui_server15;
vmCvar_t	ui_server16;

vmCvar_t	ui_cdkeychecked;

vmCvar_t	ui_redteam;
vmCvar_t	ui_redteam1;
vmCvar_t	ui_redteam2;
vmCvar_t	ui_redteam3;
vmCvar_t	ui_redteam4;
vmCvar_t	ui_redteam5;
vmCvar_t	ui_redteam6;
vmCvar_t	ui_redteam7;
vmCvar_t	ui_redteam8;
vmCvar_t	ui_blueteam;
vmCvar_t	ui_blueteam1;
vmCvar_t	ui_blueteam2;
vmCvar_t	ui_blueteam3;
vmCvar_t	ui_blueteam4;
vmCvar_t	ui_blueteam5;
vmCvar_t	ui_blueteam6;
vmCvar_t	ui_blueteam7;
vmCvar_t	ui_blueteam8;
vmCvar_t	ui_teamName;
vmCvar_t	ui_dedicated;
vmCvar_t	ui_gameType;
vmCvar_t	ui_netGameType;
vmCvar_t	ui_actualNetGameType;
vmCvar_t	ui_joinGameType;
vmCvar_t	ui_netSource;
vmCvar_t	ui_serverFilterType;
vmCvar_t	ui_opponentName;
vmCvar_t	ui_menuFiles;
vmCvar_t	ui_currentTier;
vmCvar_t	ui_currentMap;
vmCvar_t	ui_currentNetMap;
vmCvar_t	ui_mapIndex;
vmCvar_t	ui_currentOpponent;
vmCvar_t	ui_selectedPlayer;
vmCvar_t	ui_selectedPlayerName;
vmCvar_t	ui_lastServerRefresh_0;
vmCvar_t	ui_lastServerRefresh_1;
vmCvar_t	ui_lastServerRefresh_2;
vmCvar_t	ui_lastServerRefresh_3;
vmCvar_t	ui_singlePlayerActive;
vmCvar_t	ui_scoreAccuracy;
vmCvar_t	ui_scoreImpressives;
vmCvar_t	ui_scoreExcellents;
vmCvar_t	ui_scoreCaptures;
vmCvar_t	ui_scoreDefends;
vmCvar_t	ui_scoreAssists;
vmCvar_t	ui_scoreGauntlets;
vmCvar_t	ui_scoreScore;
vmCvar_t	ui_scorePerfect;
vmCvar_t	ui_scoreTeam;
vmCvar_t	ui_scoreBase;
vmCvar_t	ui_scoreTimeBonus;
vmCvar_t	ui_scoreSkillBonus;
vmCvar_t	ui_scoreShutoutBonus;
vmCvar_t	ui_scoreTime;
vmCvar_t	ui_captureLimit;
vmCvar_t	ui_fragLimit;
vmCvar_t	ui_smallFont;
vmCvar_t	ui_bigFont;
vmCvar_t	ui_findPlayer;
vmCvar_t	ui_Q3Model;
vmCvar_t	ui_hudFiles;
vmCvar_t	ui_recordSPDemo;
vmCvar_t	ui_realCaptureLimit;
vmCvar_t	ui_realWarmUp;
vmCvar_t	ui_serverStatusTimeOut;
vmCvar_t	ui_s_language;

// botfilter
vmCvar_t	ui_botfilter;

vmCvar_t	ui_widescreen;

vmCvar_t	ui_model;
vmCvar_t	ui_team_model;

vmCvar_t	ui_MVSDK;

// bk001129 - made static to avoid aliasing
static const cvarTable_t cvarTable[] = {
	{ &ui_ffa_fraglimit, "ui_ffa_fraglimit", "20", CVAR_ARCHIVE },
	{ &ui_ffa_timelimit, "ui_ffa_timelimit", "0", CVAR_ARCHIVE },

	{ &ui_tourney_fraglimit, "ui_tourney_fraglimit", "0", CVAR_ARCHIVE },
	{ &ui_tourney_timelimit, "ui_tourney_timelimit", "15", CVAR_ARCHIVE },

	{ &ui_selectedModelIndex, "ui_selectedModelIndex", "16", CVAR_ARCHIVE },

	{ &ui_team_fraglimit, "ui_team_fraglimit", "0", CVAR_ARCHIVE },
	{ &ui_team_timelimit, "ui_team_timelimit", "20", CVAR_ARCHIVE },
	{ &ui_team_friendly, "ui_team_friendly",  "1", CVAR_ARCHIVE },

	{ &ui_ctf_capturelimit, "ui_ctf_capturelimit", "8", CVAR_ARCHIVE },
	{ &ui_ctf_timelimit, "ui_ctf_timelimit", "30", CVAR_ARCHIVE },
	{ &ui_ctf_friendly, "ui_ctf_friendly",  "0", CVAR_ARCHIVE },

	{ &ui_arenasFile, "g_arenasFile", "", CVAR_INIT|CVAR_ROM },
	{ &ui_botsFile, "g_botsFile", "", CVAR_INIT|CVAR_ROM },
	{ &ui_spScores1, "g_spScores1", "", CVAR_ARCHIVE | CVAR_ROM },
	{ &ui_spScores2, "g_spScores2", "", CVAR_ARCHIVE | CVAR_ROM },
	{ &ui_spScores3, "g_spScores3", "", CVAR_ARCHIVE | CVAR_ROM },
	{ &ui_spScores4, "g_spScores4", "", CVAR_ARCHIVE | CVAR_ROM },
	{ &ui_spScores5, "g_spScores5", "", CVAR_ARCHIVE | CVAR_ROM },
	{ &ui_spAwards, "g_spAwards", "", CVAR_ARCHIVE | CVAR_ROM },
	{ &ui_spVideos, "g_spVideos", "", CVAR_ARCHIVE | CVAR_ROM },
	{ &ui_spSkill, "g_spSkill", "2", CVAR_ARCHIVE },

	{ &ui_spSelection, "ui_spSelection", "", CVAR_ROM },

	{ &ui_browserMaster, "ui_browserMaster", "0", CVAR_ARCHIVE },
	{ &ui_browserGameType, "ui_browserGameType", "0", CVAR_ARCHIVE },
	{ &ui_browserSortKey, "ui_browserSortKey", "4", CVAR_ARCHIVE },
	{ &ui_browserShowFull, "ui_browserShowFull", "1", CVAR_ARCHIVE },
	{ &ui_browserShowEmpty, "ui_browserShowEmpty", "1", CVAR_ARCHIVE },

	{ &ui_drawCrosshair, "cg_drawCrosshair", "1", CVAR_ARCHIVE },
	{ &ui_drawCrosshairNames, "cg_drawCrosshairNames", "1", CVAR_ARCHIVE },
	{ &ui_marks, "cg_marks", "1", CVAR_ARCHIVE },

	{ &ui_server1, "server1", "", CVAR_ARCHIVE },
	{ &ui_server2, "server2", "", CVAR_ARCHIVE },
	{ &ui_server3, "server3", "", CVAR_ARCHIVE },
	{ &ui_server4, "server4", "", CVAR_ARCHIVE },
	{ &ui_server5, "server5", "", CVAR_ARCHIVE },
	{ &ui_server6, "server6", "", CVAR_ARCHIVE },
	{ &ui_server7, "server7", "", CVAR_ARCHIVE },
	{ &ui_server8, "server8", "", CVAR_ARCHIVE },
	{ &ui_server9, "server9", "", CVAR_ARCHIVE },
	{ &ui_server10, "server10", "", CVAR_ARCHIVE },
	{ &ui_server11, "server11", "", CVAR_ARCHIVE },
	{ &ui_server12, "server12", "", CVAR_ARCHIVE },
	{ &ui_server13, "server13", "", CVAR_ARCHIVE },
	{ &ui_server14, "server14", "", CVAR_ARCHIVE },
	{ &ui_server15, "server15", "", CVAR_ARCHIVE },
	{ &ui_server16, "server16", "", CVAR_ARCHIVE },
	{ &ui_cdkeychecked, "ui_cdkeychecked", "0", CVAR_ROM },
	{ &ui_debug, "ui_debug", "0", CVAR_TEMP },
	{ &ui_initialized, "ui_initialized", "0", CVAR_TEMP },
	{ &ui_teamName, "ui_teamName", "Empire", CVAR_ARCHIVE },
	{ &ui_opponentName, "ui_opponentName", "Rebellion", CVAR_ARCHIVE },
	{ &ui_rankChange, "ui_rankChange", "0", CVAR_ARCHIVE },
	{ &ui_freeSaber, "ui_freeSaber", "0", CVAR_ARCHIVE },
	{ &ui_forcePowerDisable, "ui_forcePowerDisable", "0", CVAR_ARCHIVE },
	{ &ui_redteam, "ui_redteam", "Empire", CVAR_ARCHIVE },
	{ &ui_blueteam, "ui_blueteam", "Rebellion", CVAR_ARCHIVE },
	{ &ui_dedicated, "ui_dedicated", "0", CVAR_ARCHIVE },
	{ &ui_gameType, "ui_gametype", "0", CVAR_ARCHIVE },
	{ &ui_joinGameType, "ui_joinGametype", "0", CVAR_ARCHIVE },
	{ &ui_netGameType, "ui_netGametype", "0", CVAR_ARCHIVE },
	{ &ui_actualNetGameType, "ui_actualNetGametype", "3", CVAR_ARCHIVE },
	{ &ui_redteam1, "ui_redteam1", "1", CVAR_ARCHIVE }, //rww - these used to all default to 0 (closed).. I changed them to 1 (human)
	{ &ui_redteam2, "ui_redteam2", "1", CVAR_ARCHIVE },
	{ &ui_redteam3, "ui_redteam3", "1", CVAR_ARCHIVE },
	{ &ui_redteam4, "ui_redteam4", "1", CVAR_ARCHIVE },
	{ &ui_redteam5, "ui_redteam5", "1", CVAR_ARCHIVE },
	{ &ui_redteam6, "ui_redteam6", "1", CVAR_ARCHIVE },
	{ &ui_redteam7, "ui_redteam7", "1", CVAR_ARCHIVE },
	{ &ui_redteam8, "ui_redteam8", "1", CVAR_ARCHIVE },
	{ &ui_blueteam1, "ui_blueteam1", "1", CVAR_ARCHIVE },
	{ &ui_blueteam2, "ui_blueteam2", "1", CVAR_ARCHIVE },
	{ &ui_blueteam3, "ui_blueteam3", "1", CVAR_ARCHIVE },
	{ &ui_blueteam4, "ui_blueteam4", "1", CVAR_ARCHIVE },
	{ &ui_blueteam5, "ui_blueteam5", "1", CVAR_ARCHIVE },
	{ &ui_blueteam6, "ui_blueteam6", "1", CVAR_ARCHIVE },
	{ &ui_blueteam7, "ui_blueteam7", "1", CVAR_ARCHIVE },
	{ &ui_blueteam8, "ui_blueteam8", "1", CVAR_ARCHIVE },
	{ &ui_netSource, "ui_netSource", "0", CVAR_ARCHIVE },
	{ &ui_menuFiles, "ui_menuFilesMP", "ui/jk2mpmenus.txt", CVAR_ARCHIVE },
	{ &ui_currentTier, "ui_currentTier", "0", CVAR_ARCHIVE },
	{ &ui_currentMap, "ui_currentMap", "0", CVAR_ARCHIVE },
	{ &ui_currentNetMap, "ui_currentNetMap", "0", CVAR_ARCHIVE },
	{ &ui_mapIndex, "ui_mapIndex", "0", CVAR_ARCHIVE },
	{ &ui_currentOpponent, "ui_currentOpponent", "0", CVAR_ARCHIVE },
	{ &ui_selectedPlayer, "cg_selectedPlayer", "0", CVAR_ARCHIVE},
	{ &ui_selectedPlayerName, "cg_selectedPlayerName", "", CVAR_ARCHIVE},
	{ &ui_lastServerRefresh_0, "ui_lastServerRefresh_0", "", CVAR_ARCHIVE},
	{ &ui_lastServerRefresh_1, "ui_lastServerRefresh_1", "", CVAR_ARCHIVE},
	{ &ui_lastServerRefresh_2, "ui_lastServerRefresh_2", "", CVAR_ARCHIVE},
	{ &ui_lastServerRefresh_3, "ui_lastServerRefresh_3", "", CVAR_ARCHIVE},
	{ &ui_singlePlayerActive, "ui_singlePlayerActive", "0", 0},
	{ &ui_scoreAccuracy, "ui_scoreAccuracy", "0", CVAR_ARCHIVE},
	{ &ui_scoreImpressives, "ui_scoreImpressives", "0", CVAR_ARCHIVE},
	{ &ui_scoreExcellents, "ui_scoreExcellents", "0", CVAR_ARCHIVE},
	{ &ui_scoreCaptures, "ui_scoreCaptures", "0", CVAR_ARCHIVE},
	{ &ui_scoreDefends, "ui_scoreDefends", "0", CVAR_ARCHIVE},
	{ &ui_scoreAssists, "ui_scoreAssists", "0", CVAR_ARCHIVE},
	{ &ui_scoreGauntlets, "ui_scoreGauntlets", "0",CVAR_ARCHIVE},
	{ &ui_scoreScore, "ui_scoreScore", "0", CVAR_ARCHIVE},
	{ &ui_scorePerfect, "ui_scorePerfect", "0", CVAR_ARCHIVE},
	{ &ui_scoreTeam, "ui_scoreTeam", "0 to 0", CVAR_ARCHIVE},
	{ &ui_scoreBase, "ui_scoreBase", "0", CVAR_ARCHIVE},
	{ &ui_scoreTime, "ui_scoreTime", "00:00", CVAR_ARCHIVE},
	{ &ui_scoreTimeBonus, "ui_scoreTimeBonus", "0", CVAR_ARCHIVE},
	{ &ui_scoreSkillBonus, "ui_scoreSkillBonus", "0", CVAR_ARCHIVE},
	{ &ui_scoreShutoutBonus, "ui_scoreShutoutBonus", "0", CVAR_ARCHIVE},
	{ &ui_fragLimit, "ui_fragLimit", "10", 0},
	{ &ui_captureLimit, "ui_captureLimit", "5", 0},
	{ &ui_smallFont, "ui_smallFont", "0.25", CVAR_ARCHIVE},
	{ &ui_bigFont, "ui_bigFont", "0.4", CVAR_ARCHIVE},
	{ &ui_findPlayer, "ui_findPlayer", "Kyle", CVAR_ARCHIVE},
	{ &ui_Q3Model, "ui_q3model", "0", CVAR_ARCHIVE},
	{ &ui_recordSPDemo, "ui_recordSPDemo", "0", CVAR_ARCHIVE},
	{ &ui_realWarmUp, "g_warmup", "20", CVAR_ARCHIVE},
	{ &ui_realCaptureLimit, "capturelimit", "8", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART},
	{ &ui_serverStatusTimeOut, "ui_serverStatusTimeOut", "7000", CVAR_ARCHIVE},
	{ &ui_s_language, "s_language", "english", CVAR_ARCHIVE | CVAR_NORESTART},

	{ &ui_widescreen, "ui_widescreen", "1", CVAR_ARCHIVE | CVAR_LATCH },

	{ &ui_MVSDK, "ui_MVSDK", MVSDK_VERSION, CVAR_ROM | CVAR_USERINFO },

	{ &ui_model, "model", "", CVAR_USERINFO | CVAR_ARCHIVE },
	{ &ui_team_model, "team_model", "", CVAR_USERINFO | CVAR_ARCHIVE },
};

// bk001129 - made static to avoid aliasing
static int		cvarTableSize = sizeof(cvarTable) / sizeof(cvarTable[0]);

int widescreenModificationCount = - 1;
int modelModificationCount = -1;
int teamModelModificationCount = -1;
/*
=================
UI_RegisterCvars
=================
*/
void UI_RegisterCvars( void ) {
	int			i;
	const cvarTable_t *cv;

	for ( i = 0, cv = cvarTable ; i < cvarTableSize ; i++, cv++ ) {
		trap_Cvar_Register( cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags );
	}

	widescreenModificationCount = ui_widescreen.modificationCount;
	modelModificationCount = ui_model.modificationCount;
	teamModelModificationCount = ui_team_model.modificationCount;
}

/*
=================
UI_UpdateCvars
=================
*/
void UI_UpdateCvars( void ) {
	int			i;
	const cvarTable_t	*cv;

	for ( i = 0, cv = cvarTable ; i < cvarTableSize ; i++, cv++ ) {
		trap_Cvar_Update( cv->vmCvar );
	}

	if (widescreenModificationCount != ui_widescreen.modificationCount) {
		widescreenModificationCount = ui_widescreen.modificationCount;
		UI_UpdateWidescreen();
	}

	if (modelModificationCount != ui_model.modificationCount) {
		modelModificationCount = ui_model.modificationCount;
		uiUpdateModel = qtrue;
	}
	if (teamModelModificationCount != ui_team_model.modificationCount) {
		teamModelModificationCount = ui_team_model.modificationCount;
		uiUpdateModel = qtrue;
	}
}


/*
=================
ArenaServers_StopRefresh
=================
*/
static void UI_StopServerRefresh( void )
{
	int count;

	if (!uiInfo.serverStatus.refreshActive) {
		// not currently refreshing
		return;
	}
	uiInfo.serverStatus.refreshActive = qfalse;
	Com_Printf("%d servers listed in browser with %d players.\n",
					uiInfo.serverStatus.numDisplayServers,
					uiInfo.serverStatus.numPlayersOnServers);
	count = trap_LAN_GetServerCount(ui_netSource.integer);
	if (count - uiInfo.serverStatus.numDisplayServers > 0) {
		Com_Printf("%d servers not listed due to packet loss, filters or pings higher than %d\n",
						count - uiInfo.serverStatus.numDisplayServers,
						(int) trap_Cvar_VariableValue("cl_maxPing"));
	}

}


/*
=================
UI_DoServerRefresh
=================
*/
static void UI_DoServerRefresh( void )
{
	qboolean wait = qfalse;

	if (!uiInfo.serverStatus.refreshActive) {
		return;
	}
	if (ui_netSource.integer != AS_FAVORITES) {
		if (ui_netSource.integer == AS_LOCAL) {
			if (!trap_LAN_GetServerCount(ui_netSource.integer)) {
				wait = qtrue;
			}
		} else {
			if (trap_LAN_GetServerCount(ui_netSource.integer) < 0) {
				wait = qtrue;
			}
		}
	}

	if (uiInfo.uiDC.realTime < uiInfo.serverStatus.refreshtime) {
		if (wait) {
			return;
		}
	}

	// if still trying to retrieve pings
	if (trap_LAN_UpdateVisiblePings(ui_netSource.integer)) {
		uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
	} else if (!wait) {
		// get the last servers in the list
		UI_BuildServerDisplayList(2);
		// stop the refresh
		UI_StopServerRefresh();
	}
	//
	UI_BuildServerDisplayList(qfalse);
}

/*
=================
UI_StartServerRefresh
=================
*/
static void UI_StartServerRefresh(qboolean full)
{
	int		i;
	char	*ptr;
	char cvarstr[64];

	qtime_t q;
	trap_RealTime(&q);
	// serverlist refresh datetime fix
	Com_sprintf(cvarstr, sizeof(cvarstr), "ui_lastServerRefresh_%i", ui_netSource.integer);
	trap_Cvar_Set(cvarstr, va("%s-%i, %i @ %02i:%02i", GetMonthAbbrevString(q.tm_mon), q.tm_mday, 1900 + q.tm_year, q.tm_hour, q.tm_min));

	if (!full) {
		UI_UpdatePendingPings();
		return;
	}

	uiInfo.serverStatus.refreshActive = qtrue;
	uiInfo.serverStatus.nextDisplayRefresh = uiInfo.uiDC.realTime + 1000;
	// clear number of displayed servers
	uiInfo.serverStatus.numDisplayServers = 0;
	uiInfo.serverStatus.numPlayersOnServers = 0;
	// mark all servers as visible so we store ping updates for them
	trap_LAN_MarkServerVisible(ui_netSource.integer, -1, qtrue);
	// reset all the pings
	trap_LAN_ResetPings(ui_netSource.integer);
	//
	if( ui_netSource.integer == AS_LOCAL ) {
		trap_Cmd_ExecuteText( EXEC_NOW, "localservers\n" );
		uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
		return;
	}

	uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 5000;
	if( ui_netSource.integer == AS_GLOBAL || ui_netSource.integer == AS_MPLAYER ) {
		if( ui_netSource.integer == AS_GLOBAL ) {
			i = 0;
		}
		else {
			i = 1;
		}

		ptr = UI_Cvar_VariableString("debug_protocol");
		if (strlen(ptr)) {
			trap_Cmd_ExecuteText( EXEC_NOW, va( "globalservers %d %s\n", i, ptr));
		}
		else {
#ifdef JK2MV_MENU
			trap_Cmd_ExecuteText( EXEC_NOW, va( "globalservers %d 15+16\n", i ) );
#else
			if ( menuInJK2MV )
				trap_Cmd_ExecuteText( EXEC_NOW, va( "globalservers %d 15+16\n", i ) );
			else
				trap_Cmd_ExecuteText( EXEC_NOW, va( "globalservers %d %d\n", i, (int)trap_Cvar_VariableValue( "protocol" ) ) );
#endif
		}
	}
}

