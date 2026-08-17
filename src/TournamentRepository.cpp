#include "TournamentRepository.h"
#include "Log.h"

TournamentRepository* TournamentRepository::_instance = nullptr;

TournamentRepository* TournamentRepository::instance()
{
    if (!_instance)
        _instance = new TournamentRepository();
    return _instance;
}

// arena_tournaments
uint32 TournamentRepository::InsertTournament(const std::string& name, const std::string& description, 
                                               uint32 entryFeeCopper, time_t regStart, time_t regEnd, 
                                               uint32 maxParticipants, uint32 createdBy,
                                               uint32 winnerGoldCopper, uint32 winnerItem, uint32 winnerTitle)
{
    std::string safeName = name;
    std::string safeDescription = description;
    CharacterDatabase.EscapeString(safeName);
    CharacterDatabase.EscapeString(safeDescription);
    
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournaments (name, description, entry_fee, registration_start, registration_end, max_participants, created_by, winner_reward_gold, winner_reward_item, winner_title) "
        "VALUES ('{}', '{}', {}, FROM_UNIXTIME({}), FROM_UNIXTIME({}), {}, {}, {}, {}, {})",
        safeName, safeDescription, entryFeeCopper, regStart, regEnd, maxParticipants, createdBy, winnerGoldCopper, winnerItem, winnerTitle
    );
    
    QueryResult result = CharacterDatabase.Query("SELECT LAST_INSERT_ID()");
    if (!result)
        return 0;
        
    return (*result)[0].Get<uint32>();
}

bool TournamentRepository::SetTournamentStatus(uint32 id, const std::string& status)
{
    if (status == "active")
    {
        CharacterDatabase.Execute(
            "UPDATE arena_tournaments SET status = 'active', tournament_start = NOW() WHERE id = {}",
            id
        );
    }
    else
    {
        CharacterDatabase.Execute(
            "UPDATE arena_tournaments SET status = '{}' WHERE id = {}",
            status, id
        );
    }
    return true;
}

bool TournamentRepository::SetTournamentFinished(uint32 id)
{
    CharacterDatabase.Execute(
        "UPDATE arena_tournaments SET status = 'finished', tournament_end = NOW() WHERE id = {}",
        id
    );
    return true;
}

bool TournamentRepository::IncrementParticipantCount(uint32 id)
{
    CharacterDatabase.Execute(
        "UPDATE arena_tournaments SET current_participants = current_participants + 1 WHERE id = {}",
        id
    );
    return true;
}

bool TournamentRepository::DecrementParticipantCount(uint32 id)
{
    CharacterDatabase.Execute(
        "UPDATE arena_tournaments SET current_participants = GREATEST(0, current_participants - 1) WHERE id = {}",
        id
    );
    return true;
}

TournamentInfo TournamentRepository::GetTournamentInfo(uint32 id)
{
    TournamentInfo info = {};
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, description, entry_fee, UNIX_TIMESTAMP(registration_start), UNIX_TIMESTAMP(registration_end), "
        "UNIX_TIMESTAMP(tournament_start), UNIX_TIMESTAMP(tournament_end), status, max_participants, "
        "current_participants, winner_reward_gold, winner_reward_item, winner_title, created_by, UNIX_TIMESTAMP(created_at) "
        "FROM arena_tournaments WHERE id = {}",
        id
    );
    
    if (!result)
        return info;
        
    auto fields = result->Fetch();
    info.id = fields[0].Get<uint32>();
    info.name = fields[1].Get<std::string>();
    info.description = fields[2].Get<std::string>();
    info.entryFee = fields[3].Get<uint32>();
    info.registrationStart = fields[4].Get<time_t>();
    info.registrationEnd = fields[5].Get<time_t>();
    info.tournamentStart = fields[6].Get<time_t>();
    info.tournamentEnd = fields[7].Get<time_t>();
    
    std::string statusStr = fields[8].Get<std::string>();
    if (statusStr == "registration") info.status = TOURNAMENT_STATUS_REGISTRATION;
    else if (statusStr == "ready") info.status = TOURNAMENT_STATUS_READY;
    else if (statusStr == "active") info.status = TOURNAMENT_STATUS_ACTIVE;
    else if (statusStr == "finished") info.status = TOURNAMENT_STATUS_FINISHED;
    else if (statusStr == "cancelled") info.status = TOURNAMENT_STATUS_CANCELLED;
    
    info.maxParticipants = fields[9].Get<uint32>();
    info.currentParticipants = fields[10].Get<uint32>();
    info.winnerRewardGold = fields[11].Get<uint32>();
    info.winnerRewardItem = fields[12].Get<uint32>();
    info.winnerTitle = fields[13].Get<uint32>();
    info.createdBy = fields[14].Get<uint32>();
    info.createdAt = fields[15].Get<time_t>();
    
    return info;
}

