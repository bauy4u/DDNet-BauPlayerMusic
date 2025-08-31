/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
#include "DDRace.h"

#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#define GAME_TYPE_NAME "DDraceNetwork"
#define TEST_TYPE_NAME "SPlayer DM"

CGameControllerDDRace::CGameControllerDDRace(class CGameContext *pGameServer) :
	IGameController(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_TYPE_NAME : GAME_TYPE_NAME;
	m_GameFlags = protocol7::GAMEFLAG_RACE;
}

CGameControllerDDRace::~CGameControllerDDRace() = default;

CScore *CGameControllerDDRace::Score()
{
	return GameServer()->Score();
}

void CGameControllerDDRace::HandleCharacterTiles(CCharacter *pChr, int MapIndex)
{
	CPlayer *pPlayer = pChr->GetPlayer();
	const int ClientId = pPlayer->GetCid();

	int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int TileFIndex = GameServer()->Collision()->GetFrontTileIndex(MapIndex);

	//Sensitivity
	int S1 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S2 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int S3 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S4 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int Tile1 = GameServer()->Collision()->GetTileIndex(S1);
	int Tile2 = GameServer()->Collision()->GetTileIndex(S2);
	int Tile3 = GameServer()->Collision()->GetTileIndex(S3);
	int Tile4 = GameServer()->Collision()->GetTileIndex(S4);
	int FTile1 = GameServer()->Collision()->GetFrontTileIndex(S1);
	int FTile2 = GameServer()->Collision()->GetFrontTileIndex(S2);
	int FTile3 = GameServer()->Collision()->GetFrontTileIndex(S3);
	int FTile4 = GameServer()->Collision()->GetFrontTileIndex(S4);

	const ERaceState PlayerDDRaceState = pChr->m_DDRaceState;
	bool IsOnStartTile = (TileIndex == TILE_START) || (TileFIndex == TILE_START) || FTile1 == TILE_START || FTile2 == TILE_START || FTile3 == TILE_START || FTile4 == TILE_START || Tile1 == TILE_START || Tile2 == TILE_START || Tile3 == TILE_START || Tile4 == TILE_START;
	// start
	if(IsOnStartTile && PlayerDDRaceState != ERaceState::CHEATED)
	{
		const int Team = GameServer()->GetDDRaceTeam(ClientId);
		if(Teams().GetSaving(Team))
		{
			GameServer()->SendStartWarning(ClientId, "You can't start while loading/saving of team is in progress");
			pChr->Die(ClientId, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam == SV_TEAM_MANDATORY && (Team == TEAM_FLOCK || Teams().Count(Team) <= 1))
		{
			GameServer()->SendStartWarning(ClientId, "You have to be in a team with other tees to start");
			pChr->Die(ClientId, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team > TEAM_FLOCK && Team < TEAM_SUPER && Teams().Count(Team) < g_Config.m_SvMinTeamSize && !Teams().TeamFlock(Team))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Your team has fewer than %d players, so your team rank won't count", g_Config.m_SvMinTeamSize);
			GameServer()->SendStartWarning(ClientId, aBuf);
		}
		if(g_Config.m_SvResetPickups)
		{
			pChr->ResetPickups();
		}

		Teams().OnCharacterStart(ClientId);
		pChr->m_LastTimeCp = -1;
		pChr->m_LastTimeCpBroadcasted = -1;
		for(float &CurrentTimeCp : pChr->m_aCurrentTimeCp)
		{
			CurrentTimeCp = 0.0f;
		}
	}

	// finish
	if(((TileIndex == TILE_FINISH) || (TileFIndex == TILE_FINISH) || FTile1 == TILE_FINISH || FTile2 == TILE_FINISH || FTile3 == TILE_FINISH || FTile4 == TILE_FINISH || Tile1 == TILE_FINISH || Tile2 == TILE_FINISH || Tile3 == TILE_FINISH || Tile4 == TILE_FINISH) && PlayerDDRaceState == ERaceState::STARTED)
		Teams().OnCharacterFinish(ClientId);

	// unlock team
	else if(((TileIndex == TILE_UNLOCK_TEAM) || (TileFIndex == TILE_UNLOCK_TEAM)) && Teams().TeamLocked(GameServer()->GetDDRaceTeam(ClientId)))
	{
		Teams().SetTeamLock(GameServer()->GetDDRaceTeam(ClientId), false);
		GameServer()->SendChatTeam(GameServer()->GetDDRaceTeam(ClientId), "Your team was unlocked by an unlock team tile");
	}

	// solo part
	if(((TileIndex == TILE_SOLO_ENABLE) || (TileFIndex == TILE_SOLO_ENABLE)) && !Teams().m_Core.GetSolo(ClientId))
	{
		GameServer()->SendChatTarget(ClientId, "You are now in a solo part");
		pChr->SetSolo(true);
	}
	else if(((TileIndex == TILE_SOLO_DISABLE) || (TileFIndex == TILE_SOLO_DISABLE)) && Teams().m_Core.GetSolo(ClientId))
	{
		GameServer()->SendChatTarget(ClientId, "You are now out of the solo part");
		pChr->SetSolo(false);
	}
	if(((TileIndex == TILE_BADMINTON_ENABLE) || (TileFIndex == TILE_BADMINTON_ENABLE)) && !pPlayer->m_InBadmintonZone)  
	{  
		pPlayer->m_InBadmintonZone = true;  
		GameServer()->SendChatTarget(ClientId, "进入羽毛球区域！使用/ball,/red,/blue加入羽毛球身份组,使用/start [获胜分数]开始游戏,游戏过程中可以/bs查看详情");  
	}  
	else if(((TileIndex == TILE_BADMINTON_DISABLE) || (TileFIndex == TILE_BADMINTON_DISABLE)) && pPlayer->m_InBadmintonZone)  
	{  
		if(pPlayer->m_BadmintonRole == ROLE_BALL)  
		{  
			pChr->SetDeepFrozen(false);  
		}  
		
		pPlayer->m_InBadmintonZone = false;  
		pPlayer->m_BadmintonRole = ROLE_NONE;
		pPlayer->m_TeeInfos.m_UseCustomColor = 0;  
    	GameServer()->SendSkinChangeMessage(ClientId);    
		GameServer()->SendChatTarget(ClientId, "离开羽毛球区域");  
	}  
	

	// 羽毛球得分检测（使用IN/OUT图块）  
	if(pPlayer->m_InBadmintonZone && pPlayer->m_BadmintonRole == ROLE_BALL)  
	{  
		int Team = GameServer()->GetDDRaceTeam(ClientId);  
		if(Team >= TEAM_FLOCK && Team < TEAM_SUPER)  
		{  
			SBadmintonGameState *pGameState = &m_aBadmintonGameState[Team];  
			
			if(pGameState->m_GameActive)  
			{  
				// 红队得分区域（球落到红队区域，蓝队得分）  
				if((TileIndex == TILE_RED_IN || TileFIndex == TILE_RED_IN) && pPlayer->m_BadmintonRedScoreValid)  
				{  
					pGameState->m_BlueScore++;  
					pPlayer->m_BadmintonRedScoreValid = false;  
					
					char aBuf[256];  
					str_format(aBuf, sizeof(aBuf), "蓝队得分！当前比分 红队:%d 蓝队:%d",   
							pGameState->m_RedScore, pGameState->m_BlueScore);  
					GameServer()->SendChatTeam(Team, aBuf);  
					
					// 检查游戏结束条件  
					if(pGameState->m_BlueScore >= pGameState->m_GameScore)  
					{  
						GameServer()->SendChatTeam(Team, "蓝队获胜！游戏结束！");  
						pGameState->m_GameActive = false;  
						// 重置所有队伍内玩家状态  
						for(int j = 0; j < MAX_CLIENTS; j++)  
						{  
							if(GameServer()->m_apPlayers[j] && GameServer()->GetDDRaceTeam(j) == Team)  
							{  
								GameServer()->m_apPlayers[j]->m_BadmintonRole = ROLE_NONE;  
								if(GameServer()->m_apPlayers[j]->GetCharacter())  
									GameServer()->m_apPlayers[j]->GetCharacter()->SetDeepFrozen(false);  
							}  
						}  
					}  
				}  
				else if((TileIndex == TILE_RED_OUT || TileFIndex == TILE_RED_OUT) && !pPlayer->m_BadmintonRedScoreValid)  
				{  
					pPlayer->m_BadmintonRedScoreValid = true;  
				}  
				
				// 蓝队得分区域（球落到蓝队区域，红队得分）  
				if((TileIndex == TILE_BLUE_IN || TileFIndex == TILE_BLUE_IN) && pPlayer->m_BadmintonBlueScoreValid)  
				{  
					pGameState->m_RedScore++;  
					pPlayer->m_BadmintonBlueScoreValid = false;  
					
					char aBuf[256];  
					str_format(aBuf, sizeof(aBuf), "红队得分！当前比分 红队:%d 蓝队:%d",   
							pGameState->m_RedScore, pGameState->m_BlueScore);  
					GameServer()->SendChatTeam(Team, aBuf);  
					
					// 检查游戏结束条件  
					if(pGameState->m_RedScore >= pGameState->m_GameScore)  
					{  
						GameServer()->SendChatTeam(Team, "红队获胜！游戏结束！");  
						pGameState->m_GameActive = false;  
						// 重置所有队伍内玩家状态  
						for(int j = 0; j < MAX_CLIENTS; j++)  
						{  
							if(GameServer()->m_apPlayers[j] && GameServer()->GetDDRaceTeam(j) == Team)  
							{  
								GameServer()->m_apPlayers[j]->m_BadmintonRole = ROLE_NONE;  
								if(GameServer()->m_apPlayers[j]->GetCharacter())  
									GameServer()->m_apPlayers[j]->GetCharacter()->SetDeepFrozen(false);  
							}  
						}  
					}  
				}  
				else if((TileIndex == TILE_BLUE_OUT || TileFIndex == TILE_BLUE_OUT) && !pPlayer->m_BadmintonBlueScoreValid)  
				{  
					pPlayer->m_BadmintonBlueScoreValid = true;  
				}  
			}  
		}  
	}
	if(pPlayer->m_InBadmintonZone) 
	{  
		int Team = GameServer()->GetDDRaceTeam(ClientId);  
		if(Team >= TEAM_FLOCK && Team < TEAM_SUPER)  
		{  
			SBadmintonGameState *pGameState = &m_aBadmintonGameState[Team];  

			
			// 红队玩家碰到蓝队OUT区域，当校验变量为true时红队失分  
			if((TileIndex == TILE_BLUE_OUT || TileFIndex == TILE_BLUE_OUT) &&   
			pPlayer->m_BadmintonRole == ROLE_RED && pPlayer->m_BadmintonBlueOutValid && pGameState->m_GameActive)  
			{  
				if(pGameState->m_RedScore > 0)  
				{  
					pGameState->m_RedScore--;  
					char aBuf[256];  
					str_format(aBuf, sizeof(aBuf), "红队玩家出界！红队失分！当前比分 红队:%d 蓝队:%d",   
							pGameState->m_RedScore, pGameState->m_BlueScore);  
					GameServer()->SendChatTeam(Team, aBuf);  
				}  
				
				// 设置校验变量为false  
				pPlayer->m_BadmintonBlueOutValid = false;  
				
				// 传送红队玩家到红队IN处  
				TeleportPlayerToTeamIn(pChr, ROLE_RED);  
			}  
			// 红队玩家碰到红队OUT区域，重置校验变量为true  
			else if((TileIndex == TILE_RED_OUT || TileFIndex == TILE_RED_OUT) &&   
					pPlayer->m_BadmintonRole == ROLE_RED && !pPlayer->m_BadmintonRedOutValid)  
			{  
				pPlayer->m_BadmintonRedOutValid = true;  
			}  
			
			// 蓝队玩家碰到红队OUT区域，当校验变量为true时蓝队失分  
			if((TileIndex == TILE_RED_OUT || TileFIndex == TILE_RED_OUT) &&   
			pPlayer->m_BadmintonRole == ROLE_BLUE && pPlayer->m_BadmintonRedOutValid)  
			{  
				if(pGameState->m_BlueScore > 0)  
				{  
					pGameState->m_BlueScore--;  
					char aBuf[256];  
					str_format(aBuf, sizeof(aBuf), "蓝队玩家出界！蓝队失分！当前比分 红队:%d 蓝队:%d",   
							pGameState->m_RedScore, pGameState->m_BlueScore);  
					GameServer()->SendChatTeam(Team, aBuf);  
				}  
				
				// 设置校验变量为false  
				pPlayer->m_BadmintonRedOutValid = false;  
				
				// 传送蓝队玩家到蓝队IN处  
				TeleportPlayerToTeamIn(pChr, ROLE_BLUE);  
			}  
			// 蓝队玩家碰到蓝队OUT区域，重置校验变量为true  
			else if((TileIndex == TILE_BLUE_OUT || TileFIndex == TILE_BLUE_OUT) &&   
					pPlayer->m_BadmintonRole == ROLE_BLUE && !pPlayer->m_BadmintonBlueOutValid)  
			{  
				pPlayer->m_BadmintonBlueOutValid = true;  
			}  
		}  
	}
}

void CGameControllerDDRace::SetArmorProgress(CCharacter *pCharacter, int Progress)
{
	pCharacter->SetArmor(std::clamp(10 - (Progress / 15), 0, 10));
}

void CGameControllerDDRace::OnPlayerConnect(CPlayer *pPlayer)
{
	IGameController::OnPlayerConnect(pPlayer);
	int ClientId = pPlayer->GetCid();

	// init the player
	Score()->PlayerData(ClientId)->Reset();

	// Can't set score here as LoadScore() is threaded, run it in
	// LoadScoreThreaded() instead
	Score()->LoadPlayerData(ClientId);

	if(!Server()->ClientPrevIngame(ClientId))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientId), GetTeamName(pPlayer->GetTeam()));
		GameServer()->SendChat(-1, TEAM_ALL, aBuf, -1, CGameContext::FLAG_SIX);

		GameServer()->SendChatTarget(ClientId, "DDraceNetwork Mod. Version: " GAME_VERSION);
		GameServer()->SendChatTarget(ClientId, "please visit DDNet.org or say /info and make sure to read our /rules");
	}
}

