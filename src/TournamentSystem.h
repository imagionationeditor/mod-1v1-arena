#ifndef _TOURNAMENT_SYSTEM_H
#define _TOURNAMENT_SYSTEM_H

#include "Common.h"
#include "SharedDefines.h"
#include <vector>
#include <map>
#include <string>

class Player;

enum TournamentStatus
{
    TOURNAMENT_STATUS_REGISTRATION = 0,
    TOURNAMENT_STATUS_READY = 1,
    TOURNAMENT_STATUS_ACTIVE = 2,
    TOURNAMENT_STATUS_FINISHED = 3,
    TOURNAMENT_STATUS_CANCELLED = 4
};

enum TournamentMatchStatus
{
    MATCH_STATUS_PENDING = 0,
    MATCH_STATUS_ACTIVE = 1,
    MATCH_STATUS_COMPLETED = 2,
    MATCH_STATUS_FORFEIT = 3
};

enum TournamentRoundStatus
{
    ROUND_STATUS_PENDING = 0,
    ROUND_STATUS_ACTIVE = 1,
    ROUND_STATUS_COMPLETED = 2
};

enum TournamentActions
{
    TOURNAMENT_ACTION_VIEW_TOURNAMENTS = 100,
    TOURNAMENT_ACTION_REGISTER = 101,
    TOURNAMENT_ACTION_MY_TOURNAMENTS = 102,
    TOURNAMENT_ACTION_TOURNAMENT_BRACKET = 103,
    TOURNAMENT_ACTION_TOURNAMENT_STATS = 104,
    TOURNAMENT_ACTION_ADMIN_MENU = 200,
    TOURNAMENT_ACTION_ADMIN_CREATE = 201,
    TOURNAMENT_ACTION_ADMIN_START = 202,
    TOURNAMENT_ACTION_ADMIN_CANCEL = 203,
    TOURNAMENT_ACTION_ADMIN_VIEW_ALL = 204
};

struct TournamentInfo
{
    uint32 id;
    std::string name;
    std::string description;
    uint32 entryFee;
    time_t registrationStart;
    time_t registrationEnd;
    time_t tournamentStart;
    time_t tournamentEnd;
    TournamentStatus status;
    uint32 maxParticipants;
    uint32 currentParticipants;
    uint32 winnerRewardGold;
    uint32 winnerRewardItem;
    uint32 winnerTitle;
    uint32 createdBy;
    time_t createdAt;
};

struct TournamentMatch
{
    uint32 id;
    uint32 tournamentId;
    uint32 roundId;
    uint32 matchNumber;
    uint32 player1Guid;
    uint32 player2Guid;
    std::string player1Name;
    std::string player2Name;
    uint32 winnerGuid;
    TournamentMatchStatus status;
    time_t matchStart;
    time_t matchEnd;
    uint32 joinAttemptsPlayer1;
    uint32 joinAttemptsPlayer2;
    uint32 battlegroundId;
};

struct TournamentRound
{
    uint32 id;
    uint32 tournamentId;
    uint32 roundNumber;
    std::string roundName;
    TournamentRoundStatus status;
    time_t startTime;
    time_t endTime;
    std::vector<TournamentMatch> matches;
};

struct TournamentRegistration
{
    uint32 id;
    uint32 tournamentId;
    uint32 playerGuid;
    std::string characterName;
    time_t registrationTime;
    bool entryFeePaid;
    bool confirmed;
};

class TournamentSystem
{
public:
    static TournamentSystem* instance();

    // Tournament Management
    uint32 CreateTournament(const std::string& name, const std::string& description, 
                           uint32 entryFee, uint32 registrationDurationHours, 
                           uint32 maxParticipants, uint32 createdBy,
                           uint32 winnerRewardGold = 0, uint32 winnerRewardItem = 0, uint32 winnerTitle = 0);
    bool StartTournament(uint32 tournamentId);
    bool CancelTournament(uint32 tournamentId);
    bool FinishTournament(uint32 tournamentId, uint32 winnerGuid);

    // Registration Management
    bool RegisterPlayer(uint32 tournamentId, Player* player);
    bool UnregisterPlayer(uint32 tournamentId, uint32 playerGuid);
    bool PayEntryFee(uint32 tournamentId, Player* player);
    
    // Tournament Operations
    bool GenerateBracket(uint32 tournamentId);
    bool StartNextRound(uint32 tournamentId);
    bool ProcessMatchResult(uint32 matchId, uint32 winnerGuid);
    bool CheckForForfeits();
    
    // Information Retrieval
    std::vector<TournamentInfo> GetActiveTournaments();
    std::vector<TournamentInfo> GetAllTournaments();
    TournamentInfo GetTournamentInfo(uint32 tournamentId);
    std::vector<TournamentRegistration> GetTournamentRegistrations(uint32 tournamentId);
    std::vector<TournamentRound> GetTournamentBracket(uint32 tournamentId);
    TournamentMatch GetPlayerCurrentMatch(uint32 playerGuid);
    
    // Player Statistics
    bool UpdatePlayerStats(uint32 playerGuid, bool won, uint32 goldEarned = 0);
    std::map<std::string, uint32> GetPlayerTournamentStats(uint32 playerGuid);
    
    // Utilities
    bool IsPlayerRegistered(uint32 tournamentId, uint32 playerGuid);
    bool CanPlayerRegister(uint32 tournamentId, Player* player, std::string& errorMsg);
    bool IsPlayerInActiveTournament(uint32 playerGuid);
    std::string GetTournamentStatusString(TournamentStatus status);
    std::string GetMatchStatusString(TournamentMatchStatus status);
    
    // Admin Functions
    bool ForceStartMatch(uint32 matchId);
    bool DisqualifyPlayer(uint32 tournamentId, uint32 playerGuid);
    void SendTournamentAnnouncement(const std::string& message);
    
    // Event Handlers
    void OnPlayerLogout(Player* player);
    void OnBattlegroundEnd(uint32 battlegroundId, uint32 winnerGuid);
    
    // Scheduled Tasks
    void Update(uint32 diff);

private:
    TournamentSystem() = default;
    ~TournamentSystem() = default;
    
    // Helper Functions
    std::string GenerateRoundName(uint32 participantCount, uint32 roundNumber);
    uint32 CalculateRoundsNeeded(uint32 participantCount);
    void ShuffleParticipants(std::vector<uint32>& participants);
    bool HasEnoughGold(Player* player, uint32 amount);
    void GiveRewards(uint32 playerGuid, uint32 goldAmount, uint32 itemId = 0, uint32 titleId = 0);
    void LogTournamentEvent(uint32 tournamentId, const std::string& event);
    
    static TournamentSystem* _instance;
    uint32 _updateTimer;
};

#define sTournamentSystem TournamentSystem::instance()

#endif // _TOURNAMENT_SYSTEM_H
