#include "TournamentSystem.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Chat.h"
#include "World.h"
#include "WorldSession.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "Language.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "Config.h"
#include "Util.h" // for Acore::StringFormat
#include <algorithm>
#include <random>
#include <sstream>

TournamentSystem* TournamentSystem::_instance = nullptr;

TournamentSystem* TournamentSystem::instance()
{
    if (!_instance)
        _instance = new TournamentSystem();
    return _instance;
}

uint32 TournamentSystem::CreateTournament(const std::string& name, const std::string& description,
    uint32 entryFee, uint32 registrationDurationHours, uint32 maxParticipants, uint32 createdBy,
    uint32 winnerRewardGold, uint32 winnerRewardItem, uint32 winnerTitle)
{
    time_t now = time(nullptr);
    time_t registrationEnd = now + (registrationDurationHours * 3600);
    
    // Use config defaults if not specified
    if (winnerRewardGold == 0)
        winnerRewardGold = sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardGold", 500) * 10000;
    if (winnerRewardItem == 0)
        winnerRewardItem = sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardItem", 0);
    if (winnerTitle == 0)
        winnerTitle = sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerTitle", 0);
    
    std::string safeName = name;
    std::string safeDescription = description;
    CharacterDatabase.EscapeString(safeName);
    CharacterDatabase.EscapeString(safeDescription);
    
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournaments (name, description, entry_fee, registration_start, registration_end, max_participants, created_by, winner_reward_gold, winner_reward_item, winner_title) "
        "VALUES ('{}', '{}', {}, FROM_UNIXTIME({}), FROM_UNIXTIME({}), {}, {}, {}, {}, {})",
        safeName, safeDescription, entryFee, now, registrationEnd, maxParticipants, createdBy, winnerRewardGold, winnerRewardItem, winnerTitle
    );
    
    QueryResult result = CharacterDatabase.Query("SELECT LAST_INSERT_ID()");
    if (!result)
        return 0;
        
    uint32 tournamentId = (*result)[0].Get<uint32>();
    
    // Announce tournament creation
    std::ostringstream announce;
    announce << "|cffFFD700[Tournament]|r New tournament '|cff00FF00" << name 
             << "|r' has been created! Entry fee: |cffFFD700" << (entryFee / 10000) 
             << "|r gold. Registration ends in |cff00FFFF" << registrationDurationHours << "|r hours!";
    SendTournamentAnnouncement(announce.str());
    
    LOG_INFO("tournament", "Tournament '{}' created with ID {} by player {}", name, tournamentId, createdBy);
    
    return tournamentId;
}

bool TournamentSystem::RegisterPlayer(uint32 tournamentId, Player* player)
{
    if (!player)
        return false;
        
    std::string errorMsg;
    if (!CanPlayerRegister(tournamentId, player, errorMsg))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("Registration failed: {}", errorMsg);
        return false;
    }
    
    // Check if player already registered
    QueryResult checkResult = CharacterDatabase.Query(
        "SELECT id FROM arena_tournament_registrations WHERE tournament_id = {} AND player_guid = {}",
        tournamentId, player->GetGUID().GetCounter()
    );
    
    if (checkResult)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("You are already registered for this tournament!");
        return false;
    }
    
    // Get tournament info for entry fee
    QueryResult tournamentResult = CharacterDatabase.Query(
        "SELECT entry_fee, current_participants, max_participants FROM arena_tournaments WHERE id = {}",
        tournamentId
    );
    
    if (!tournamentResult)
        return false;
        
    auto fields = tournamentResult->Fetch();
    uint32 entryFee = fields[0].Get<uint32>();
    uint32 currentParticipants = fields[1].Get<uint32>();
    uint32 maxParticipants = fields[2].Get<uint32>();
    
    // Check if tournament is full
    if (currentParticipants >= maxParticipants)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("Tournament is full! Maximum participants: {}", maxParticipants);
        return false;
    }
    
    // Check if player has enough gold
    if (!HasEnoughGold(player, entryFee))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("You need {} gold to register for this tournament!", entryFee / 10000);
        return false;
    }
    
    // Deduct entry fee
    player->ModifyMoney(-int32(entryFee));
    
    // Register player
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_registrations (tournament_id, player_guid, character_name, entry_fee_paid, status) "
        "VALUES ({}, {}, '{}', 1, 'confirmed')",
        tournamentId, player->GetGUID().GetCounter(), player->GetName()
    );
    
    // Update participant count
    CharacterDatabase.Execute(
        "UPDATE arena_tournaments SET current_participants = current_participants + 1 WHERE id = {}",
        tournamentId
    );
    
    ChatHandler(player->GetSession()).PSendSysMessage("|cff00FF00Successfully registered for tournament! Entry fee of {} gold has been deducted.|r", entryFee / 10000);
    
    LOG_INFO("tournament", "Player {} registered for tournament {}", player->GetName(), tournamentId);
    
    return true;
}

bool TournamentSystem::StartTournament(uint32 tournamentId)
{
    // Check tournament exists and is in registration phase
    QueryResult result = CharacterDatabase.Query(
        "SELECT name, status, current_participants FROM arena_tournaments WHERE id = {}",
        tournamentId
    );
    
    if (!result)
        return false;
        
    auto fields = result->Fetch();
    std::string name = fields[0].Get<std::string>();
    std::string status = fields[1].Get<std::string>();
    uint32 participants = fields[2].Get<uint32>();
    
    if (status != "registration" && status != "ready")
    {
        LOG_ERROR("tournament", "Cannot start tournament {} - invalid status: {}", tournamentId, status);
        return false;
    }
    
    if (participants < 2)
    {
        LOG_ERROR("tournament", "Cannot start tournament {} - not enough participants: {}", tournamentId, participants);
        return false;
    }
    
    // Update tournament status
    CharacterDatabase.Execute(
        "UPDATE arena_tournaments SET status = 'active', tournament_start = NOW() WHERE id = {}",
        tournamentId
    );
    
    // Generate bracket
    if (!GenerateBracket(tournamentId))
    {
        LOG_ERROR("tournament", "Failed to generate bracket for tournament {}", tournamentId);
        return false;
    }
    
    // Announce tournament start
    std::ostringstream announce;
    announce << "|cffFFD700[Tournament]|r Tournament '|cff00FF00" << name 
             << "|r' has started with |cffFFD700" << participants << "|r participants! Good luck!";
    SendTournamentAnnouncement(announce.str());
    
    LOG_INFO("tournament", "Tournament '{}' (ID: {}) started with {} participants", name, tournamentId, participants);
    
    return true;
}