void CGameControllerDDRace::OnPlayerDisconnect(CPlayer *pPlayer, const char *pReason)
{
	int ClientId = pPlayer->GetCid();
	bool WasModerator = pPlayer->m_Moderating && Server()->ClientIngame(ClientId);

	IGameController::OnPlayerDisconnect(pPlayer, pReason);

	if(!GameServer()->PlayerModerating() && WasModerator)
		GameServer()->SendChat(-1, TEAM_ALL, "Server kick/spec votes are no longer actively moderated.");

	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO)
		Teams().SetForceCharacterTeam(ClientId, TEAM_FLOCK);

	for(int Team = TEAM_FLOCK + 1; Team < TEAM_SUPER; Team++)
		if(Teams().IsInvited(Team, ClientId))
			Teams().SetClientInvited(Team, ClientId, false);
}

void CGameControllerDDRace::OnReset()
{
	IGameController::OnReset();
	Teams().Reset();
	for(int i = 0; i < NUM_DDRACE_TEAMS; i++)  
    {  
        m_aBadmintonGameState[i] = SBadmintonGameState();  
    }  
}

void CGameControllerDDRace::Tick()
{
	IGameController::Tick();
	Teams().ProcessSaveTeam();
	Teams().Tick();
	for(int i = 0; i < MAX_CLIENTS; i++)  
    {  
        if(!GameServer()->m_apPlayers[i] || !GameServer()->m_apPlayers[i]->m_InBadmintonZone ||   
           !GameServer()->m_apPlayers[i]->m_BadmintonGameActive)  
            continue;  
  
        // 每10秒广播一次分数  
        if(Server()->Tick() - GameServer()->m_apPlayers[i]->m_BadmintonLastBroadcastTick > Server()->TickSpeed() * 10)  
        {  
            GameServer()->m_apPlayers[i]->m_BadmintonLastBroadcastTick = Server()->Tick();  
              
            char aBuf[256];  
            str_format(aBuf, sizeof(aBuf), "比分 红队:%d 蓝队:%d (目标:%d)",   
                      GameServer()->m_apPlayers[i]->m_BadmintonRedScore,   
                      GameServer()->m_apPlayers[i]->m_BadmintonBlueScore,  
                      GameServer()->m_apPlayers[i]->m_BadmintonGameScore);  
              
            int Team = GameServer()->GetDDRaceTeam(i);  
            GameServer()->SendChatTeam(Team, aBuf);  
            break; // 只需要一个玩家广播即可  
        }  
    } 
}