std::vector<TournamentInfo> TournamentRepository::GetTournamentsByStatuses(const std::vector<std::string>& statuses)
{
    std::vector<TournamentInfo> tournaments;
    
    if (statuses.empty())
        return tournaments;
        
    std::ostringstream statusQuery;
    for (size_t i = 0; i < statuses.size(); ++i)
    {
        if (i > 0) statusQuery << ", ";
        statusQuery << "'" << statuses[i] << "'";
    }
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, description, entry_fee, UNIX_TIMESTAMP(registration_start), UNIX_TIMESTAMP(registration_end), "
        "UNIX_TIMESTAMP(tournament_start), UNIX_TIMESTAMP(tournament_end), status, max_participants, "
        "current_participants, winner_reward_gold, winner_reward_item, winner_title, created_by, UNIX_TIMESTAMP(created_at) "
        "FROM arena_tournaments WHERE status IN ({}) ORDER BY created_at DESC",
        statusQuery.str()
    );
    
    if (!result)
        return tournaments;
        
    do
    {
        auto fields = result->Fetch();
        TournamentInfo info;
        info.id = fields[0].Get<uint32>();
        info.name = fields[1].Get<std::string>();
        info.description = fields[2].Get<std::string>();
        info.entryFee = fields[3].Get<uint32>();
        info.registrationStart = fields[4].Get<time_t>();
        info.registrationEnd = fields[5].Get<time_t>();
        info.tournamentStart = fields[6].Get<time_t>();
        info.tournamentEnd = fields[7].Get<time_t>();
        
        std::string statusStr = fields[8].Get<std::string>();
        if (statusStr == "registration") info.status = TOURNAMENT_STATUS_REGISTRATION;
        else if (statusStr == "ready") info.status = TOURNAMENT_STATUS_READY;
        else if (statusStr == "active") info.status = TOURNAMENT_STATUS_ACTIVE;
        else if (statusStr == "finished") info.status = TOURNAMENT_STATUS_FINISHED;
        else if (statusStr == "cancelled") info.status = TOURNAMENT_STATUS_CANCELLED;
        
        info.maxParticipants = fields[9].Get<uint32>();
        info.currentParticipants = fields[10].Get<uint32>();
        info.winnerRewardGold = fields[11].Get<uint32>();
        info.winnerRewardItem = fields[12].Get<uint32>();
        info.winnerTitle = fields[13].Get<uint32>();
        info.createdBy = fields[14].Get<uint32>();
        info.createdAt = fields[15].Get<time_t>();
        
        tournaments.push_back(info);
    } while (result->NextRow());
    
    return tournaments;
}