bool TournamentSystem::GenerateBracket(uint32 tournamentId)
{
    // Get all confirmed registrations
    QueryResult result = CharacterDatabase.Query(
        "SELECT player_guid, character_name FROM arena_tournament_registrations "
        "WHERE tournament_id = {} AND status = 'confirmed' ORDER BY registration_time",
        tournamentId
    );
    
    if (!result)
        return false;
        
    std::vector<std::pair<uint32, std::string>> participants;
    do
    {
        auto fields = result->Fetch();
        participants.emplace_back(fields[0].Get<uint32>(), fields[1].Get<std::string>());
    } while (result->NextRow());
    
    if (participants.size() < 2)
        return false;
        
    // Shuffle participants for random seeding
    std::vector<uint32> playerGuids;
    std::map<uint32, std::string> playerNames;
    
    for (const auto& p : participants)
    {
        playerGuids.push_back(p.first);
        playerNames[p.first] = p.second;
    }
    
    ShuffleParticipants(playerGuids);
    
    // Calculate rounds needed
    uint32 roundsNeeded = CalculateRoundsNeeded(participants.size());
    
    // Create first round
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_rounds (tournament_id, round_number, round_name, status) "
        "VALUES ({}, 1, '{}', 'active')",
        tournamentId, GenerateRoundName(participants.size(), 1)
    );
    
    QueryResult roundResult = CharacterDatabase.Query("SELECT LAST_INSERT_ID()");
    if (!roundResult)
        return false;
        
    uint32 roundId = (*roundResult)[0].Get<uint32>();
    
    // Create matches for first round
    uint32 matchNumber = 1;
    for (size_t i = 0; i < playerGuids.size(); i += 2)
    {
        if (i + 1 < playerGuids.size())
        {
            // Normal match
            CharacterDatabase.Execute(
                "INSERT INTO arena_tournament_matches (tournament_id, round_id, match_number, player1_guid, player2_guid, player1_name, player2_name, status) "
                "VALUES ({}, {}, {}, {}, {}, '{}', '{}', 'pending')",
                tournamentId, roundId, matchNumber, playerGuids[i], playerGuids[i + 1],
                playerNames[playerGuids[i]], playerNames[playerGuids[i + 1]]
            );
        }
        else
        {
            // Bye (automatic advancement)
            CharacterDatabase.Execute(
                "INSERT INTO arena_tournament_matches (tournament_id, round_id, match_number, player1_guid, player2_guid, player1_name, player2_name, winner_guid, status) "
                "VALUES ({}, {}, {}, {}, 0, '{}', 'BYE', {}, 'completed')",
                tournamentId, roundId, matchNumber, playerGuids[i], playerNames[playerGuids[i]], playerGuids[i]
            );
        }
        matchNumber++;
    }
    
    LOG_INFO("tournament", "Generated bracket for tournament {} with {} participants and {} rounds", 
             tournamentId, participants.size(), roundsNeeded);
    
    return true;
}

bool TournamentSystem::ProcessMatchResult(uint32 matchId, uint32 winnerGuid)
{
    // Get match info
    QueryResult result = CharacterDatabase.Query(
        "SELECT tournament_id, round_id, player1_guid, player2_guid, player1_name, player2_name "
        "FROM arena_tournament_matches WHERE id = {}",
        matchId
    );
    
    if (!result)
        return false;
        
    auto fields = result->Fetch();
    uint32 tournamentId = fields[0].Get<uint32>();
    uint32 roundId = fields[1].Get<uint32>();
    uint32 player1Guid = fields[2].Get<uint32>();
    uint32 player2Guid = fields[3].Get<uint32>();
    std::string player1Name = fields[4].Get<std::string>();
    std::string player2Name = fields[5].Get<std::string>();
    
    // Validate winner
    if (winnerGuid != player1Guid && winnerGuid != player2Guid)
        return false;
        
    // Update match result
    CharacterDatabase.Execute(
        "UPDATE arena_tournament_matches SET winner_guid = {}, status = 'completed', match_end = NOW() WHERE id = {}",
        winnerGuid, matchId
    );
    
    std::string winnerName = (winnerGuid == player1Guid) ? player1Name : player2Name;
    std::string loserName = (winnerGuid == player1Guid) ? player2Name : player1Name;
    
    // Update player stats
    UpdatePlayerStats(winnerGuid, true);
    uint32 loserGuid = (winnerGuid == player1Guid) ? player2Guid : player1Guid;
    UpdatePlayerStats(loserGuid, false);
    
    LOG_INFO("tournament", "Match {} completed: {} defeated {}", matchId, winnerName, loserName);
    
    // Check if round is complete
    QueryResult roundCheck = CharacterDatabase.Query(
        "SELECT COUNT(*) as total, SUM(CASE WHEN status = 'completed' THEN 1 ELSE 0 END) as completed "
        "FROM arena_tournament_matches WHERE round_id = {}",
        roundId
    );
    
    if (roundCheck)
    {
        auto roundFields = roundCheck->Fetch();
        uint32 totalMatches = roundFields[0].Get<uint32>();
        uint32 completedMatches = roundFields[1].Get<uint32>();
        
        if (completedMatches >= totalMatches)
        {
            // Round complete, start next round or finish tournament
            CharacterDatabase.Execute(
                "UPDATE arena_tournament_rounds SET status = 'completed', end_time = NOW() WHERE id = {}",
                roundId
            );
            
            if (!StartNextRound(tournamentId))
            {
                // Tournament finished
                FinishTournament(tournamentId, winnerGuid);
            }
        }
    }
    
    return true;
}

