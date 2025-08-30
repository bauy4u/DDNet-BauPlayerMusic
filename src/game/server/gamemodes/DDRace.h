/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_GAMEMODES_DDRACE_H
#define GAME_SERVER_GAMEMODES_DDRACE_H

#include <game/server/gamecontroller.h>

struct SBadmintonGameState  
{  
    int m_GameScore;         // 目标获胜分数  
    int m_RedScore;          // 红队当前分数  
    int m_BlueScore;         // 蓝队当前分数  
    bool m_GameActive;       // 游戏是否进行中  
    int m_LastBroadcastTick; // 上次广播tick  
      
    SBadmintonGameState()  
    {  
        m_GameScore = 0;  
        m_RedScore = 0;  
        m_BlueScore = 0;  
        m_GameActive = false;  
        m_LastBroadcastTick = 0;  
    }  
}; 

class CGameControllerDDRace : public IGameController
{
public:
	CGameControllerDDRace(class CGameContext *pGameServer);
	~CGameControllerDDRace();

	CScore *Score();

	void HandleCharacterTiles(class CCharacter *pChr, int MapIndex) override;
	void SetArmorProgress(CCharacter *pCharacter, int Progress) override;

	void OnPlayerConnect(class CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;
	SBadmintonGameState m_aBadmintonGameState[NUM_DDRACE_TEAMS]; 

	void OnReset() override;

	void Tick() override;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true) override;
};
#endif // GAME_SERVER_GAMEMODES_DDRACE_H