std::vector<TournamentInfo> TournamentRepository::GetAllTournaments()
{
    std::vector<TournamentInfo> tournaments;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, description, entry_fee, UNIX_TIMESTAMP(registration_start), UNIX_TIMESTAMP(registration_end), "
        "UNIX_TIMESTAMP(tournament_start), UNIX_TIMESTAMP(tournament_end), status, max_participants, "
        "current_participants, winner_reward_gold, winner_reward_item, winner_title, created_by, UNIX_TIMESTAMP(created_at) "
        "FROM arena_tournaments ORDER BY created_at DESC"
    );
    
    if (!result)
        return tournaments;
        
    do
    {
        auto fields = result->Fetch();
        TournamentInfo info;
        info.id = fields[0].Get<uint32>();
        info.name = fields[1].Get<std::string>();
        info.description = fields[2].Get<std::string>();
        info.entryFee = fields[3].Get<uint32>();
        info.registrationStart = fields[4].Get<time_t>();
        info.registrationEnd = fields[5].Get<time_t>();
        info.tournamentStart = fields[6].Get<time_t>();
        info.tournamentEnd = fields[7].Get<time_t>();
        
        std::string statusStr = fields[8].Get<std::string>();
        if (statusStr == "registration") info.status = TOURNAMENT_STATUS_REGISTRATION;
        else if (statusStr == "ready") info.status = TOURNAMENT_STATUS_READY;
        else if (statusStr == "active") info.status = TOURNAMENT_STATUS_ACTIVE;
        else if (statusStr == "finished") info.status = TOURNAMENT_STATUS_FINISHED;
        else if (statusStr == "cancelled") info.status = TOURNAMENT_STATUS_CANCELLED;
        
        info.maxParticipants = fields[9].Get<uint32>();
        info.currentParticipants = fields[10].Get<uint32>();
        info.winnerRewardGold = fields[11].Get<uint32>();
        info.winnerRewardItem = fields[12].Get<uint32>();
        info.winnerTitle = fields[13].Get<uint32>();
        info.createdBy = fields[14].Get<uint32>();
        info.createdAt = fields[15].Get<time_t>();
        
        tournaments.push_back(info);
    } while (result->NextRow());
    
    return tournaments;
}

// arena_tournament_registrations
bool TournamentRepository::InsertRegistration(uint32 tournamentId, uint32 playerGuid, const std::string& characterName)
{
    std::string safeName = characterName;
    CharacterDatabase.EscapeString(safeName);
    
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_registrations (tournament_id, player_guid, character_name, entry_fee_paid, status) "
        "VALUES ({}, {}, '{}', 1, 'confirmed')",
        tournamentId, playerGuid, safeName
    );
    return true;
}

bool TournamentRepository::IsPlayerRegistered(uint32 tournamentId, uint32 playerGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT id FROM arena_tournament_registrations WHERE tournament_id = {} AND player_guid = {}",
        tournamentId, playerGuid
    );
    return result != nullptr;
}

bool TournamentRepository::SetEntryFeePaid(uint32 tournamentId, uint32 playerGuid, bool paid)
{
    CharacterDatabase.Execute(
        "UPDATE arena_tournament_registrations SET entry_fee_paid = {} WHERE tournament_id = {} AND player_guid = {}",
        paid ? 1 : 0, tournamentId, playerGuid
    );
    return true;
}

bool TournamentRepository::DeleteRegistration(uint32 tournamentId, uint32 playerGuid)
{
    CharacterDatabase.Execute(
        "DELETE FROM arena_tournament_registrations WHERE tournament_id = {} AND player_guid = {}",
        tournamentId, playerGuid
    );
    return true;
}

std::vector<TournamentRegistration> TournamentRepository::GetRegistrations(uint32 tournamentId)
{
    std::vector<TournamentRegistration> registrations;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, tournament_id, player_guid, character_name, UNIX_TIMESTAMP(registration_time), entry_fee_paid, confirmed "
        "FROM arena_tournament_registrations WHERE tournament_id = {} ORDER BY registration_time",
        tournamentId
    );
    
    if (!result)
        return registrations;
        
    do
    {
        auto fields = result->Fetch();
        TournamentRegistration reg;
        reg.id = fields[0].Get<uint32>();
        reg.tournamentId = fields[1].Get<uint32>();
        reg.playerGuid = fields[2].Get<uint32>();
        reg.characterName = fields[3].Get<std::string>();
        reg.registrationTime = fields[4].Get<time_t>();
        reg.entryFeePaid = fields[5].Get<bool>();
        reg.confirmed = fields[6].Get<bool>();
        registrations.push_back(reg);
    } while (result->NextRow());
    
    return registrations;
}