bool TournamentSystem::StartNextRound(uint32 tournamentId)
{
    // Get winners from last round
    QueryResult lastRoundResult = CharacterDatabase.Query(
        "SELECT r.round_number FROM arena_tournament_rounds r "
        "WHERE r.tournament_id = {} AND r.status = 'completed' "
        "ORDER BY r.round_number DESC LIMIT 1",
        tournamentId
    );
    
    if (!lastRoundResult)
        return false;
        
    uint32 lastRoundNumber = (*lastRoundResult)[0].Get<uint32>();
    
    // Get winners from last round
    QueryResult winnersResult = CharacterDatabase.Query(
        "SELECT m.winner_guid, reg.character_name "
        "FROM arena_tournament_matches m "
        "JOIN arena_tournament_rounds r ON m.round_id = r.id "
        "JOIN arena_tournament_registrations reg ON m.winner_guid = reg.player_guid AND reg.tournament_id = m.tournament_id "
        "WHERE r.tournament_id = {} AND r.round_number = {} AND m.winner_guid > 0",
        tournamentId, lastRoundNumber
    );
    
    if (!winnersResult)
        return false;
        
    std::vector<std::pair<uint32, std::string>> winners;
    do
    {
        auto fields = winnersResult->Fetch();
        winners.emplace_back(fields[0].Get<uint32>(), fields[1].Get<std::string>());
    } while (winnersResult->NextRow());
    
    if (winners.size() < 2)
    {
        // Tournament finished with single winner
        if (winners.size() == 1)
            return false; // This will trigger tournament finish
        return false;
    }
    
    // Create next round
    uint32 nextRoundNumber = lastRoundNumber + 1;
    std::string roundName = GenerateRoundName(winners.size(), nextRoundNumber);
    
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_rounds (tournament_id, round_number, round_name, status) "
        "VALUES ({}, {}, '{}', 'active')",
        tournamentId, nextRoundNumber, roundName
    );
    
    QueryResult roundResult = CharacterDatabase.Query("SELECT LAST_INSERT_ID()");
    if (!roundResult)
        return false;
        
    uint32 roundId = (*roundResult)[0].Get<uint32>();
    
    // Create matches for next round
    uint32 matchNumber = 1;
    for (size_t i = 0; i < winners.size(); i += 2)
    {
        if (i + 1 < winners.size())
        {
            CharacterDatabase.Execute(
                "INSERT INTO arena_tournament_matches (tournament_id, round_id, match_number, player1_guid, player2_guid, player1_name, player2_name, status) "
                "VALUES ({}, {}, {}, {}, {}, '{}', '{}', 'pending')",
                tournamentId, roundId, matchNumber, winners[i].first, winners[i + 1].first,
                winners[i].second, winners[i + 1].second
            );
        }
        else
        {
            // Bye
            CharacterDatabase.Execute(
                "INSERT INTO arena_tournament_matches (tournament_id, round_id, match_number, player1_guid, player2_guid, player1_name, player2_name, winner_guid, status) "
                "VALUES ({}, {}, {}, {}, 0, '{}', 'BYE', {}, 'completed')",
                tournamentId, roundId, matchNumber, winners[i].first, winners[i].second, winners[i].first
            );
        }
        matchNumber++;
    }
    
    LOG_INFO("tournament", "Started round {} for tournament {} with {} participants", nextRoundNumber, tournamentId, winners.size());
    
    return true;
}

bool TournamentSystem::FinishTournament(uint32 tournamentId, uint32 winnerGuid)
{
    // Get tournament and winner info
    QueryResult result = CharacterDatabase.Query(
        "SELECT t.name, t.winner_reward_gold, t.winner_reward_item, t.winner_title, t.current_participants, reg.character_name "
        "FROM arena_tournaments t "
        "JOIN arena_tournament_registrations reg ON t.id = reg.tournament_id AND reg.player_guid = {} "
        "WHERE t.id = {}",
        winnerGuid, tournamentId
    );
    
    if (!result)
        return false;
        
    auto fields = result->Fetch();
    std::string tournamentName = fields[0].Get<std::string>();
    uint32 goldReward = fields[1].Get<uint32>();
    uint32 itemReward = fields[2].Get<uint32>();
    uint32 titleReward = fields[3].Get<uint32>();
    uint32 totalParticipants = fields[4].Get<uint32>();
    std::string winnerName = fields[5].Get<std::string>();
    
    // Get runner-up and semi-finalists for additional rewards
    QueryResult finalsResult = CharacterDatabase.Query(
        "SELECT m.player1_guid, m.player2_guid, m.player1_name, m.player2_name, m.winner_guid "
        "FROM arena_tournament_matches m "
        "JOIN arena_tournament_rounds r ON m.round_id = r.id "
        "WHERE r.tournament_id = {} AND r.round_name = 'Finals' AND m.status = 'completed'",
        tournamentId
    );
    
    uint32 runnerUpGuid = 0;
    std::string runnerUpName = "";
    if (finalsResult)
    {
        auto finalsFields = finalsResult->Fetch();
        uint32 player1 = finalsFields[0].Get<uint32>();
        uint32 player2 = finalsFields[1].Get<uint32>();
        runnerUpGuid = (finalsFields[4].Get<uint32>() == player1) ? player2 : player1;
        runnerUpName = (finalsFields[4].Get<uint32>() == player1) ? finalsFields[3].Get<std::string>() : finalsFields[2].Get<std::string>();
        
        // Give runner-up reward (2nd place)
        uint32 runnerUpReward = sConfigMgr->GetOption<uint32>("Tournament.RunnerUpRewardGold", 100) * 10000;
        GiveRewards(runnerUpGuid, runnerUpReward);
        UpdatePlayerStats(runnerUpGuid, false, runnerUpReward);
    }
    
    // Get semi-finalists (3rd/4th place) if tournament had semi-finals
    QueryResult semiResult = CharacterDatabase.Query(
        "SELECT m.player1_guid, m.player2_guid, m.player1_name, m.player2_name, m.winner_guid "
        "FROM arena_tournament_matches m "
        "JOIN arena_tournament_rounds r ON m.round_id = r.id "
        "WHERE r.tournament_id = {} AND r.round_name = 'Semi Finals' AND m.status = 'completed'",
        tournamentId
    );
    
    if (semiResult)
    {
        uint32 semiReward = sConfigMgr->GetOption<uint32>("Tournament.SemiFinalistRewardGold", 25) * 10000;
        do
        {
            auto semiFields = semiResult->Fetch();
            uint32 player1 = semiFields[0].Get<uint32>();
            uint32 player2 = semiFields[1].Get<uint32>();
            uint32 winner = semiFields[4].Get<uint32>();
            uint32 loser = (winner == player1) ? player2 : player1;
            
            // Don't give semi-finalist reward to winner or runner-up
            if (loser != winnerGuid && loser != runnerUpGuid)
            {
                GiveRewards(loser, semiReward);
                UpdatePlayerStats(loser, false, semiReward);
            }
        } while (semiResult->NextRow());
    }
    
    // Update tournament status
    CharacterDatabase.Execute(
        "UPDATE arena_tournaments SET status = 'finished', tournament_end = NOW() WHERE id = {}",
        tournamentId
    );
    
    // Give rewards to winner
    GiveRewards(winnerGuid, goldReward, itemReward, titleReward);
    
    // Update player stats
    UpdatePlayerStats(winnerGuid, true, goldReward);
    
    // Record tournament history
    std::ostringstream rewardsJson;
    rewardsJson << "{\"winner_gold\":" << goldReward;
    if (itemReward > 0) rewardsJson << ",\"item\":" << itemReward;
    if (titleReward > 0) rewardsJson << ",\"title\":" << titleReward;
    if (runnerUpGuid > 0) rewardsJson << ",\"runner_up_gold\":" << (sConfigMgr->GetOption<uint32>("Tournament.RunnerUpRewardGold", 100) * 10000);
    rewardsJson << "}";
    
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_history (tournament_id, winner_guid, winner_name, total_participants, rewards_given) "
        "VALUES ({}, {}, '{}', {}, '{}')",
        tournamentId, winnerGuid, winnerName, totalParticipants, rewardsJson.str()
    );
    
    // Announce tournament completion with all winners
    std::ostringstream announce;
    announce << "|cffFFD700[Tournament]|r Tournament '|cff00FF00" << tournamentName 
             << "|r' has ended! |cffFFD700" << winnerName << "|r is the champion!";
    if (!runnerUpName.empty())
        announce << " Runner-up: |cffC0C0C0" << runnerUpName << "|r";
    announce << " Winner receives |cffFFD700" << (goldReward / 10000) << "|r gold!";
    
    if (sConfigMgr->GetOption<bool>("Tournament.EnableServerAnnouncements", true))
        SendTournamentAnnouncement(announce.str());
    
    LOG_INFO("tournament", "Tournament '{}' (ID: {}) finished. Winner: {} ({}), Runner-up: {} ({})", 
             tournamentName, tournamentId, winnerName, winnerGuid, runnerUpName, runnerUpGuid);
    
    return true;
}

