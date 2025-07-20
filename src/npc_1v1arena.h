#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "TournamentSystem.h"
#include <string>


class npc_1v1arena : public CreatureScript
{
public:
    npc_1v1arena() : CreatureScript("npc_1v1arena") { }

    bool OnGossipHello(Player* player, Creature* creature) override;
    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override;
    bool JoinQueueArena(Player* player, Creature* /* me */, bool isRated);
    bool CreateArenateam(Player* player, Creature* /* me */);
    void ShowLeaderboard(Player* player, Creature* creature);
    void ShowQueueStatus(Player* player, Creature* creature);
    
    // Tournament functions
    void ShowTournamentMenu(Player* player, Creature* creature);
    void ShowActiveTournaments(Player* player, Creature* creature);
    void ShowTournamentDetails(Player* player, Creature* creature, uint32 tournamentId);
    void ShowTournamentBracket(Player* player, Creature* creature, uint32 tournamentId);
    void ShowMyTournaments(Player* player, Creature* creature);
    void ShowTournamentStats(Player* player, Creature* creature);
    void ShowAdminTournamentMenu(Player* player, Creature* creature);
    bool RegisterForTournament(Player* player, uint32 tournamentId);
    
    // Enhanced Admin Tournament Functions
    void ShowBasicTournamentCreation(Player* player, Creature* creature);
    void ShowAdvancedTournamentCreation(Player* player, Creature* creature);
    void ShowStartTournamentMenu(Player* player, Creature* creature);
    void ShowCancelTournamentMenu(Player* player, Creature* creature);
    void ShowRewardSettingsMenu(Player* player, Creature* creature);
    void ShowGeneralSettingsMenu(Player* player, Creature* creature);
    void ShowAllTournamentsAdmin(Player* player, Creature* creature);
    void ShowTournamentParticipants(Player* player, Creature* creature, uint32 tournamentId);
    void CreateTournamentFromMenu(Player* player, const std::string& name, const std::string& description,
        uint32 entryFee, uint32 registrationHours, uint32 maxParticipants,
        uint32 winnerGold, uint32 winnerItem, uint32 winnerTitle,
        uint32 runnerUpGold, uint32 semiFinalistGold, uint32 winnerCount = 1);
    
private:
    bool Arena1v1CheckTalents(Player* player);
    std::string GetClassNameById(uint8 classId);
    std::string GetClassColorById(uint8 classId);
    bool IsPlayerAdmin(Player* player);
    std::string FormatTime(time_t timestamp);
    std::string FormatDuration(uint32 seconds);
};