std::vector<std::pair<uint32,std::string>> TournamentRepository::GetConfirmedParticipants(uint32 tournamentId)
{
    std::vector<std::pair<uint32,std::string>> participants;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT player_guid, character_name FROM arena_tournament_registrations "
        "WHERE tournament_id = {} AND status = 'confirmed' ORDER BY registration_time",
        tournamentId
    );
    
    if (!result)
        return participants;
        
    do
    {
        auto fields = result->Fetch();
        participants.emplace_back(fields[0].Get<uint32>(), fields[1].Get<std::string>());
    } while (result->NextRow());
    
    return participants;
}

// arena_tournament_rounds / matches
uint32 TournamentRepository::InsertRound(uint32 tournamentId, uint32 roundNumber, const std::string& roundName)
{
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_rounds (tournament_id, round_number, round_name, status) "
        "VALUES ({}, {}, '{}', 'active')",
        tournamentId, roundNumber, roundName
    );
    
    QueryResult result = CharacterDatabase.Query("SELECT LAST_INSERT_ID()");
    if (!result)
        return 0;
        
    return (*result)[0].Get<uint32>();
}

bool TournamentRepository::CompleteRound(uint32 roundId)
{
    CharacterDatabase.Execute(
        "UPDATE arena_tournament_rounds SET status = 'completed', end_time = NOW() WHERE id = {}",
        roundId
    );
    return true;
}

uint32 TournamentRepository::GetLastCompletedRoundNumber(uint32 tournamentId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT MAX(round_number) FROM arena_tournament_rounds WHERE tournament_id = {} AND status = 'completed'",
        tournamentId
    );
    
    if (!result)
        return 0;
        
    return (*result)[0].Get<uint32>();
}

std::vector<std::pair<uint32,std::string>> TournamentRepository::GetRoundWinners(uint32 tournamentId, uint32 roundNumber)
{
    std::vector<std::pair<uint32,std::string>> winners;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT winner_guid, '' as winner_name FROM arena_tournament_matches "
        "WHERE tournament_id = {} AND round_number = {} AND status = 'completed' AND winner_guid > 0",
        tournamentId, roundNumber
    );
    
    if (!result)
        return winners;
        
    do
    {
        auto fields = result->Fetch();
        winners.emplace_back(fields[0].Get<uint32>(), fields[1].Get<std::string>());
    } while (result->NextRow());
    
    return winners;
}

void TournamentRepository::InsertMatch(uint32 tournamentId, uint32 roundId, uint32 matchNumber,
                                        uint32 p1Guid, uint32 p2Guid, const std::string& p1Name, const std::string& p2Name,
                                        uint32 byeWinnerGuid)
{
    std::string safeP1Name = p1Name;
    std::string safeP2Name = p2Name;
    CharacterDatabase.EscapeString(safeP1Name);
    CharacterDatabase.EscapeString(safeP2Name);
    
    if (p2Guid == 0 && byeWinnerGuid > 0)
    {
        // Bye match - automatically completed
        CharacterDatabase.Execute(
            "INSERT INTO arena_tournament_matches (tournament_id, round_id, match_number, player1_guid, player2_guid, "
            "player1_name, player2_name, winner_guid, status) "
            "VALUES ({}, {}, {}, {}, 0, '{}', '', {}, 'completed')",
            tournamentId, roundId, matchNumber, p1Guid, safeP1Name, p1Guid
        );
    }
    else
    {
        CharacterDatabase.Execute(
            "INSERT INTO arena_tournament_matches (tournament_id, round_id, match_number, player1_guid, player2_guid, "
            "player1_name, player2_name, status) "
            "VALUES ({}, {}, {}, {}, {}, '{}', '{}', 'pending')",
            tournamentId, roundId, matchNumber, p1Guid, p2Guid, safeP1Name, safeP2Name
        );
    }
}