// Additional helper functions...

std::vector<TournamentInfo> TournamentSystem::GetActiveTournaments()
{
    std::vector<TournamentInfo> tournaments;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, description, entry_fee, UNIX_TIMESTAMP(registration_start), UNIX_TIMESTAMP(registration_end), "
        "UNIX_TIMESTAMP(tournament_start), UNIX_TIMESTAMP(tournament_end), status, max_participants, current_participants, "
        "winner_reward_gold, winner_reward_item, winner_title, created_by, UNIX_TIMESTAMP(created_at) "
        "FROM arena_tournaments WHERE status IN ('registration', 'ready', 'active') ORDER BY created_at DESC"
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
        info.registrationStart = fields[4].Get<uint32>();
        info.registrationEnd = fields[5].Get<uint32>();
        info.tournamentStart = fields[6].Get<uint32>();
        info.tournamentEnd = fields[7].Get<uint32>();
        
        std::string statusStr = fields[8].Get<std::string>();
        if (statusStr == "registration") info.status = TOURNAMENT_STATUS_REGISTRATION;
        else if (statusStr == "ready") info.status = TOURNAMENT_STATUS_READY;
        else if (statusStr == "active") info.status = TOURNAMENT_STATUS_ACTIVE;
        else if (statusStr == "finished") info.status = TOURNAMENT_STATUS_FINISHED;
        else info.status = TOURNAMENT_STATUS_CANCELLED;
        
        info.maxParticipants = fields[9].Get<uint32>();
        info.currentParticipants = fields[10].Get<uint32>();
        info.winnerRewardGold = fields[11].Get<uint32>();
        info.winnerRewardItem = fields[12].Get<uint32>();
        info.winnerTitle = fields[13].Get<uint32>();
        info.createdBy = fields[14].Get<uint32>();
        info.createdAt = fields[15].Get<uint32>();
        
        tournaments.push_back(info);
    } while (result->NextRow());
    
    return tournaments;
}

bool TournamentSystem::CanPlayerRegister(uint32 tournamentId, Player* player, std::string& errorMsg)
{
    if (!player)
    {
        errorMsg = "Invalid player";
        return false;
    }
    
    // Check if tournament exists and is in registration phase
    QueryResult result = CharacterDatabase.Query(
        "SELECT status, UNIX_TIMESTAMP(registration_end), current_participants, max_participants, entry_fee "
        "FROM arena_tournaments WHERE id = {}",
        tournamentId
    );
    
    if (!result)
    {
        errorMsg = "Tournament not found";
        return false;
    }
    
    auto fields = result->Fetch();
    std::string status = fields[0].Get<std::string>();
    time_t registrationEnd = fields[1].Get<time_t>();
    uint32 currentParticipants = fields[2].Get<uint32>();
    uint32 maxParticipants = fields[3].Get<uint32>();
    uint32 entryFee = fields[4].Get<uint32>();
    
    if (status != "registration")
    {
        errorMsg = "Tournament registration is closed";
        return false;
    }
    
    if (time(nullptr) > registrationEnd)
    {
        errorMsg = "Registration period has ended";
        return false;
    }
    
    if (currentParticipants >= maxParticipants)
    {
        errorMsg = "Tournament is full";
        return false;
    }
    
    if (!HasEnoughGold(player, entryFee))
    {
        errorMsg = "Insufficient gold for entry fee";
        return false;
    }
    
    // Check if player is already in an active tournament
    if (IsPlayerInActiveTournament(player->GetGUID().GetCounter()))
    {
        errorMsg = "You are already participating in an active tournament";
        return false;
    }
    
    return true;
}

bool TournamentSystem::IsPlayerInActiveTournament(uint32 playerGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT t.id FROM arena_tournaments t "
        "JOIN arena_tournament_registrations reg ON t.id = reg.tournament_id "
        "WHERE reg.player_guid = {} AND t.status IN ('active') AND reg.status = 'confirmed'",
        playerGuid
    );
    
    return result != nullptr;
}

bool TournamentSystem::HasEnoughGold(Player* player, uint32 amount)
{
    return player->GetMoney() >= amount;
}

void TournamentSystem::GiveRewards(uint32 playerGuid, uint32 goldAmount, uint32 itemId, uint32 titleId)
{
    if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(playerGuid)))
    {
        if (goldAmount > 0)
        {
            player->ModifyMoney(goldAmount);
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffFFD700You have received {} gold as tournament reward!|r", goldAmount / 10000);
        }
        
        if (itemId > 0)
        {
            ItemPosCountVec dest;
            if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1) == EQUIP_ERR_OK)
            {
                Item* item = player->StoreNewItem(dest, itemId, true);
                if (item)
                    player->SendNewItem(item, 1, true, false);
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cff00FF00You have received a special tournament item!|r");
            }
            else
            {
                // Inventory full, send via mail
                MailDraft draft("Tournament Reward", "Congratulations! Here is your tournament reward item.");
                draft.AddItem(itemId, 1);
                draft.SendMailTo(player, MAIL_SENDER_ARENA, MAIL_STATIONERY_GM);
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cff00FF00Tournament item sent via mail (inventory was full).|r");
            }
        }
        
        if (titleId > 0)
        {
            if (CharTitlesEntry const* title = sCharTitlesStore.LookupEntry(titleId))
            {
                player->SetTitle(title);
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffFF6600You have been granted a tournament title!|r");
            }
        }
    }
    else
    {
        // Player is offline, add gold directly to database
        if (goldAmount > 0)
        {
            CharacterDatabase.Execute("UPDATE characters SET money = money + {} WHERE guid = {}", goldAmount, playerGuid);
            LOG_INFO("tournament", "Added {} gold to offline player {}", goldAmount / 10000, playerGuid);
        }
        // Items and titles will be given when player logs in (would need additional tracking)
    }
}