void CGameControllerDDRace::DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	CCharacter *pCharacter = pPlayer->GetCharacter();

	if(Team == TEAM_SPECTATORS)
	{
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && pCharacter)
		{
			// Joining spectators should not kill a locked team, but should still
			// check if the team finished by you leaving it.
			int DDRTeam = pCharacter->Team();
			Teams().SetForceCharacterTeam(pPlayer->GetCid(), TEAM_FLOCK);
			Teams().CheckTeamFinished(DDRTeam);
		}
	}

	IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);
}

void CGameControllerDDRace::RestoreBadmintonStates(SBadmintonGameState **pSavedStates)  
{  
    for(int i = 0; i < NUM_DDRACE_TEAMS; i++)  
    {  
        m_aBadmintonGameState[i] = *(pSavedStates[i]);  
    }  
}

void CGameControllerDDRace::TeleportPlayerToTeamIn(CCharacter *pChr, int TeamRole)    
{    
	GameServer()->SendChatTarget(pChr->GetPlayer()->GetCid(), "TeleportPlayerToTeamIn called!");  
    if(!pChr)    
    {  
        GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", "Character is null, aborting teleport");  
        return;  
    }  
        
    int TargetTile = (TeamRole == ROLE_RED) ? TILE_RED_IN : TILE_BLUE_IN;    
      
    // 添加调试输出  
    char aBuf[256];  
    str_format(aBuf, sizeof(aBuf), "Starting teleport search for player %s, looking for tile %d (TeamRole=%d)",   
       GameServer()->Server()->ClientName(pChr->GetPlayer()->GetCid()), TargetTile, TeamRole);  
    GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", aBuf);  
      
    int MapWidth = GameServer()->Collision()->GetWidth();  
    int MapHeight = GameServer()->Collision()->GetHeight();  
    str_format(aBuf, sizeof(aBuf), "Map dimensions: %dx%d", MapWidth, MapHeight);  
    GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", aBuf);  
      
    int TilesFound = 0;  
    int SpecialTilesFound = 0;  
        
    for(int y = 0; y < MapHeight; y++)    
    {    
        for(int x = 0; x < MapWidth; x++)    
        {    
            int Index = GameServer()->Collision()->GetIndex(x, y);    
            int TileIndex = GameServer()->Collision()->GetTileIndex(Index);    
            int TileFIndex = GameServer()->Collision()->GetFrontTileIndex(Index);    
              
            // 调试：输出找到的特殊图块（非空图块）  
            if(TileIndex > 0 || TileFIndex > 0) {  
                SpecialTilesFound++;  
                if(SpecialTilesFound <= 10) { // 只输出前10个，避免刷屏  
                    str_format(aBuf, sizeof(aBuf), "Found tile at (%d,%d): Game=%d, Front=%d", x, y, TileIndex, TileFIndex);  
                    GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", aBuf);  
                }  
            }  
                
            if(TileIndex == TargetTile || TileFIndex == TargetTile)    
            {    
                TilesFound++;  
                vec2 TelePos = vec2(x * 32.0f + 16.0f, y * 32.0f + 16.0f);  
                  
                str_format(aBuf, sizeof(aBuf), "Found target tile %d at position (%d,%d), teleporting to (%.1f,%.1f)",   
                    TargetTile, x, y, TelePos.x, TelePos.y);  
                GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", aBuf);  
                  
                pChr->SetPosition(TelePos);    
                pChr->m_Pos = TelePos;    
                pChr->m_PrevPos = TelePos;    
                pChr->SetVelocity(vec2(0, 0));    
                    
                // 创建传送效果    
                GameServer()->CreateDeath(pChr->GetPos(), pChr->GetPlayer()->GetCid(), pChr->TeamMask());    
                GameServer()->CreateSound(pChr->GetPos(), SOUND_WEAPON_SPAWN, pChr->TeamMask());    
                  
                str_format(aBuf, sizeof(aBuf), "Teleport completed successfully to tile %d", TargetTile);  
                GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", aBuf);  
                    
                return;    
            }    
        }    
    }    
      
    // 添加详细的未找到信息  
    str_format(aBuf, sizeof(aBuf), "Target tile %d NOT FOUND in map! Searched %d total tiles, found %d special tiles",   
        TargetTile, MapWidth * MapHeight, SpecialTilesFound);  
    GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", aBuf);  
      
    // 建议检查的事项  
    GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", "Suggestions:");  
    GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", "1. Check if tile ID 171/172 exists in your map");  
    GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", "2. Verify tile is placed in Game or Front layer, not Tele/Switch layer");  
    GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "teleport", "3. Confirm TILE_RED_IN and TILE_BLUE_IN constants are correctly defined");  
}