bool TournamentRepository::SetMatchResult(uint32 matchId, uint32 winnerGuid, const std::string& status)
{
    CharacterDatabase.Execute(
        "UPDATE arena_tournament_matches SET winner_guid = {}, status = '{}', match_end = NOW() WHERE id = {}",
        winnerGuid, status, matchId
    );
    return true;
}

bool TournamentRepository::GetMatchBasicInfo(uint32 matchId, uint32& tournamentId, uint32& roundId,
                                              uint32& p1Guid, uint32& p2Guid, std::string& p1Name, std::string& p2Name)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT tournament_id, round_id, player1_guid, player2_guid, player1_name, player2_name "
        "FROM arena_tournament_matches WHERE id = {}",
        matchId
    );
    
    if (!result)
        return false;
        
    auto fields = result->Fetch();
    tournamentId = fields[0].Get<uint32>();
    roundId = fields[1].Get<uint32>();
    p1Guid = fields[2].Get<uint32>();
    p2Guid = fields[3].Get<uint32>();
    p1Name = fields[4].Get<std::string>();
    p2Name = fields[5].Get<std::string>();
    
    return true;
}

void TournamentRepository::GetRoundMatchCounts(uint32 roundId, uint32& total, uint32& completed)
{
    total = 0;
    completed = 0;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*) as total, SUM(CASE WHEN status = 'completed' THEN 1 ELSE 0 END) as completed "
        "FROM arena_tournament_matches WHERE round_id = {}",
        roundId
    );
    
    if (result)
    {
        auto fields = result->Fetch();
        total = fields[0].Get<uint32>();
        completed = fields[1].Get<uint32>();
    }
}

std::vector<TournamentRound> TournamentRepository::GetBracket(uint32 tournamentId)
{
    std::vector<TournamentRound> rounds;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, round_number, round_name, status, UNIX_TIMESTAMP(start_time), UNIX_TIMESTAMP(end_time) "
        "FROM arena_tournament_rounds WHERE tournament_id = {} ORDER BY round_number",
        tournamentId
    );
    
    if (!result)
        return rounds;
        
    do
    {
        auto fields = result->Fetch();
        TournamentRound round;
        round.id = fields[0].Get<uint32>();
        round.tournamentId = tournamentId;
        round.roundNumber = fields[1].Get<uint32>();
        round.roundName = fields[2].Get<std::string>();
        
        std::string statusStr = fields[3].Get<std::string>();
        if (statusStr == "pending") round.status = ROUND_STATUS_PENDING;
        else if (statusStr == "active") round.status = ROUND_STATUS_ACTIVE;
        else if (statusStr == "completed") round.status = ROUND_STATUS_COMPLETED;
        
        round.startTime = fields[4].Get<time_t>();
        round.endTime = fields[5].Get<time_t>();
        
        // Get matches for this round
        QueryResult matchResult = CharacterDatabase.Query(
            "SELECT id, tournament_id, round_id, match_number, player1_guid, player2_guid, player1_name, player2_name, "
            "winner_guid, status, UNIX_TIMESTAMP(match_start), UNIX_TIMESTAMP(match_end), "
            "join_attempts_player1, join_attempts_player2, battleground_id "
            "FROM arena_tournament_matches WHERE round_id = {} ORDER BY match_number",
            round.id
        );
        
        if (matchResult)
        {
            do
            {
                auto mFields = matchResult->Fetch();
                TournamentMatch match;
                match.id = mFields[0].Get<uint32>();
                match.tournamentId = mFields[1].Get<uint32>();
                match.roundId = mFields[2].Get<uint32>();
                match.matchNumber = mFields[3].Get<uint32>();
                match.player1Guid = mFields[4].Get<uint32>();
                match.player2Guid = mFields[5].Get<uint32>();
                match.player1Name = mFields[6].Get<std::string>();
                match.player2Name = mFields[7].Get<std::string>();
                match.winnerGuid = mFields[8].Get<uint32>();
                
                std::string matchStatusStr = mFields[9].Get<std::string>();
                if (matchStatusStr == "pending") match.status = MATCH_STATUS_PENDING;
                else if (matchStatusStr == "active") match.status = MATCH_STATUS_ACTIVE;
                else if (matchStatusStr == "completed") match.status = MATCH_STATUS_COMPLETED;
                else if (matchStatusStr == "forfeit") match.status = MATCH_STATUS_FORFEIT;
                
                match.matchStart = mFields[10].Get<time_t>();
                match.matchEnd = mFields[11].Get<time_t>();
                match.joinAttemptsPlayer1 = mFields[12].Get<uint32>();
                match.joinAttemptsPlayer2 = mFields[13].Get<uint32>();
                match.battlegroundId = mFields[14].Get<uint32>();
                
                round.matches.push_back(match);
            } while (matchResult->NextRow());
        }
        
        rounds.push_back(round);
    } while (result->NextRow());
    
    return rounds;
}