void TournamentSystem::SendTournamentAnnouncement(const std::string& message)
{
    sWorld->SendServerMessage(SERVER_MSG_STRING, message.c_str());
}

std::string TournamentSystem::GenerateRoundName(uint32 participantCount, uint32 roundNumber)
{
    if (participantCount <= 2)
        return "Finals";
    else if (participantCount <= 4)
        return roundNumber == 1 ? "Semi Finals" : "Finals";
    else if (participantCount <= 8)
    {
        switch (roundNumber)
        {
            case 1: return "Quarter Finals";
            case 2: return "Semi Finals";
            case 3: return "Finals";
            default: return "Round " + std::to_string(roundNumber);
        }
    }
    else
        return "Round " + std::to_string(roundNumber);
}

uint32 TournamentSystem::CalculateRoundsNeeded(uint32 participantCount)
{
    uint32 rounds = 0;
    uint32 remaining = participantCount;
    
    while (remaining > 1)
    {
        remaining = (remaining + 1) / 2; // Ceiling division
        rounds++;
    }
    
    return rounds;
}

void TournamentSystem::ShuffleParticipants(std::vector<uint32>& participants)
{
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(participants.begin(), participants.end(), g);
}

bool TournamentSystem::UpdatePlayerStats(uint32 playerGuid, bool won, uint32 goldEarned)
{
    // Insert or update player stats
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_player_stats "
        "(player_guid, tournaments_participated, tournaments_won, total_matches_played, total_matches_won, total_gold_earned, last_tournament_date) "
        "VALUES ({}, 1, {}, 1, {}, {}, NOW()) "
        "ON DUPLICATE KEY UPDATE "
        "tournaments_participated = tournaments_participated + 1, "
        "tournaments_won = tournaments_won + {}, "
        "total_matches_played = total_matches_played + 1, "
        "total_matches_won = total_matches_won + {}, "
        "total_gold_earned = total_gold_earned + {}, "
        "last_tournament_date = NOW()",
        playerGuid, won ? 1 : 0, won ? 1 : 0, goldEarned,
        won ? 1 : 0, won ? 1 : 0, goldEarned
    );
    
    return true;
}

void TournamentSystem::Update(uint32 diff)
{
    // Update timer
    _updateTimer += diff;
    
    // Run checks every 30 seconds
    if (_updateTimer >= 30000)
    {
        _updateTimer = 0;
        
        // Check for forfeits
        CheckForForfeits();
        
        // Check for tournaments that should transition from registration to ready
        // Add any other periodic checks here
    }
}

bool TournamentSystem::CheckForForfeits()
{
    // Get forfeit attempts from config
    uint32 maxAttempts = sConfigMgr->GetOption<uint32>("Tournament.ForfeitAfterAttempts", 3);
    
    // Check for players who haven't joined their matches after configured attempts
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, tournament_id, player1_guid, player2_guid, join_attempts_player1, join_attempts_player2 "
        "FROM arena_tournament_matches "
        "WHERE status = 'pending' AND (join_attempts_player1 >= {} OR join_attempts_player2 >= {})",
        maxAttempts, maxAttempts
    );
    
    if (!result)
        return false;
        
    do
    {
        auto fields = result->Fetch();
        uint32 matchId = fields[0].Get<uint32>();
        uint32 tournamentId = fields[1].Get<uint32>();
        uint32 player1Guid = fields[2].Get<uint32>();
        uint32 player2Guid = fields[3].Get<uint32>();
        uint32 attempts1 = fields[4].Get<uint32>();
        uint32 attempts2 = fields[5].Get<uint32>();
        
        uint32 winnerGuid = 0;
        
        if (attempts1 >= maxAttempts && attempts2 < maxAttempts)
            winnerGuid = player2Guid; // Player 1 forfeits
        else if (attempts2 >= maxAttempts && attempts1 < maxAttempts)
            winnerGuid = player1Guid; // Player 2 forfeits
        else if (attempts1 >= maxAttempts && attempts2 >= maxAttempts)
        {
            // Both forfeit - random winner or tournament rules decide
            winnerGuid = (rand() % 2) ? player1Guid : player2Guid;
        }
        
        if (winnerGuid > 0)
        {
            CharacterDatabase.Execute(
                "UPDATE arena_tournament_matches SET winner_guid = {}, status = 'forfeit', match_end = NOW() WHERE id = {}",
                winnerGuid, matchId
            );
            
            LOG_INFO("tournament", "Match {} decided by forfeit, winner: {}", matchId, winnerGuid);
            
            // Process the forfeit as a normal match result
            ProcessMatchResult(matchId, winnerGuid);
        }
        
    } while (result->NextRow());
    
    return true;
}

bool TournamentSystem::CancelTournament(uint32 tournamentId)
{
    // Check if tournament exists
    QueryResult result = CharacterDatabase.Query(
        "SELECT name, status FROM arena_tournaments WHERE id = {}",
        tournamentId
    );
    
    if (!result)
        return false;
        
    auto fields = result->Fetch();
    std::string name = fields[0].Get<std::string>();
    std::string status = fields[1].Get<std::string>();
    
    // Can only cancel registration or ready tournaments
    if (status != "registration" && status != "ready")
    {
        LOG_ERROR("tournament", "Cannot cancel tournament {} - invalid status: {}", tournamentId, status);
        return false;
    }
    
    // Refund entry fees to all registered players
    QueryResult refundResult = CharacterDatabase.Query(
        "SELECT player_guid, entry_fee_paid FROM arena_tournament_registrations WHERE tournament_id = {}",
        tournamentId
    );
    
    if (refundResult)
    {
        do
        {
            auto refundFields = refundResult->Fetch();
            uint32 playerGuid = refundFields[0].Get<uint32>();
            bool feePaid = refundFields[1].Get<bool>();
            
            if (feePaid)
            {
                // Refund logic - mark for refund (actual refund happens on login)
                CharacterDatabase.Execute(
                    "UPDATE arena_tournament_registrations SET entry_fee_paid = 0 WHERE tournament_id = {} AND player_guid = {}",
                    tournamentId, playerGuid
                );
            }
        } while (refundResult->NextRow());
    }
    
    // Update tournament status
    CharacterDatabase.Execute(
        "UPDATE arena_tournaments SET status = 'cancelled' WHERE id = {}",
        tournamentId
    );
    
    // Announce cancellation
    std::ostringstream announce;
    announce << "|cffFFD700[Tournament]|r Tournament '|cffC0C0C0" << name 
             << "|r' has been cancelled. All entry fees will be refunded.";
    SendTournamentAnnouncement(announce.str());
    
    LOG_INFO("tournament", "Tournament '{}' (ID: {}) cancelled", name, tournamentId);
    LogTournamentEvent(tournamentId, "Tournament cancelled");
    
    return true;
}

