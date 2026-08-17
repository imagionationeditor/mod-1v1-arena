#pragma once
#include "DatabaseEnv.h"
#include "TournamentSystem.h"
#include <vector>
#include <map>
#include <utility>
#include <tuple>

class TournamentRepository
{
public:
    static TournamentRepository* instance();

    // arena_tournaments
    uint32 InsertTournament(const std::string& name, const std::string& description, uint32 entryFeeCopper,
                             time_t regStart, time_t regEnd, uint32 maxParticipants, uint32 createdBy,
                             uint32 winnerGoldCopper, uint32 winnerItem, uint32 winnerTitle);
    bool SetTournamentStatus(uint32 id, const std::string& status);
    bool SetTournamentFinished(uint32 id);
    bool IncrementParticipantCount(uint32 id);
    bool DecrementParticipantCount(uint32 id);
    TournamentInfo GetTournamentInfo(uint32 id);
    std::vector<TournamentInfo> GetTournamentsByStatuses(const std::vector<std::string>& statuses);
    std::vector<TournamentInfo> GetAllTournaments();

    // arena_tournament_registrations
    bool InsertRegistration(uint32 tournamentId, uint32 playerGuid, const std::string& characterName);
    bool IsPlayerRegistered(uint32 tournamentId, uint32 playerGuid);
    bool SetEntryFeePaid(uint32 tournamentId, uint32 playerGuid, bool paid);
    bool DeleteRegistration(uint32 tournamentId, uint32 playerGuid);
    std::vector<TournamentRegistration> GetRegistrations(uint32 tournamentId);
    std::vector<std::pair<uint32,std::string>> GetConfirmedParticipants(uint32 tournamentId);

    // arena_tournament_rounds / matches
    uint32 InsertRound(uint32 tournamentId, uint32 roundNumber, const std::string& roundName);
    bool CompleteRound(uint32 roundId);
    uint32 GetLastCompletedRoundNumber(uint32 tournamentId);
    std::vector<std::pair<uint32,std::string>> GetRoundWinners(uint32 tournamentId, uint32 roundNumber);
    void InsertMatch(uint32 tournamentId, uint32 roundId, uint32 matchNumber,
                      uint32 p1Guid, uint32 p2Guid, const std::string& p1Name, const std::string& p2Name,
                      uint32 byeWinnerGuid = 0);
    bool SetMatchResult(uint32 matchId, uint32 winnerGuid, const std::string& status);
    bool GetMatchBasicInfo(uint32 matchId, uint32& tournamentId, uint32& roundId,
                            uint32& p1Guid, uint32& p2Guid, std::string& p1Name, std::string& p2Name);
    void GetRoundMatchCounts(uint32 roundId, uint32& total, uint32& completed);
    std::vector<TournamentRound> GetBracket(uint32 tournamentId);
    TournamentMatch GetPlayerCurrentMatch(uint32 playerGuid);
    
    struct ForfeitCandidate 
    { 
        uint32 matchId, tournamentId, p1Guid, p2Guid, attempts1, attempts2; 
    };
    std::vector<ForfeitCandidate> GetForfeitCandidates(uint32 maxAttempts);
    bool IncrementJoinAttempts(uint32 matchId, bool isPlayer1, uint32 currentValue);
    bool SetMatchBattleground(uint32 matchId, uint32 battlegroundId);
    bool GetMatchesForDisqualify(uint32 tournamentId, uint32 playerGuid,
                                  std::vector<std::tuple<uint32,uint32,uint32>>& outMatches);
    bool FindMatchByBattleground(uint32 battlegroundId, uint32& matchId, uint32& tournamentId);

    // arena_tournament_history
    bool InsertHistory(uint32 tournamentId, uint32 winnerGuid, const std::string& winnerName,
                        uint32 totalParticipants, const std::string& rewardsJson);
    std::string GetChampionName(uint32 tournamentId);

    // arena_tournament_player_stats
    void UpsertPlayerStats(uint32 playerGuid, bool won, uint32 goldEarned);
    std::map<std::string,uint32> GetPlayerStats(uint32 playerGuid);

    // arena_tournament_logs
    void InsertLog(uint32 tournamentId, const std::string& event);

    // misc
    std::string GetCharacterName(uint32 guid);
    bool AddOfflineGold(uint32 guid, uint32 amountCopper);
    bool IsPlayerInActiveTournament(uint32 playerGuid);
    static void EscapeString(std::string& str);
    
    // Leaderboard & Stats (for UI)
    std::vector<std::pair<uint32, std::string>> GetLeaderboard(uint32 limit = 10);
    struct GlobalStats { uint32 totalTournaments, totalMatches, totalPlayers; };
    GlobalStats GetGlobalStats();
    struct ChampionInfo { uint32 guid; std::string name; uint32 wins; };
    std::vector<ChampionInfo> GetTopChampions(uint32 limit = 10);
    std::vector<TournamentInfo> GetTournamentsByStatuses(const std::vector<std::string>& statuses);

private:
    TournamentRepository() = default;
};

#define sTournamentRepo TournamentRepository::instance()