TournamentMatch TournamentRepository::GetPlayerCurrentMatch(uint32 playerGuid)
{
    TournamentMatch match = {};
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, tournament_id, round_id, match_number, player1_guid, player2_guid, player1_name, player2_name, "
        "winner_guid, status, UNIX_TIMESTAMP(match_start), UNIX_TIMESTAMP(match_end), "
        "join_attempts_player1, join_attempts_player2, battleground_id "
        "FROM arena_tournament_matches "
        "WHERE (player1_guid = {} OR player2_guid = {}) AND status IN ('pending', 'active')",
        playerGuid, playerGuid
    );
    
    if (!result)
        return match;
        
    auto fields = result->Fetch();
    match.id = fields[0].Get<uint32>();
    match.tournamentId = fields[1].Get<uint32>();
    match.roundId = fields[2].Get<uint32>();
    match.matchNumber = fields[3].Get<uint32>();
    match.player1Guid = fields[4].Get<uint32>();
    match.player2Guid = fields[5].Get<uint32>();
    match.player1Name = fields[6].Get<std::string>();
    match.player2Name = fields[7].Get<std::string>();
    match.winnerGuid = fields[8].Get<uint32>();
    
    std::string statusStr = fields[9].Get<std::string>();
    if (statusStr == "pending") match.status = MATCH_STATUS_PENDING;
    else if (statusStr == "active") match.status = MATCH_STATUS_ACTIVE;
    else if (statusStr == "completed") match.status = MATCH_STATUS_COMPLETED;
    else if (statusStr == "forfeit") match.status = MATCH_STATUS_FORFEIT;
    
    match.matchStart = fields[10].Get<time_t>();
    match.matchEnd = fields[11].Get<time_t>();
    match.joinAttemptsPlayer1 = fields[12].Get<uint32>();
    match.joinAttemptsPlayer2 = fields[13].Get<uint32>();
    match.battlegroundId = fields[14].Get<uint32>();
    
    return match;
}

std::vector<TournamentRepository::ForfeitCandidate> TournamentRepository::GetForfeitCandidates(uint32 maxAttempts)
{
    std::vector<ForfeitCandidate> candidates;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, tournament_id, player1_guid, player2_guid, join_attempts_player1, join_attempts_player2 "
        "FROM arena_tournament_matches WHERE status = 'active' AND "
        "(join_attempts_player1 >= {} OR join_attempts_player2 >= {})",
        maxAttempts, maxAttempts
    );
    
    if (!result)
        return candidates;
        
    do
    {
        auto fields = result->Fetch();
        ForfeitCandidate candidate;
        candidate.matchId = fields[0].Get<uint32>();
        candidate.tournamentId = fields[1].Get<uint32>();
        candidate.p1Guid = fields[2].Get<uint32>();
        candidate.p2Guid = fields[3].Get<uint32>();
        candidate.attempts1 = fields[4].Get<uint32>();
        candidate.attempts2 = fields[5].Get<uint32>();
        candidates.push_back(candidate);
    } while (result->NextRow());
    
    return candidates;
}