bool TournamentSystem::UnregisterPlayer(uint32 tournamentId, uint32 playerGuid)
{
    // Check if player is registered
    if (!IsPlayerRegistered(tournamentId, playerGuid))
        return false;
    
    // Remove registration
    CharacterDatabase.Execute(
        "DELETE FROM arena_tournament_registrations WHERE tournament_id = {} AND player_guid = {}",
        tournamentId, playerGuid
    );
    
    // Update participant count
    CharacterDatabase.Execute(
        "UPDATE arena_tournaments SET current_participants = GREATEST(current_participants - 1, 0) WHERE id = {}",
        tournamentId
    );
    
    LOG_INFO("tournament", "Player {} unregistered from tournament {}", playerGuid, tournamentId);
    
    return true;
}

bool TournamentSystem::PayEntryFee(uint32 tournamentId, Player* player)
{
    if (!player)
        return false;
        
    uint32 playerGuid = player->GetGUID().GetCounter();
    
    // Check if player is registered but hasn't paid
    QueryResult result = CharacterDatabase.Query(
        "SELECT entry_fee_paid FROM arena_tournament_registrations WHERE tournament_id = {} AND player_guid = {}",
        tournamentId, playerGuid
    );
    
    if (!result)
        return false;
        
    bool alreadyPaid = (*result)[0].Get<bool>();
    if (alreadyPaid)
        return true; // Already paid
    
    // Get tournament entry fee
    QueryResult feeResult = CharacterDatabase.Query(
        "SELECT entry_fee FROM arena_tournaments WHERE id = {}",
        tournamentId
    );
    
    if (!feeResult)
        return false;
        
    uint32 entryFee = (*feeResult)[0].Get<uint32>();
    
    // Check if player has enough gold
    if (!HasEnoughGold(player, entryFee))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("You need {} gold to pay the entry fee!", entryFee / 10000);
        return false;
    }
    
    // Deduct entry fee
    player->ModifyMoney(-int32(entryFee));
    
    // Mark as paid
    CharacterDatabase.Execute(
        "UPDATE arena_tournament_registrations SET entry_fee_paid = 1 WHERE tournament_id = {} AND player_guid = {}",
        tournamentId, playerGuid
    );
    
    ChatHandler(player->GetSession()).PSendSysMessage("|cff00FF00Entry fee of {} gold has been paid.|r", entryFee / 10000);
    LOG_INFO("tournament", "Player {} paid entry fee for tournament {}", player->GetName(), tournamentId);
    
    return true;
}

std::vector<TournamentInfo> TournamentSystem::GetAllTournaments()
{
    std::vector<TournamentInfo> tournaments;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, description, entry_fee, UNIX_TIMESTAMP(registration_start), UNIX_TIMESTAMP(registration_end), "
        "UNIX_TIMESTAMP(tournament_start), UNIX_TIMESTAMP(tournament_end), status, max_participants, current_participants, "
        "winner_reward_gold, winner_reward_item, winner_title, created_by, UNIX_TIMESTAMP(created_at) "
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
        info.registrationStart = fields[4].Get<uint32>();
        info.registrationEnd = fields[5].Get<uint32>();
        info.tournamentStart = fields[6].Get<uint32>();
        info.tournamentEnd = fields[7].Get<uint32>();
        
        std::string statusStr = fields[8].Get<std::string>();
        if (statusStr == "registration") info.status = TOURNAMENT_STATUS_REGISTRATION;
        else if (statusStr == "ready") info.status = TOURNAMENT_STATUS_READY;
        else if (statusStr == "active") info.status = TOURNAMENT_STATUS_ACTIVE;
        else if (statusStr == "finished") info.status = TOURNAMENT_STATUS_FINISHED;
        else if (statusStr == "cancelled") info.status = TOURNAMENT_STATUS_CANCELLED;
        else info.status = TOURNAMENT_STATUS_FINISHED;
        
        info.maxParticipants = fields[9].Get<uint32>();
        info.currentParticipants = fields[10].Get<uint32>();
        info.winnerRewardGold = fields[11].Get<uint32>();
        info.winnerRewardItem = fields[12].Get<uint32>();
        info.winnerTitle = fields[13].Get<uint32>();
        info.createdBy = fields[14].Get<uint32>();
        info.createdAt = fields[15].Get<uint32>();
        
        tournaments.push_back(info);
    } while (result->NextRow());
    
    return tournaments;
}

TournamentInfo TournamentSystem::GetTournamentInfo(uint32 tournamentId)
{
    TournamentInfo info;
    info.id = 0; // Default to invalid
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, description, entry_fee, UNIX_TIMESTAMP(registration_start), UNIX_TIMESTAMP(registration_end), "
        "UNIX_TIMESTAMP(tournament_start), UNIX_TIMESTAMP(tournament_end), status, max_participants, current_participants, "
        "winner_reward_gold, winner_reward_item, winner_title, created_by, UNIX_TIMESTAMP(created_at) "
        "FROM arena_tournaments WHERE id = {}",
        tournamentId
    );
    
    if (!result)
        return info;
        
    auto fields = result->Fetch();
    info.id = fields[0].Get<uint32>();
    info.name = fields[1].Get<std::string>();
    info.description = fields[2].Get<std::string>();
    info.entryFee = fields[3].Get<uint32>();
    info.registrationStart = fields[4].Get<uint32>();
    info.registrationEnd = fields[5].Get<uint32>();
    info.tournamentStart = fields[6].Get<uint32>();
    info.tournamentEnd = fields[7].Get<uint32>();
    
    std::string statusStr = fields[8].Get<std::string>();
    if (statusStr == "registration") info.status = TOURNAMENT_STATUS_REGISTRATION;
    else if (statusStr == "ready") info.status = TOURNAMENT_STATUS_READY;
    else if (statusStr == "active") info.status = TOURNAMENT_STATUS_ACTIVE;
    else if (statusStr == "finished") info.status = TOURNAMENT_STATUS_FINISHED;
    else if (statusStr == "cancelled") info.status = TOURNAMENT_STATUS_CANCELLED;
    else info.status = TOURNAMENT_STATUS_FINISHED;
    
    info.maxParticipants = fields[9].Get<uint32>();
    info.currentParticipants = fields[10].Get<uint32>();
    info.winnerRewardGold = fields[11].Get<uint32>();
    info.winnerRewardItem = fields[12].Get<uint32>();
    info.winnerTitle = fields[13].Get<uint32>();
    info.createdBy = fields[14].Get<uint32>();
    info.createdAt = fields[15].Get<uint32>();
    
    return info;
}