bool TournamentRepository::IncrementJoinAttempts(uint32 matchId, bool isPlayer1, uint32 currentValue)
{
    if (isPlayer1)
    {
        CharacterDatabase.Execute(
            "UPDATE arena_tournament_matches SET join_attempts_player1 = {} WHERE id = {}",
            currentValue + 1, matchId
        );
    }
    else
    {
        CharacterDatabase.Execute(
            "UPDATE arena_tournament_matches SET join_attempts_player2 = {} WHERE id = {}",
            currentValue + 1, matchId
        );
    }
    return true;
}

bool TournamentRepository::SetMatchBattleground(uint32 matchId, uint32 battlegroundId)
{
    CharacterDatabase.Execute(
        "UPDATE arena_tournament_matches SET battleground_id = {}, status = 'active', match_start = NOW() WHERE id = {}",
        battlegroundId, matchId
    );
    return true;
}

bool TournamentRepository::GetMatchesForDisqualify(uint32 tournamentId, uint32 playerGuid,
                                                    std::vector<std::tuple<uint32,uint32,uint32>>& outMatches)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, player1_guid, player2_guid FROM arena_tournament_matches "
        "WHERE tournament_id = {} AND (player1_guid = {} OR player2_guid = {}) AND status IN ('pending', 'active')",
        tournamentId, playerGuid, playerGuid
    );
    
    if (!result)
        return false;
        
    do
    {
        auto fields = result->Fetch();
        outMatches.emplace_back(fields[0].Get<uint32>(), fields[1].Get<uint32>(), fields[2].Get<uint32>());
    } while (result->NextRow());
    
    return true;
}

bool TournamentRepository::FindMatchByBattleground(uint32 battlegroundId, uint32& matchId, uint32& tournamentId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, tournament_id FROM arena_tournament_matches WHERE battleground_id = {}",
        battlegroundId
    );
    
    if (!result)
        return false;
        
    auto fields = result->Fetch();
    matchId = fields[0].Get<uint32>();
    tournamentId = fields[1].Get<uint32>();
    
    return true;
}

// arena_tournament_history
bool TournamentRepository::InsertHistory(uint32 tournamentId, uint32 winnerGuid, const std::string& winnerName,
                                          uint32 totalParticipants, const std::string& rewardsJson)
{
    std::string safeWinnerName = winnerName;
    std::string safeRewardsJson = rewardsJson;
    CharacterDatabase.EscapeString(safeWinnerName);
    CharacterDatabase.EscapeString(safeRewardsJson);
    
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_history (tournament_id, winner_guid, winner_name, total_participants, rewards_json) "
        "VALUES ({}, {}, '{}', {}, '{}')",
        tournamentId, winnerGuid, safeWinnerName, totalParticipants, safeRewardsJson
    );
    return true;
}

std::string TournamentRepository::GetChampionName(uint32 tournamentId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT winner_name FROM arena_tournament_history WHERE tournament_id = {}",
        tournamentId
    );
    
    if (!result)
        return "";
        
    return (*result)[0].Get<std::string>();
}

// arena_tournament_player_stats
void TournamentRepository::UpsertPlayerStats(uint32 playerGuid, bool won, uint32 goldEarned)
{
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_player_stats (player_guid, tournaments_won, total_gold_earned) "
        "VALUES ({}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "tournaments_won = tournaments_won + {}, total_gold_earned = total_gold_earned + {}",
        playerGuid, won ? 1 : 0, goldEarned, won ? 1 : 0, goldEarned
    );
}

std::map<std::string,uint32> TournamentRepository::GetPlayerStats(uint32 playerGuid)
{
    std::map<std::string,uint32> stats;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT tournaments_won, total_gold_earned FROM arena_tournament_player_stats WHERE player_guid = {}",
        playerGuid
    );
    
    if (result)
    {
        auto fields = result->Fetch();
        stats["tournaments_won"] = fields[0].Get<uint32>();
        stats["total_gold_earned"] = fields[1].Get<uint32>();
    }
    else
    {
        stats["tournaments_won"] = 0;
        stats["total_gold_earned"] = 0;
    }
    
    return stats;
}

// arena_tournament_logs
void TournamentRepository::InsertLog(uint32 tournamentId, const std::string& event)
{
    std::string safeEvent = event;
    CharacterDatabase.EscapeString(safeEvent);
    
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_logs (tournament_id, event) VALUES ({}, '{}')",
        tournamentId, safeEvent
    );
}

// misc
std::string TournamentRepository::GetCharacterName(uint32 guid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT name FROM characters WHERE guid = {}",
        guid
    );
    
    if (!result)
        return "Unknown";
        
    return (*result)[0].Get<std::string>();
}

bool TournamentRepository::AddOfflineGold(uint32 guid, uint32 amountCopper)
{
    CharacterDatabase.Execute(
        "UPDATE characters SET money = money + {} WHERE guid = {}",
        amountCopper, guid
    );
    return true;
}

bool TournamentRepository::IsPlayerInActiveTournament(uint32 playerGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT r.id FROM arena_tournament_registrations r "
        "JOIN arena_tournaments t ON r.tournament_id = t.id "
        "WHERE r.player_guid = {} AND t.status = 'active'",
        playerGuid
    );
    
    return result != nullptr;
}

void TournamentRepository::EscapeString(std::string& str)
{
    CharacterDatabase.EscapeString(str);
}

// Leaderboard & Stats (for UI)
std::vector<std::pair<uint32, std::string>> TournamentRepository::GetLeaderboard(uint32 limit)
{
    std::vector<std::pair<uint32, std::string>> leaderboard;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT c.guid, c.name FROM characters c "
        "JOIN arena_tournament_player_stats s ON c.guid = s.player_guid "
        "ORDER BY s.tournaments_won DESC, s.total_gold_earned DESC "
        "LIMIT {}",
        limit
    );
    
    if (result)
    {
        do
        {
            auto fields = result->Fetch();
            leaderboard.push_back({fields[0].Get<uint32>(), fields[1].Get<std::string>()});
        } while (result->NextRow());
    }
    
    return leaderboard;
}

TournamentRepository::GlobalStats TournamentRepository::GetGlobalStats()
{
    GlobalStats stats = {0, 0, 0};
    
    auto result = CharacterDatabase.Query("SELECT COUNT(*) FROM arena_tournaments");
    if (result) stats.totalTournaments = (*result)[0].Get<uint32>();
    
    result = CharacterDatabase.Query("SELECT COUNT(*) FROM arena_tournament_matches");
    if (result) stats.totalMatches = (*result)[0].Get<uint32>();
    
    result = CharacterDatabase.Query("SELECT COUNT(DISTINCT player_guid) FROM arena_tournament_registrations");
    if (result) stats.totalPlayers = (*result)[0].Get<uint32>();
    
    return stats;
}

std::vector<TournamentRepository::ChampionInfo> TournamentRepository::GetTopChampions(uint32 limit)
{
    std::vector<ChampionInfo> champions;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT c.guid, c.name, s.tournaments_won "
        "FROM characters c "
        "JOIN arena_tournament_player_stats s ON c.guid = s.player_guid "
        "WHERE s.tournaments_won > 0 "
        "ORDER BY s.tournaments_won DESC "
        "LIMIT {}",
        limit
    );
    
    if (result)
    {
        do
        {
            auto fields = result->Fetch();
            champions.push_back({fields[0].Get<uint32>(), fields[1].Get<std::string>(), fields[2].Get<uint32>()});
        } while (result->NextRow());
    }
    
    return champions;
}