std::vector<TournamentRegistration> TournamentSystem::GetTournamentRegistrations(uint32 tournamentId)
{
    std::vector<TournamentRegistration> registrations;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, player_guid, character_name, UNIX_TIMESTAMP(registration_time), entry_fee_paid, status "
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
        reg.playerGuid = fields[1].Get<uint32>();
        reg.characterName = fields[2].Get<std::string>();
        reg.registrationTime = fields[3].Get<uint32>();
        reg.entryFeePaid = fields[4].Get<bool>();
        reg.confirmed = (fields[5].Get<std::string>() == "confirmed");
        
        registrations.push_back(reg);
    } while (result->NextRow());
    
    return registrations;
}

std::vector<TournamentRound> TournamentSystem::GetTournamentBracket(uint32 tournamentId)
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
        else round.status = ROUND_STATUS_PENDING;
        
        round.startTime = fields[4].Get<uint32>();
        round.endTime = fields[5].Get<uint32>();
        
        // Get matches for this round
        QueryResult matchResult = CharacterDatabase.Query(
            "SELECT id, match_number, player1_guid, player2_guid, player1_name, player2_name, "
            "winner_guid, status, UNIX_TIMESTAMP(match_start), UNIX_TIMESTAMP(match_end), "
            "join_attempts_player1, join_attempts_player2, battleground_id "
            "FROM arena_tournament_matches WHERE round_id = {} ORDER BY match_number",
            round.id
        );
        
        if (matchResult)
        {
            do
            {
                auto matchFields = matchResult->Fetch();
                TournamentMatch match;
                match.id = matchFields[0].Get<uint32>();
                match.tournamentId = tournamentId;
                match.roundId = round.id;
                match.matchNumber = matchFields[1].Get<uint32>();
                match.player1Guid = matchFields[2].Get<uint32>();
                match.player2Guid = matchFields[3].Get<uint32>();
                match.player1Name = matchFields[4].Get<std::string>();
                match.player2Name = matchFields[5].Get<std::string>();
                match.winnerGuid = matchFields[6].Get<uint32>();
                
                std::string matchStatusStr = matchFields[7].Get<std::string>();
                if (matchStatusStr == "pending") match.status = MATCH_STATUS_PENDING;
                else if (matchStatusStr == "active") match.status = MATCH_STATUS_ACTIVE;
                else if (matchStatusStr == "completed") match.status = MATCH_STATUS_COMPLETED;
                else if (matchStatusStr == "forfeit") match.status = MATCH_STATUS_FORFEIT;
                else match.status = MATCH_STATUS_PENDING;
                
                match.matchStart = matchFields[8].Get<uint32>();
                match.matchEnd = matchFields[9].Get<uint32>();
                match.joinAttemptsPlayer1 = matchFields[10].Get<uint32>();
                match.joinAttemptsPlayer2 = matchFields[11].Get<uint32>();
                match.battlegroundId = matchFields[12].Get<uint32>();
                
                round.matches.push_back(match);
            } while (matchResult->NextRow());
        }
        
        rounds.push_back(round);
    } while (result->NextRow());
    
    return rounds;
}

TournamentMatch TournamentSystem::GetPlayerCurrentMatch(uint32 playerGuid)
{
    TournamentMatch match;
    match.id = 0; // Default to invalid
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT m.id, m.tournament_id, m.round_id, m.match_number, m.player1_guid, m.player2_guid, "
        "m.player1_name, m.player2_name, m.winner_guid, m.status, "
        "UNIX_TIMESTAMP(m.match_start), UNIX_TIMESTAMP(m.match_end), "
        "m.join_attempts_player1, m.join_attempts_player2, m.battleground_id "
        "FROM arena_tournament_matches m "
        "JOIN arena_tournament_rounds r ON m.round_id = r.id "
        "JOIN arena_tournaments t ON r.tournament_id = t.id "
        "WHERE (m.player1_guid = {} OR m.player2_guid = {}) "
        "AND m.status IN ('pending', 'active') AND t.status = 'active' "
        "ORDER BY r.round_number DESC, m.match_number LIMIT 1",
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
    else match.status = MATCH_STATUS_PENDING;
    
    match.matchStart = fields[10].Get<uint32>();
    match.matchEnd = fields[11].Get<uint32>();
    match.joinAttemptsPlayer1 = fields[12].Get<uint32>();
    match.joinAttemptsPlayer2 = fields[13].Get<uint32>();
    match.battlegroundId = fields[14].Get<uint32>();
    
    return match;
}

std::map<std::string, uint32> TournamentSystem::GetPlayerTournamentStats(uint32 playerGuid)
{
    std::map<std::string, uint32> stats;
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT tournaments_participated, tournaments_won, total_matches_played, total_matches_won, total_gold_earned "
        "FROM arena_tournament_player_stats WHERE player_guid = {}",
        playerGuid
    );
    
    if (!result)
    {
        // Return zeros if no stats found
        stats["tournaments_participated"] = 0;
        stats["tournaments_won"] = 0;
        stats["total_matches_played"] = 0;
        stats["total_matches_won"] = 0;
        stats["total_gold_earned"] = 0;
        return stats;
    }
    
    auto fields = result->Fetch();
    stats["tournaments_participated"] = fields[0].Get<uint32>();
    stats["tournaments_won"] = fields[1].Get<uint32>();
    stats["total_matches_played"] = fields[2].Get<uint32>();
    stats["total_matches_won"] = fields[3].Get<uint32>();
    stats["total_gold_earned"] = fields[4].Get<uint32>();
    
    return stats;
}

bool TournamentSystem::IsPlayerRegistered(uint32 tournamentId, uint32 playerGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT id FROM arena_tournament_registrations WHERE tournament_id = {} AND player_guid = {}",
        tournamentId, playerGuid
    );
    
    return result != nullptr;
}

std::string TournamentSystem::GetTournamentStatusString(TournamentStatus status)
{
    switch (status)
    {
        case TOURNAMENT_STATUS_REGISTRATION: return "Registration";
        case TOURNAMENT_STATUS_READY: return "Ready";
        case TOURNAMENT_STATUS_ACTIVE: return "Active";
        case TOURNAMENT_STATUS_FINISHED: return "Finished";
        case TOURNAMENT_STATUS_CANCELLED: return "Cancelled";
        default: return "Unknown";
    }
}

std::string TournamentSystem::GetMatchStatusString(TournamentMatchStatus status)
{
    switch (status)
    {
        case MATCH_STATUS_PENDING: return "Pending";
        case MATCH_STATUS_ACTIVE: return "Active";
        case MATCH_STATUS_COMPLETED: return "Completed";
        case MATCH_STATUS_FORFEIT: return "Forfeit";
        default: return "Unknown";
    }
}

bool TournamentSystem::ForceStartMatch(uint32 matchId)
{
    // Get match info
    QueryResult result = CharacterDatabase.Query(
        "SELECT tournament_id, player1_guid, player2_guid, status FROM arena_tournament_matches WHERE id = {}",
        matchId
    );
    
    if (!result)
        return false;
        
    auto fields = result->Fetch();
    uint32 tournamentId = fields[0].Get<uint32>();
    uint32 player1Guid = fields[1].Get<uint32>();
    uint32 player2Guid = fields[2].Get<uint32>();
    std::string status = fields[3].Get<std::string>();
    
    if (status != "pending")
    {
        LOG_ERROR("tournament", "Cannot force start match {} - invalid status: {}", matchId, status);
        return false;
    }
    
    // Create battleground for the match
    uint32 bgTypeId = sConfigMgr->GetOption<uint32>("Tournament.BattlegroundTypeId", 3); // Arena
    
    Battleground* bg = sBattlegroundMgr->CreateNewBattleground(bgTypeId, 2, 0, false);
    if (!bg)
    {
        LOG_ERROR("tournament", "Failed to create battleground for match {}", matchId);
        return false;
    }
    
    // Update match with battleground ID
    CharacterDatabase.Execute(
        "UPDATE arena_tournament_matches SET battleground_id = {}, status = 'active', match_start = NOW() WHERE id = {}",
        bg->GetID(), matchId
    );
    
    // Invite players to battleground
    if (Player* player1 = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(player1Guid)))
    {
        player1->GetSession()->QueuePacket(bg->GetBattlegroundEntryPoint());
    }
    
    if (Player* player2 = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(player2Guid)))
    {
        player2->GetSession()->QueuePacket(bg->GetBattlegroundEntryPoint());
    }
    
    LOG_INFO("tournament", "Match {} forced to start with battleground ID {}", matchId, bg->GetID());
    LogTournamentEvent(tournamentId, "Match " + std::to_string(matchId) + " forced to start");
    
    return true;
}

bool TournamentSystem::DisqualifyPlayer(uint32 tournamentId, uint32 playerGuid)
{
    // Check if player is registered
    if (!IsPlayerRegistered(tournamentId, playerGuid))
        return false;
    
    // Unregister player
    UnregisterPlayer(tournamentId, playerGuid);
    
    // Mark any active matches as forfeit
    QueryResult matchResult = CharacterDatabase.Query(
        "SELECT id, player1_guid, player2_guid FROM arena_tournament_matches "
        "WHERE tournament_id = {} AND (player1_guid = {} OR player2_guid = {}) AND status IN ('pending', 'active')",
        tournamentId, playerGuid, playerGuid
    );
    
    if (matchResult)
    {
        do
        {
            auto fields = matchResult->Fetch();
            uint32 matchId = fields[0].Get<uint32>();
            uint32 p1 = fields[1].Get<uint32>();
            uint32 p2 = fields[2].Get<uint32>();
            
            uint32 winnerGuid = (p1 == playerGuid) ? p2 : p1;
            
            CharacterDatabase.Execute(
                "UPDATE arena_tournament_matches SET winner_guid = {}, status = 'forfeit', match_end = NOW() WHERE id = {}",
                winnerGuid, matchId
            );
            
            // Process match result
            ProcessMatchResult(matchId, winnerGuid);
            
            LOG_INFO("tournament", "Player {} disqualified from match {}, opponent {} wins by forfeit", playerGuid, matchId, winnerGuid);
        } while (matchResult->NextRow());
    }
    
    LOG_INFO("tournament", "Player {} disqualified from tournament {}", playerGuid, tournamentId);
    LogTournamentEvent(tournamentId, "Player " + std::to_string(playerGuid) + " disqualified");
    
    return true;
}

void TournamentSystem::OnPlayerLogout(Player* player)
{
    if (!player)
        return;
        
    uint32 playerGuid = player->GetGUID().GetCounter();
    
    // Check if player has an active match
    TournamentMatch match = GetPlayerCurrentMatch(playerGuid);
    if (match.id > 0 && match.status == MATCH_STATUS_ACTIVE)
    {
        // Increment join attempts
        bool isPlayer1 = (match.player1Guid == playerGuid);
        CharacterDatabase.Execute(
            "UPDATE arena_tournament_matches SET {} = {} + 1 WHERE id = {}",
            isPlayer1 ? "join_attempts_player1" : "join_attempts_player2",
            isPlayer1 ? match.joinAttemptsPlayer1 : match.joinAttemptsPlayer2,
            match.id
        );
        
        LOG_INFO("tournament", "Player {} logged out during active match {}, attempt count incremented", playerGuid, match.id);
    }
}

void TournamentSystem::OnBattlegroundEnd(uint32 battlegroundId, uint32 winnerGuid)
{
    // Find match associated with this battleground
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, tournament_id FROM arena_tournament_matches WHERE battleground_id = {} AND status = 'active'",
        battlegroundId
    );
    
    if (!result)
        return;
        
    auto fields = result->Fetch();
    uint32 matchId = fields[0].Get<uint32>();
    uint32 tournamentId = fields[1].Get<uint32>();
    
    // Process match result
    ProcessMatchResult(matchId, winnerGuid);
    
    LOG_INFO("tournament", "Battleground {} ended, match {} processed with winner {}", battlegroundId, matchId, winnerGuid);
}

void TournamentSystem::LogTournamentEvent(uint32 tournamentId, const std::string& event)
{
    std::string safeEvent = event;
    CharacterDatabase.EscapeString(safeEvent);
    
    CharacterDatabase.Execute(
        "INSERT INTO arena_tournament_logs (tournament_id, event, event_time) VALUES ({}, '{}', NOW())",
        tournamentId, safeEvent
    );
    
    LOG_INFO("tournament", "Tournament {}: {}", tournamentId, event);
}

// WorldScript for Tournament System
class TournamentSystemWorldScript : public WorldScript
{
public:
    TournamentSystemWorldScript() : WorldScript("TournamentSystemWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_UPDATE
    }) {}

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        // Initialize tournament system
        LOG_INFO("tournament", "Tournament System initialized");
    }

    void OnUpdate(uint32 diff) override
    {
        sTournamentSystem->Update(diff);
    }
};

// Register scripts
void AddSC_TournamentSystem()
{
    new TournamentSystemWorldScript();
}
