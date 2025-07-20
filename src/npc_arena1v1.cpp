/*
 *   Copyright (C) 2016+     AzerothCore <www.azerothcore.org>, released under GNU AGPL3 v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 *   Copyright (C) 2013      Emu-Devstore <http://emu-devstore.com/>
 *
 *   Written by Teiby <http://www.teiby.de/>
 *   Adjusted by fr4z3n for azerothcore
 *   Reworked by XDev
 */

#include "ScriptMgr.h"
#include "ArenaTeamMgr.h"
#include "DisableMgr.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "BattlegroundQueue.h"
#include "ArenaTeam.h"
#include "Language.h"
#include "Config.h"
#include "Log.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "SharedDefines.h"
#include "Chat.h"
#include "npc_1v1arena.h"
#include "TournamentSystem.h"
#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "World.h"
#include <iomanip>
#include <sstream>

#define NPC_TEXT_ENTRY_1v1 999992

//Const for 1v1 arena
constexpr uint32 ARENA_TEAM_1V1 = 1;
constexpr uint32 ARENA_TYPE_1V1 = 1;
constexpr uint32 BATTLEGROUND_QUEUE_1V1 = 11;
constexpr BattlegroundQueueTypeId bgQueueTypeId = (BattlegroundQueueTypeId)((int)BATTLEGROUND_QUEUE_5v5 + 1);
uint32 ARENA_SLOT_1V1 = 3;

//Config
std::vector<uint32> forbiddenTalents;

enum npcActions {
    NPC_ARENA_1V1_ACTION_CREATE_ARENA_TEAM = 1,
    NPC_ARENA_1V1_ACTION_JOIN_QUEUE_ARENA_RATED = 2,
    NPC_ARENA_1V1_ACTION_LEAVE_QUEUE = 3,
    NPC_ARENA_1V1_ACTION_GET_STATISTICS = 4,
    NPC_ARENA_1V1_ACTION_DISBAND_ARENA_TEAM = 5,
    NPC_ARENA_1V1_ACTION_JOIN_QUEUE_ARENA_UNRATED = 20,
    NPC_ARENA_1V1_MAIN_MENU = 21,
    NPC_ARENA_1V1_ACTION_HELP = 22,
    NPC_ARENA_1V1_ACTION_VIEW_LEADERBOARD = 23,
    NPC_ARENA_1V1_ACTION_QUEUE_STATUS = 24,
    
    // Tournament Actions
    NPC_ARENA_1V1_ACTION_TOURNAMENTS = 100,
    NPC_ARENA_1V1_ACTION_TOURNAMENT_REGISTER = 101,
    NPC_ARENA_1V1_ACTION_TOURNAMENT_VIEW = 102,
    NPC_ARENA_1V1_ACTION_TOURNAMENT_BRACKET = 103,
    NPC_ARENA_1V1_ACTION_MY_TOURNAMENTS = 104,
    NPC_ARENA_1V1_ACTION_TOURNAMENT_STATS = 105,
    
    // Admin Tournament Actions (200+ range)
    NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN = 200,
    NPC_ARENA_1V1_ACTION_ADMIN_CREATE_TOURNAMENT = 201,
    NPC_ARENA_1V1_ACTION_ADMIN_START_TOURNAMENT = 202,
    NPC_ARENA_1V1_ACTION_ADMIN_CANCEL_TOURNAMENT = 203,
    NPC_ARENA_1V1_ACTION_ADMIN_VIEW_TOURNAMENTS = 204,
    
    // Enhanced Admin Creation (210+ range)
    NPC_ARENA_1V1_ACTION_ADMIN_CREATE_BASIC = 210,
    NPC_ARENA_1V1_ACTION_ADMIN_CREATE_ADVANCED = 211,
    NPC_ARENA_1V1_ACTION_ADMIN_SET_REWARDS = 212,
    NPC_ARENA_1V1_ACTION_ADMIN_SET_SCHEDULE = 213,
    NPC_ARENA_1V1_ACTION_ADMIN_CONFIRM_CREATE = 214,
    NPC_ARENA_1V1_ACTION_ADMIN_REWARD_SETTINGS = 215,
    NPC_ARENA_1V1_ACTION_ADMIN_GENERAL_SETTINGS = 216,
    
    // Tournament specific actions (300+ range for dynamic tournament IDs)
    NPC_ARENA_1V1_ACTION_TOURNAMENT_DETAILS_BASE = 1000,
    NPC_ARENA_1V1_ACTION_TOURNAMENT_REGISTER_BASE = 2000,
    NPC_ARENA_1V1_ACTION_TOURNAMENT_BRACKET_BASE = 3000,
};


bool teamExistForPlayerGuid(Player* player)
{
    QueryResult queryPlayerTeam = CharacterDatabase.Query("SELECT * FROM `arena_team` WHERE `captainGuid`={} AND `type`=1", player->GetGUID().GetCounter());
    if (queryPlayerTeam)
        return true;

    return false;
}

void deleteTeamArenaForPlayer(Player* player)
{
    QueryResult queryPlayerTeam = CharacterDatabase.Query("SELECT `arenaTeamId` FROM `arena_team` WHERE `captainGuid`={} AND `type`=1", player->GetGUID().GetCounter());
    if (queryPlayerTeam)
    {
        CharacterDatabase.Execute("DELETE FROM `arena_team` WHERE `captainGuid`={} AND `type`=1", player->GetGUID().GetCounter());
        CharacterDatabase.Execute("DELETE FROM `arena_team_member` WHERE `guid`={} AND `type`=1", player->GetGUID().GetCounter());
    }
}

class configloader_1v1arena : public WorldScript
{
public:
    configloader_1v1arena() : WorldScript("configloader_1v1arena", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    }) {}

    virtual void OnAfterConfigLoad(bool /*Reload*/) override
    {
        std::stringstream ss(sConfigMgr->GetOption<std::string>("Arena1v1.ForbiddenTalentsIDs", "0"));

        for (std::string blockedTalentsStr; std::getline(ss, blockedTalentsStr, ',');)
            forbiddenTalents.push_back(stoi(blockedTalentsStr));

        ARENA_SLOT_1V1 = sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3);

        ArenaTeam::ArenaSlotByType.emplace(ARENA_TEAM_1V1, ARENA_SLOT_1V1);
        ArenaTeam::ArenaReqPlayersForType.emplace(ARENA_TYPE_1V1, 2);

        BattlegroundMgr::queueToBg.insert({ BATTLEGROUND_QUEUE_1V1,   BATTLEGROUND_AA });
        BattlegroundMgr::QueueToArenaType.emplace(BATTLEGROUND_QUEUE_1V1, (ArenaType) ARENA_TYPE_1V1);
        BattlegroundMgr::ArenaTypeToQueue.emplace(ARENA_TYPE_1V1, (BattlegroundQueueTypeId) BATTLEGROUND_QUEUE_1V1);
    }

};

class playerscript_1v1arena : public PlayerScript
{
public:
    playerscript_1v1arena() : PlayerScript("playerscript_1v1arena", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_GET_MAX_PERSONAL_ARENA_RATING_REQUIREMENT,
        PLAYERHOOK_ON_GET_ARENA_TEAM_ID,
        PLAYERHOOK_ON_CHAT
    }) { }

    void OnPlayerLogin(Player* pPlayer) override
    {
        if (sConfigMgr->GetOption<bool>("Arena1v1.Announcer", true))
        {
            ChatHandler(pPlayer->GetSession()).SendSysMessage("|cFFFFD700==================================================|r");
            ChatHandler(pPlayer->GetSession()).SendSysMessage("|cFF4CFF00Welcome to the 1v1 Arena System!|r");
            ChatHandler(pPlayer->GetSession()).SendSysMessage("|cFFFFFFFFTest your skills in solo combat.|r");
            ChatHandler(pPlayer->GetSession()).SendSysMessage("|cFFFFFFFFFind the Arena Master NPC to get started!|r");
            ChatHandler(pPlayer->GetSession()).SendSysMessage("|cFFFFD700==================================================|r");
        }
    }

    void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg) override
    {
        // Handle tournament creation from chat
        if (player->GetSession()->HasInGuildInvite() && player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && type == CHAT_MSG_SAY)
        {
            // Player is in tournament creation mode
            std::string tournamentName = msg;
            
            if (tournamentName.length() < 3 || tournamentName.length() > 50)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cFFFF0000Tournament name must be between 3 and 50 characters!|r");
                return;
            }
            
            // Create tournament with default settings
            uint32 entryFee = sConfigMgr->GetOption<uint32>("Tournament.DefaultEntryFee", 100) * 10000;
            uint32 registrationHours = sConfigMgr->GetOption<uint32>("Tournament.DefaultRegistrationHours", 24);
            uint32 maxParticipants = sConfigMgr->GetOption<uint32>("Tournament.DefaultMaxParticipants", 16);
            uint32 winnerGold = sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardGold", 500) * 10000;
            uint32 winnerItem = sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardItem", 0);
            uint32 winnerTitle = sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerTitle", 0);
            
            std::string description = "Tournament created by " + player->GetName();
            
            uint32 tournamentId = sTournamentSystem->CreateTournament(
                tournamentName, description, entryFee, registrationHours, maxParticipants,
                player->GetGUID().GetCounter(), winnerGold, winnerItem, winnerTitle
            );
            
            if (tournamentId > 0)
            {
                std::ostringstream success;
                success << "|cFF00FF00Tournament '|cFFFFD700" << tournamentName << "|cFF00FF00' created successfully!|r";
                success << "\n|cFFFFD700Tournament ID:|r |cFFFFFFFF" << tournamentId << "|r";
                success << "\n|cFFFFD700Entry Fee:|r |cFFFFD700" << (entryFee / 10000) << " gold|r";
                success << "\n|cFFFFD700Registration:|r |cFFFFFFFF" << registrationHours << " hours|r";
                success << "\n|cFFFFD700Max Players:|r |cFFFFFFFF" << maxParticipants << "|r";
                success << "\n|cFFFFD700Winner Prize:|r |cFFFFD700" << (winnerGold / 10000) << " gold|r";
                
                ChatHandler(player->GetSession()).PSendSysMessage("%s", success.str().c_str());
                
                // Announce to server
                std::ostringstream announce;
                announce << "|cFFFFD700[Tournament]|r New tournament '|cFF00FF00" << tournamentName 
                         << "|r' created by GM! Entry fee: |cFFFFD700" << (entryFee / 10000) 
                         << "|r gold. Registration open for |cFF00FFFF" << registrationHours << "|r hours!";
                sWorld->SendServerMessage(SERVER_MSG_STRING, announce.str().c_str());
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cFFFF0000Failed to create tournament! Check server logs for details.|r");
            }
            
            // Clear the creation mode flag
            player->GetSession()->SetInGuildInvite(false);
            
            // Suppress the original chat message since we handled it
            msg = "";
        }
    }

    void OnPlayerGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 minslot, uint32& maxArenaRating) const override
    {
        if (sConfigMgr->GetOption<bool>("Arena1v1.VendorRating", false) && minslot < (uint32)sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3))
            if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TEAM_1V1))
                maxArenaRating = std::max(at->GetRating(), maxArenaRating);
    }

    void OnPlayerGetArenaTeamId(Player* player, uint8 slot, uint32& result) override
    {
        if (!player)
            return;

        if (slot == ARENA_SLOT_1V1)
            result = player->GetArenaTeamIdFromDB(player->GetGUID(), ARENA_TYPE_1V1);
    }
};


bool npc_1v1arena::OnGossipHello(Player* player, Creature* creature)
{
    if (!player || !creature)
        return true;

    if (sConfigMgr->GetOption<bool>("Arena1v1.Enable", true) == false)
    {
        ChatHandler(player->GetSession()).SendSysMessage("1v1 disabled!");
        return true;
    }

    // Tournament queue restriction: only registered players can queue during active tournaments
    bool inActiveTournament = sTournamentSystem->IsPlayerInActiveTournament(player->GetGUID().GetCounter());
    std::vector<TournamentInfo> activeTournaments = sTournamentSystem->GetActiveTournaments();
    bool hasActiveTournament = false;
    
    for (const auto& tournament : activeTournaments)
    {
        if (tournament.status == TOURNAMENT_STATUS_ACTIVE)
        {
            hasActiveTournament = true;
            break;
        }
    }

    if (player->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId))
        AddGossipItemFor(player, GOSSIP_ICON_DOT, "|cFFFF4500|TInterface/ICONS/Spell_ChargeNegative:30:30:-20:0|t|r |cFFFF4500Leave Arena Queue|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_LEAVE_QUEUE, "Are you sure you want to leave the queue?", 0, false);
    else
    {
        // Check tournament restrictions
        if (hasActiveTournament && !inActiveTournament)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFFFF0000|TInterface/ICONS/Achievement_Arena_2v2_4:30:30:-20:0|t|r |cFFFF0000Arena Locked (Tournament Active)|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_MAIN_MENU);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "|cFF00BFFF|TInterface\\icons\\Achievement_Arena_2v2_4:30:30:-20:0|t|r |cFF00BFFFJoin Arena (Unrated)|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_JOIN_QUEUE_ARENA_UNRATED);
        }
    }

    if (!teamExistForPlayerGuid(player))
    {
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, "|cFF32CD32|TInterface/ICONS/Achievement_Arena_2v2_7:30:30:-20:0|t|r |cFF32CD32Create Arena Team|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_CREATE_ARENA_TEAM, "Are you sure you want to create a new 1v1 Arena Team?\n\nCost: " + std::to_string(sConfigMgr->GetOption<uint32>("Arena1v1.Costs", 400000) / 10000) + " gold", sConfigMgr->GetOption<uint32>("Arena1v1.Costs", 400000), false);
    }
    else
    {
        if (!player->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId))
        {
            // Check tournament restrictions for rated queue too
            if (hasActiveTournament && !inActiveTournament)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFFFF0000|TInterface\\icons\\Achievement_Arena_2v2_1:30:30:-20:0|t|r |cFFFF0000Rated Arena Locked (Tournament)|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_MAIN_MENU);
            }
            else
            {
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "|cFFFFD700|TInterface\\icons\\Achievement_Arena_2v2_1:30:30:-20:0|t|r |cFFFFD700Join Arena (Rated)|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_JOIN_QUEUE_ARENA_RATED);
            }
            AddGossipItemFor(player, GOSSIP_ICON_DOT, "|cFFFF6347|TInterface/ICONS/Spell_Shadow_SacrificialShield:30:30:-20:0|t|r |cFFFF6347Disband Team|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_DISBAND_ARENA_TEAM, "Are you sure you want to disband your arena team?\n\nThis action cannot be undone!", 0, false);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF9370DB|TInterface/ICONS/INV_Misc_Coin_01:30:30:-20:0|t|r |cFF9370DBMy Statistics|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_GET_STATISTICS);
    }

    // Tournament section
    AddGossipItemFor(player, GOSSIP_ICON_TAXI, "|cFFFF1493|TInterface/ICONS/Achievement_Arena_2v2_8:30:30:-20:0|t|r |cFFFF1493Tournaments|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENTS);

    // Always show queue status and leaderboard
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF20B2AA|TInterface/ICONS/Achievement_BG_returnXflags_def_WSG:30:30:-20:0|t|r |cFF20B2AAQueue Status|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_QUEUE_STATUS);
    AddGossipItemFor(player, GOSSIP_ICON_TABARD, "|cFFFFA500|TInterface/ICONS/Achievement_Arena_2v2_2:30:30:-20:0|t|r |cFFFFA500Leaderboard|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_VIEW_LEADERBOARD);
    AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "|cFFDC143C|TInterface/ICONS/inv_misc_questionmark:30:30:-20:0|t|r |cFFDC143CHelp & Info|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_HELP);

    // Admin section for GMs
    if (IsPlayerAdmin(player))
    {
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|cFFFFD700[ADMIN]|r |cFFFF0000Tournament Management|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN);
    }

    SendGossipMenuFor(player, 68, creature);
    return true;
}

bool npc_1v1arena::OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
{
    if (!player || !creature)
        return true;

    ClearGossipMenuFor(player);

    ChatHandler handler(player->GetSession());

    switch (action)
    {
        case NPC_ARENA_1V1_ACTION_CREATE_ARENA_TEAM:
        {
            if (sConfigMgr->GetOption<uint32>("Arena1v1.MinLevel", 80) <= player->GetLevel())
            {
                if (player->GetMoney() >= uint32(sConfigMgr->GetOption<uint32>("Arena1v1.Costs", 400000)) && CreateArenateam(player, creature))
                    player->ModifyMoney(sConfigMgr->GetOption<uint32>("Arena1v1.Costs", 400000) * -1);
            }
            else
            {
                handler.PSendSysMessage("|cFFFF0000You need to be level %u+ to create a 1v1 arena team.|r", sConfigMgr->GetOption<uint32>("Arena1v1.MinLevel", 80));
                return true;
            }
            CloseGossipMenuFor(player);
        }
        break;

        case NPC_ARENA_1V1_ACTION_JOIN_QUEUE_ARENA_RATED:
        {
            if (Arena1v1CheckTalents(player) && !JoinQueueArena(player, creature, true))
                handler.SendSysMessage("|cFFFF0000Something went wrong when joining the rated queue.|r");
            else if (Arena1v1CheckTalents(player))
                handler.SendSysMessage("|cFF00FF00Successfully joined the rated arena queue!|r");

            CloseGossipMenuFor(player);
            return true;
        }


        case NPC_ARENA_1V1_ACTION_JOIN_QUEUE_ARENA_UNRATED:
        {
            if (Arena1v1CheckTalents(player) && !JoinQueueArena(player, creature, false))
                handler.SendSysMessage("|cFFFF0000Something went wrong when joining the unrated queue.|r");
            else if (Arena1v1CheckTalents(player))
                handler.SendSysMessage("|cFF00FF00Successfully joined the unrated arena queue!|r");

            CloseGossipMenuFor(player);
            return true;
        }

        case NPC_ARENA_1V1_ACTION_LEAVE_QUEUE:
        {
            uint8 arenaType = ARENA_TYPE_1V1;

            if (!player->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId))
                return true;

            WorldPacket data;
            data << arenaType << (uint8)0x0 << (uint32)BATTLEGROUND_AA << (uint16)0x0 << (uint8)0x0;
            player->GetSession()->HandleBattleFieldPortOpcode(data);
            CloseGossipMenuFor(player);
            return true;
        }


        case NPC_ARENA_1V1_ACTION_GET_STATISTICS:
        {
            ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ARENA_SLOT_1V1));
            if (at)
            {
                std::stringstream s;
                ArenaTeamStats stats = at->GetStats();
                float winRate = stats.SeasonGames > 0 ? (float)stats.SeasonWins / stats.SeasonGames * 100.0f : 0.0f;
                
                s << "\n|cFFFFD700========== Your 1v1 Arena Statistics ==========|r";
                s << "\n|cFFFFD700Current Rating: |cFFFFFFFF" << stats.Rating << "|r";
                s << "\n|cFFFFD700Server Rank: |cFFFFFFFF#" << stats.Rank << "|r";
                s << "\n|cFFFFD700Season Games: |cFFFFFFFF" << stats.SeasonGames << "|r";
                s << "\n|cFFFFD700Season Wins: |cFF00FF00" << stats.SeasonWins << "|r";
                s << "\n|cFFFFD700Season Losses: |cFFFF0000" << (stats.SeasonGames - stats.SeasonWins) << "|r";
                s << "\n|cFFFFD700Win Rate: |cFF90EE90" << std::fixed << std::setprecision(1) << winRate << "%|r";
                s << "\n|cFFFFD700Week Games: |cFFFFFFFF" << stats.WeekGames << "|r";
                s << "\n|cFFFFD700Week Wins: |cFF00FF00" << stats.WeekWins << "|r";
                s << "\n|cFFFFD700============================================|r";

                ChatHandler(player->GetSession()).PSendSysMessage("%s", s.str().c_str());
            }
            else
            {
                handler.SendSysMessage("|cFFFF0000No arena team found!|r");
            }
            CloseGossipMenuFor(player);
        }
        break;

        case NPC_ARENA_1V1_ACTION_DISBAND_ARENA_TEAM:
        {
            uint32 playerHonorPoints = player->GetHonorPoints();
            uint32 playerArenaPoints = player->GetArenaPoints();

            WorldPacket Data;
            Data << player->GetArenaTeamId(ARENA_SLOT_1V1);
            player->GetSession()->HandleArenaTeamLeaveOpcode(Data);
            handler.SendSysMessage("|cFF00FF00Arena team successfully disbanded!|r");
            CloseGossipMenuFor(player);

            // hackfix: restore points
            player->SetHonorPoints(playerHonorPoints);
            player->SetArenaPoints(playerArenaPoints);

            return true;
        }

        case NPC_ARENA_1V1_ACTION_HELP:
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Main Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_MAIN_MENU);
            SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
        }
        break;

        case NPC_ARENA_1V1_ACTION_VIEW_LEADERBOARD:
        {
            ShowLeaderboard(player, creature);
            CloseGossipMenuFor(player);
        }
        break;

        case NPC_ARENA_1V1_ACTION_QUEUE_STATUS:
        {
            ShowQueueStatus(player, creature);
            CloseGossipMenuFor(player);
        }
        break;

        case NPC_ARENA_1V1_MAIN_MENU:
            OnGossipHello(player, creature);
            break;

        // Tournament Actions
        case NPC_ARENA_1V1_ACTION_TOURNAMENTS:
        {
            ShowTournamentMenu(player, creature);
        }
        break;

        case NPC_ARENA_1V1_ACTION_MY_TOURNAMENTS:
        {
            ShowMyTournaments(player, creature);
        }
        break;

        case NPC_ARENA_1V1_ACTION_TOURNAMENT_STATS:
        {
            ShowTournamentStats(player, creature);
        }
        break;

        // Admin Tournament Actions
        case NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN:
        {
            if (IsPlayerAdmin(player))
                ShowAdminTournamentMenu(player, creature);
            else
                OnGossipHello(player, creature);
        }
        break;

        // Enhanced Admin Tournament Creation
        case NPC_ARENA_1V1_ACTION_ADMIN_CREATE_BASIC:
        {
            if (IsPlayerAdmin(player))
                ShowBasicTournamentCreation(player, creature);
            else
                OnGossipHello(player, creature);
        }
        break;

        case NPC_ARENA_1V1_ACTION_ADMIN_CREATE_ADVANCED:
        {
            if (IsPlayerAdmin(player))
                ShowAdvancedTournamentCreation(player, creature);
            else
                OnGossipHello(player, creature);
        }
        break;

        case NPC_ARENA_1V1_ACTION_ADMIN_START_TOURNAMENT:
        {
            if (IsPlayerAdmin(player))
                ShowStartTournamentMenu(player, creature);
            else
                OnGossipHello(player, creature);
        }
        break;

        case NPC_ARENA_1V1_ACTION_ADMIN_CANCEL_TOURNAMENT:
        {
            if (IsPlayerAdmin(player))
                ShowCancelTournamentMenu(player, creature);
            else
                OnGossipHello(player, creature);
        }
        break;

        case NPC_ARENA_1V1_ACTION_ADMIN_REWARD_SETTINGS:
        {
            if (IsPlayerAdmin(player))
                ShowRewardSettingsMenu(player, creature);
            else
                OnGossipHello(player, creature);
        }
        break;

        case NPC_ARENA_1V1_ACTION_ADMIN_GENERAL_SETTINGS:
        {
            if (IsPlayerAdmin(player))
                ShowGeneralSettingsMenu(player, creature);
            else
                OnGossipHello(player, creature);
        }
        break;

        case NPC_ARENA_1V1_ACTION_ADMIN_VIEW_TOURNAMENTS:
        {
            if (IsPlayerAdmin(player))
                ShowAllTournamentsAdmin(player, creature);
            else
                OnGossipHello(player, creature);
        }
        break;

        default:
        {
            // Handle dynamic tournament actions
            if (action >= NPC_ARENA_1V1_ACTION_TOURNAMENT_DETAILS_BASE && action < NPC_ARENA_1V1_ACTION_TOURNAMENT_DETAILS_BASE + 1000)
            {
                uint32 tournamentId = action - NPC_ARENA_1V1_ACTION_TOURNAMENT_DETAILS_BASE;
                ShowTournamentDetails(player, creature, tournamentId);
            }
            else if (action >= NPC_ARENA_1V1_ACTION_TOURNAMENT_REGISTER_BASE && action < NPC_ARENA_1V1_ACTION_TOURNAMENT_REGISTER_BASE + 1000)
            {
                uint32 tournamentId = action - NPC_ARENA_1V1_ACTION_TOURNAMENT_REGISTER_BASE;
                RegisterForTournament(player, tournamentId);
                ShowTournamentMenu(player, creature);
            }
            else if (action >= NPC_ARENA_1V1_ACTION_TOURNAMENT_BRACKET_BASE && action < NPC_ARENA_1V1_ACTION_TOURNAMENT_BRACKET_BASE + 1000)
            {
                uint32 tournamentId = action - NPC_ARENA_1V1_ACTION_TOURNAMENT_BRACKET_BASE;
                ShowTournamentBracket(player, creature, tournamentId);
            }
            // Admin tournament start actions (5000+ range)
            else if (action >= 5000 && action < 6000 && IsPlayerAdmin(player))
            {
                uint32 tournamentId = action - 5000;
                if (sTournamentSystem->StartTournament(tournamentId))
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("|cFF00FF00Tournament %u started successfully!|r", tournamentId);
                }
                else
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("|cFFFF0000Failed to start tournament %u!|r", tournamentId);
                }
                ShowStartTournamentMenu(player, creature);
            }
            // Admin tournament cancel actions (6000+ range)
            else if (action >= 6000 && action < 7000 && IsPlayerAdmin(player))
            {
                uint32 tournamentId = action - 6000;
                
                // Cancel tournament by updating status and refunding fees
                CharacterDatabase.Execute(
                    "UPDATE arena_tournaments SET status = 'cancelled' WHERE id = {}",
                    tournamentId
                );
                
                // Refund entry fees to all registered players
                QueryResult result = CharacterDatabase.Query(
                    "SELECT reg.player_guid, t.entry_fee "
                    "FROM arena_tournament_registrations reg "
                    "JOIN arena_tournaments t ON reg.tournament_id = t.id "
                    "WHERE reg.tournament_id = {} AND reg.status = 'confirmed'",
                    tournamentId
                );
                
                uint32 refundCount = 0;
                if (result)
                {
                    do
                    {
                        auto fields = result->Fetch();
                        uint32 playerGuid = fields[0].Get<uint32>();
                        uint32 entryFee = fields[1].Get<uint32>();
                        
                        if (Player* refundPlayer = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(playerGuid)))
                        {
                            refundPlayer->ModifyMoney(entryFee);
                            ChatHandler(refundPlayer->GetSession()).PSendSysMessage(
                                "|cFFFFD700Tournament cancelled! Your entry fee of %u gold has been refunded.|r", entryFee / 10000);
                        }
                        else
                        {
                            // Handle offline players - add to mail or character table
                            CharacterDatabase.Execute(
                                "UPDATE characters SET money = money + {} WHERE guid = {}",
                                entryFee, playerGuid
                            );
                        }
                        refundCount++;
                        
                    } while (result->NextRow());
                }
                
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cFF00FF00Tournament %u cancelled successfully! Refunded %u players.|r", 
                    tournamentId, refundCount
                );
                ShowCancelTournamentMenu(player, creature);
            }
            // View participants actions (7000+ range)
            else if (action >= 7000 && action < 8000)
            {
                uint32 tournamentId = action - 7000;
                ShowTournamentParticipants(player, creature, tournamentId);
            }
        }
        break;

    }

    return true;
}

bool npc_1v1arena::JoinQueueArena(Player* player, Creature* /* me */, bool isRated)
{
    if (!player)
        return false;

    if (sConfigMgr->GetOption<uint32>("Arena1v1.MinLevel", 80) > player->GetLevel())
        return false;

    uint8 arenatype = ARENA_TYPE_1V1;
    uint32 arenaRating = 0;
    uint32 matchmakerRating = 0;

    // ignore if we already in BG or BG queue
    if (player->InBattleground())
        return false;

    //check existance
    Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
    if (!bg)
    {
        LOG_ERROR("module", "Battleground: template bg (all arenas) not found");
        return false;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, BATTLEGROUND_AA, nullptr))
    {
        ChatHandler(player->GetSession()).PSendSysMessage(LANG_ARENA_DISABLED);
        return false;
    }

    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), player->GetLevel());
    if (!bracketEntry)
        return false;

    // check if already in queue
    if (player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return false; // //player is already in this queue

    // check if has free queue slots
    if (!player->HasFreeBattlegroundQueueId())
        return false;

    uint32 ateamId = 0;

    if (isRated)
    {
        ateamId = player->GetArenaTeamId(ARENA_SLOT_1V1);
        ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(ateamId);
        if (!at)
        {
            player->GetSession()->SendNotInArenaTeamPacket(arenatype);
            return false;
        }

        // get the team rating for queueing
        arenaRating = std::max(0u, at->GetRating());
        matchmakerRating = arenaRating;
        // the arenateam id must match for everyone in the group
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
    BattlegroundTypeId bgTypeId = BATTLEGROUND_AA;

    bg->SetRated(isRated);
    bg->SetMaxPlayersPerTeam(1);

    GroupQueueInfo* ginfo = bgQueue.AddGroup(player, nullptr, bgTypeId, bracketEntry, arenatype, isRated != 0, false, arenaRating, matchmakerRating, ateamId, 0);
    uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo);
    uint32 queueSlot = player->AddBattlegroundQueueId(bgQueueTypeId);

    // send status packet (in queue)
    WorldPacket data;
    sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, arenatype, TEAM_NEUTRAL, isRated);
    player->GetSession()->SendPacket(&data);

    sBattlegroundMgr->ScheduleQueueUpdate(matchmakerRating, arenatype, bgQueueTypeId, bgTypeId, bracketEntry->GetBracketId());

    return true;
}

bool npc_1v1arena::CreateArenateam(Player* player, Creature* /* me */)
{
    if (!player)
        return false;

    uint8 slot = ArenaTeam::GetSlotByType(ARENA_TEAM_1V1);
    //Just to make sure as some other module might edit this value
    if (slot == 0)
        return false;

    // Check if player is already in an arena team
    if (player->GetArenaTeamId(slot))
    {
        player->GetSession()->SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, player->GetName(), "You are already in an arena team!", ERR_ALREADY_IN_ARENA_TEAM);
        return false;
    }

    // This disaster is the result of changing the MAX_ARENA_SLOT from 3 to 4.
    uint32 playerHonorPoints = player->GetHonorPoints();
    uint32 playerArenaPoints = player->GetArenaPoints();
    player->SetHonorPoints(0);
    player->SetArenaPoints(0);

    // This disaster is the result of changing the MAX_ARENA_SLOT from 3 to 4.
    sArenaTeamMgr->RemoveArenaTeam(player->GetArenaTeamId(ARENA_SLOT_1V1));
    deleteTeamArenaForPlayer(player);

    // Create arena team
    ArenaTeam* arenaTeam = new ArenaTeam();
    if (!arenaTeam->Create(player->GetGUID(), ARENA_TEAM_1V1, player->GetName(), 4283124816, 45, 4294242303, 5, 4294705149))
    {
        delete arenaTeam;

        // hackfix: restore points
        player->SetHonorPoints(playerHonorPoints);
        player->SetArenaPoints(playerArenaPoints);

        return false;
    }

    // Register arena team
    sArenaTeamMgr->AddArenaTeam(arenaTeam);

    ChatHandler(player->GetSession()).SendSysMessage("|cFF00FF001v1 Arena team successfully created!|r");
    ChatHandler(player->GetSession()).SendSysMessage("|cFFFFD700You can now join rated arena matches!|r");

    // This disaster is the result of changing the MAX_ARENA_SLOT from 3 to 4.
    // hackfix: restore points
    player->SetHonorPoints(playerHonorPoints);
    player->SetArenaPoints(playerArenaPoints);

    return true;
}

bool npc_1v1arena::Arena1v1CheckTalents(Player* player)
{
    if (!player)
        return false;

    if (player->HasHealSpec() && (sConfigMgr->GetOption<bool>("Arena1v1.PreventHealingTalents", false)))
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000You can't join arena because you have healing talents.|r");
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFFD700Please respec to a DPS build to participate in 1v1 arena.|r");
        return false;
    }

    if (player->HasTankSpec() && (sConfigMgr->GetOption<bool>("Arena1v1.PreventTankTalents", false)))
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000You can't join arena because you have tank talents.|r");
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFFD700Please respec to a DPS build to participate in 1v1 arena.|r");
        return false;
    }

    return true;
}

void npc_1v1arena::ShowLeaderboard(Player* player, Creature* /* creature */)
{
    if (!player)
        return;

    ChatHandler handler(player->GetSession());
    
    QueryResult result = CharacterDatabase.Query(
        "SELECT at.name, at.rating, at.seasonGames, at.seasonWins, c.name as playerName, c.class "
        "FROM arena_team at "
        "JOIN arena_team_member atm ON at.arenaTeamId = atm.arenaTeamId "
        "JOIN characters c ON atm.guid = c.guid "
        "WHERE at.type = 1 AND at.seasonGames > 0 "
        "ORDER BY at.rating DESC LIMIT 10"
    );

    if (!result)
    {
        handler.SendSysMessage("|cFFFF0000No arena teams found in leaderboard.|r");
        return;
    }

    handler.SendSysMessage("|cFFFFD700========== 1v1 Arena Leaderboard ==========|r");
    handler.SendSysMessage("|cFFFFD700Rank | Player Name | Rating | Games | Wins | Class|r");
    handler.SendSysMessage("|cFFFFD700-------------------------------------------------|r");

    uint32 rank = 1;
    do
    {
        Field* fields = result->Fetch();
        std::string teamName = fields[0].Get<std::string>();
        uint32 rating = fields[1].Get<uint32>();
        uint32 seasonGames = fields[2].Get<uint32>();
        uint32 seasonWins = fields[3].Get<uint32>();
        std::string playerName = fields[4].Get<std::string>();
        uint8 playerClass = fields[5].Get<uint8>();

        std::string className = GetClassNameById(playerClass);
        float winRate = seasonGames > 0 ? (float)seasonWins / seasonGames * 100.0f : 0.0f;

        handler.PSendSysMessage("|cFF%s%u. |cFFFFFFFF%s |cFFFFD700[%u] |cFF87CEEB%u/%u |cFF90EE90%.1f%% |cFF%s%s|r",
            rank <= 3 ? "FFD700" : "C0C0C0",  // Gold for top 3, silver for others
            rank,
            playerName.c_str(),
            rating,
            seasonWins,
            seasonGames,
            winRate,
            GetClassColorById(playerClass).c_str(),
            className.c_str()
        );

        rank++;
    } while (result->NextRow());

    handler.SendSysMessage("|cFFFFD700===============================================|r");
}

void npc_1v1arena::ShowQueueStatus(Player* player, Creature* /* creature */)
{
    if (!player)
        return;

    ChatHandler handler(player->GetSession());
    
    // Get queue information
    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
    
    handler.SendSysMessage("|cFFFFD700========== Arena Queue Status ==========|r");
    
    if (player->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId))
    {
        handler.SendSysMessage("|cFF00FF00You are currently in the arena queue.|r");
        
        uint32 queueSlot = player->GetBattlegroundQueueIndex(bgQueueTypeId);
        if (queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES)
        {
            handler.SendSysMessage("|cFFFFFFFFQueue Position: In Queue|r");
            handler.SendSysMessage("|cFFFFFFFFEstimated Wait Time: Calculating...|r");
        }
    }
    else
    {
        handler.SendSysMessage("|cFFFF6347You are not currently in any queue.|r");
    }
    
    // Show server queue statistics
    handler.SendSysMessage("|cFFFFD700----------------------------------------|r");
    handler.SendSysMessage("|cFF87CEEBServer Queue Information:|r");
    handler.SendSysMessage("|cFFFFFFFFActive 1v1 Battles: Calculating...|r");
    handler.SendSysMessage("|cFFFFFFFFPlayers in Queue: Calculating...|r");
    handler.SendSysMessage("|cFFFFD700========================================|r");
}

std::string npc_1v1arena::GetClassNameById(uint8 classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR: return "Warrior";
        case CLASS_PALADIN: return "Paladin";
        case CLASS_HUNTER: return "Hunter";
        case CLASS_ROGUE: return "Rogue";
        case CLASS_PRIEST: return "Priest";
        case CLASS_DEATH_KNIGHT: return "Death Knight";
        case CLASS_SHAMAN: return "Shaman";
        case CLASS_MAGE: return "Mage";
        case CLASS_WARLOCK: return "Warlock";
        case CLASS_DRUID: return "Druid";
        default: return "Unknown";
    }
}

// Tournament System Functions
void npc_1v1arena::ShowTournamentMenu(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    std::vector<TournamentInfo> tournaments = sTournamentSystem->GetActiveTournaments();
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Main Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_MAIN_MENU);
    
    if (tournaments.empty())
    {
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFFFF0000No active tournaments|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_MAIN_MENU);
    }
    else
    {
        for (const auto& tournament : tournaments)
        {
            std::string statusColor;
            std::string statusText;
            
            switch (tournament.status)
            {
                case TOURNAMENT_STATUS_REGISTRATION:
                    statusColor = "00FF00";
                    statusText = "Registration Open";
                    break;
                case TOURNAMENT_STATUS_READY:
                    statusColor = "FFFF00";
                    statusText = "Ready to Start";
                    break;
                case TOURNAMENT_STATUS_ACTIVE:
                    statusColor = "FF4500";
                    statusText = "Active";
                    break;
                default:
                    statusColor = "CCCCCC";
                    statusText = "Unknown";
                    break;
            }
            
            std::ostringstream menuText;
            menuText << "|cFF" << statusColor << "[" << statusText << "]|r ";
            menuText << "|cFFFFD700" << tournament.name << "|r";
            menuText << " |cFFFFFFFF(" << tournament.currentParticipants << "/" << tournament.maxParticipants << ")|r";
            
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, menuText.str(), 
                GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_DETAILS_BASE + tournament.id);
        }
    }
    
    AddGossipItemFor(player, GOSSIP_ICON_TABARD, "|cFF9370DB|TInterface/ICONS/Achievement_Arena_2v2_3:30:30:-20:0|t|r |cFF9370DBMy Tournaments|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_MY_TOURNAMENTS);
    AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "|cFFFFA500|TInterface/ICONS/Achievement_Arena_2v2_6:30:30:-20:0|t|r |cFFFFA500Tournament Statistics|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_STATS);
    
    SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
}

void npc_1v1arena::ShowTournamentDetails(Player* player, Creature* creature, uint32 tournamentId)
{
    ClearGossipMenuFor(player);
    
    // Get detailed tournament information from database
    QueryResult result = CharacterDatabase.Query(
        "SELECT name, description, entry_fee, UNIX_TIMESTAMP(registration_start), UNIX_TIMESTAMP(registration_end), "
        "UNIX_TIMESTAMP(tournament_start), UNIX_TIMESTAMP(tournament_end), status, max_participants, current_participants, "
        "winner_reward_gold, winner_reward_item, winner_title, created_by "
        "FROM arena_tournaments WHERE id = {}",
        tournamentId
    );
    
    if (!result)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cFFFF0000Tournament not found!|r");
        ShowTournamentMenu(player, creature);
        return;
    }
    
    auto fields = result->Fetch();
    std::string name = fields[0].Get<std::string>();
    std::string description = fields[1].Get<std::string>();
    uint32 entryFee = fields[2].Get<uint32>();
    time_t regStart = fields[3].Get<time_t>();
    time_t regEnd = fields[4].Get<time_t>();
    time_t tStart = fields[5].Get<time_t>();
    time_t tEnd = fields[6].Get<time_t>();
    std::string status = fields[7].Get<std::string>();
    uint32 maxParticipants = fields[8].Get<uint32>();
    uint32 currentParticipants = fields[9].Get<uint32>();
    uint32 winnerGold = fields[10].Get<uint32>();
    uint32 winnerItem = fields[11].Get<uint32>();
    uint32 winnerTitle = fields[12].Get<uint32>();
    uint32 createdBy = fields[13].Get<uint32>();
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Tournament Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENTS);
    
    // Get creator name
    std::string creatorName = "Unknown";
    QueryResult creatorResult = CharacterDatabase.Query("SELECT name FROM characters WHERE guid = {}", createdBy);
    if (creatorResult)
        creatorName = (*creatorResult)[0].Get<std::string>();
    
    std::ostringstream details;
    details << "\n|cFFFFD700========== Tournament Details ==========|r";
    details << "\n|cFFFFD700Name:|r |cFFFFFFFF" << name << "|r";
    details << "\n|cFFFFD700Description:|r |cFFFFFFFF" << description << "|r";
    details << "\n|cFFFFD700Created by:|r |cFF9370DB" << creatorName << "|r";
    details << "\n|cFFFFD700Status:|r ";
    
    if (status == "registration") details << "|cFF00FF00Registration Open|r";
    else if (status == "ready") details << "|cFFFFD700Ready to Start|r";
    else if (status == "active") details << "|cFF00FFFF Active|r";
    else if (status == "finished") details << "|cFFC0C0C0Finished|r";
    else details << "|cFFFF0000" << status << "|r";
    
    details << "\n\n|cFFFFD700--- Participation Info ---|r";
    details << "\n|cFF00FF00Entry Fee:|r |cFFFFD700" << (entryFee / 10000) << " gold|r";
    details << "\n|cFF00FF00Participants:|r |cFFFFFFFF" << currentParticipants << "/" << maxParticipants << "|r";
    
    if (status == "registration")
    {
        time_t timeLeft = regEnd - time(nullptr);
        if (timeLeft > 0)
        {
            details << "\n|cFF00FF00Registration Ends:|r |cFFFFFFFF" << FormatTime(regEnd) << "|r";
            details << "\n|cFF00FF00Time Remaining:|r |cFFFFFFFF" << FormatDuration(timeLeft) << "|r";
        }
        else
        {
            details << "\n|cFFFF0000Registration Period Ended|r";
        }
    }
    else if (status == "active")
    {
        details << "\n|cFF00FF00Started At:|r |cFFFFFFFF" << FormatTime(tStart) << "|r";
        
        // Show current round info
        QueryResult roundResult = CharacterDatabase.Query(
            "SELECT round_number, round_name FROM arena_tournament_rounds "
            "WHERE tournament_id = {} AND status = 'active'", tournamentId
        );
        if (roundResult)
        {
            auto roundFields = roundResult->Fetch();
            details << "\n|cFF00FF00Current Round:|r |cFFFFFFFF" << roundFields[1].Get<std::string>() << "|r";
        }
    }
    else if (status == "finished" && tEnd > 0)
    {
        details << "\n|cFF00FF00Finished At:|r |cFFFFFFFF" << FormatTime(tEnd) << "|r";
        
        // Show winner
        QueryResult winnerResult = CharacterDatabase.Query(
            "SELECT winner_name FROM arena_tournament_history WHERE tournament_id = {}", tournamentId
        );
        if (winnerResult)
        {
            details << "\n|cFFFFD700Champion:|r |cFF00FF00" << (*winnerResult)[0].Get<std::string>() << "|r";
        }
    }
    
    details << "\n\n|cFFFFD700--- Rewards ---|r";
    details << "\n|cFFFFD700Winner:|r |cFFFFD700" << (winnerGold / 10000) << " gold|r";
    if (winnerItem > 0)
        details << " + |cFF00FF00Item " << winnerItem << "|r";
    if (winnerTitle > 0)
        details << " + |cFFFF6600Title " << winnerTitle << "|r";
    
    // Show runner-up and semi-finalist rewards
    uint32 runnerUpReward = sConfigMgr->GetOption<uint32>("Tournament.RunnerUpRewardGold", 100);
    uint32 semiReward = sConfigMgr->GetOption<uint32>("Tournament.SemiFinalistRewardGold", 25);
    
    if (runnerUpReward > 0)
        details << "\n|cFFC0C0C0Runner-up:|r |cFFFFD700" << (runnerUpReward / 100) << " gold|r";
    if (semiReward > 0 && maxParticipants >= 4)
        details << "\n|cFFCD853FSemi-finalists:|r |cFFFFD700" << (semiReward / 100) << " gold|r";
    
    details << "\n|cFFFFD700========================================|r";
    
    ChatHandler(player->GetSession()).PSendSysMessage("%s", details.str().c_str());
    
    // Show action buttons based on tournament status and player state
    bool isRegistered = false;
    QueryResult regCheck = CharacterDatabase.Query(
        "SELECT id FROM arena_tournament_registrations WHERE tournament_id = {} AND player_guid = {}",
        tournamentId, player->GetGUID().GetCounter()
    );
    isRegistered = regCheck != nullptr;
    
    if (status == "registration" && !isRegistered)
    {
        std::string errorMsg;
        if (sTournamentSystem->CanPlayerRegister(tournamentId, player, errorMsg))
        {
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, 
                "|cFF00FF00|TInterface/ICONS/INV_Misc_Coin_17:30:30:-20:0|t|r |cFF00FF00Register for Tournament|r", 
                GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_REGISTER_BASE + tournamentId,
                ("Are you sure you want to register for '" + name + "'?\n\nEntry fee: " + std::to_string(entryFee / 10000) + " gold").c_str(),
                entryFee, false);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, 
                ("|cFFFF0000Cannot Register: " + errorMsg + "|r").c_str(), 
                GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENTS);
        }
    }
    else if (isRegistered && status == "registration")
    {
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF00FF00✓ You are registered!|r", 
            GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENTS);
    }
    else if (isRegistered && (status == "ready" || status == "active"))
    {
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "|cFF00FFFF⚔ You are participating!|r", 
            GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENTS);
    }
    
    if (status == "active" || status == "finished")
    {
        AddGossipItemFor(player, GOSSIP_ICON_TABARD, "|cFFFFA500|TInterface/ICONS/Achievement_Arena_2v2_8:30:30:-20:0|t|r View Tournament Bracket|r", 
            GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_BRACKET_BASE + tournamentId);
    }
    
    // Show participant list option
    AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "|cFF9370DB|TInterface/ICONS/Achievement_BG_returnXflags_def_WSG:30:30:-20:0|t|r View Participants|r", 
        GOSSIP_SENDER_MAIN, 7000 + tournamentId);
    
    SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
}

void npc_1v1arena::ShowTournamentBracket(Player* player, Creature* creature, uint32 tournamentId)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Tournament Details|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_DETAILS_BASE + tournamentId);
    
    std::vector<TournamentRound> rounds = sTournamentSystem->GetTournamentBracket(tournamentId);
    
    if (rounds.empty())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cFFFF0000No bracket data available.|r");
        SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
        return;
    }
    
    std::ostringstream bracket;
    bracket << "\n|cFFFFD700========== Tournament Bracket ==========|r";
    
    for (const auto& round : rounds)
    {
        bracket << "\n\n|cFF00FFFF=== " << round.roundName << " ===|r";
        
        if (round.matches.empty())
        {
            bracket << "\n|cFFCCCCCCNo matches in this round|r";
            continue;
        }
        
        for (const auto& match : round.matches)
        {
            bracket << "\n|cFFFFFFFFMatch " << match.matchNumber << ": |r";
            
            if (match.player2Guid == 0)
            {
                // Bye
                bracket << "|cFFFFD700" << match.player1Name << "|r |cFF00FF00(BYE)|r";
            }
            else if (match.status == MATCH_STATUS_COMPLETED)
            {
                std::string winnerName = (match.winnerGuid == match.player1Guid) ? match.player1Name : match.player2Name;
                std::string loserName = (match.winnerGuid == match.player1Guid) ? match.player2Name : match.player1Name;
                bracket << "|cFF00FF00" << winnerName << "|r def. |cFFFF0000" << loserName << "|r";
            }
            else if (match.status == MATCH_STATUS_ACTIVE)
            {
                bracket << "|cFFFFD700" << match.player1Name << "|r vs |cFFFFD700" << match.player2Name << "|r |cFFFF4500(In Progress)|r";
            }
            else
            {
                bracket << "|cFFFFD700" << match.player1Name << "|r vs |cFFFFD700" << match.player2Name << "|r |cFFCCCCCC(Pending)|r";
            }
        }
    }
    
    bracket << "\n|cFFFFD700=======================================|r";
    
    ChatHandler(player->GetSession()).PSendSysMessage("%s", bracket.str().c_str());
    SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
}

void npc_1v1arena::ShowMyTournaments(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Tournament Menu|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENTS);
    
    // Get player's current match if in active tournament
    TournamentMatch currentMatch = sTournamentSystem->GetPlayerCurrentMatch(player->GetGUID().GetCounter());
    
    if (currentMatch.id > 0)
    {
        std::ostringstream matchInfo;
        matchInfo << "\n|cFFFF4500========== Current Tournament Match ==========|r";
        matchInfo << "\n|cFFFFD700Opponent: |cFFFFFFFF";
        
        if (currentMatch.player1Guid == player->GetGUID().GetCounter())
            matchInfo << currentMatch.player2Name;
        else
            matchInfo << currentMatch.player1Name;
            
        matchInfo << "|r";
        matchInfo << "\n|cFFFFD700Status: |cFF00FF00" << sTournamentSystem->GetMatchStatusString(currentMatch.status) << "|r";
        matchInfo << "\n|cFFFFD700Match Number: |cFFFFFFFF" << currentMatch.matchNumber << "|r";
        
        if (currentMatch.status == MATCH_STATUS_PENDING)
        {
            matchInfo << "\n|cFFFF0000Please join the arena queue when ready!|r";
        }
        
        matchInfo << "\n|cFFFF4500============================================|r";
        
        ChatHandler(player->GetSession()).PSendSysMessage("%s", matchInfo.str().c_str());
    }
    else
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cFFCCCCCCYou are not currently in any active tournament match.|r");
    }
    
    // Show tournament history/stats for this player
    std::map<std::string, uint32> stats = sTournamentSystem->GetPlayerTournamentStats(player->GetGUID().GetCounter());
    
    std::ostringstream statsInfo;
    statsInfo << "\n|cFF9370DB========== Your Tournament Stats ==========|r";
    statsInfo << "\n|cFFFFD700Tournaments Participated: |cFFFFFFFF" << stats["tournaments_participated"] << "|r";
    statsInfo << "\n|cFFFFD700Tournaments Won: |cFF00FF00" << stats["tournaments_won"] << "|r";
    statsInfo << "\n|cFFFFD700Total Matches Played: |cFFFFFFFF" << stats["total_matches_played"] << "|r";
    statsInfo << "\n|cFFFFD700Total Matches Won: |cFF00FF00" << stats["total_matches_won"] << "|r";
    statsInfo << "\n|cFFFFD700Total Gold Earned: |cFFFFD700" << (stats["total_gold_earned"] / 10000) << "|r gold";
    
    if (stats["total_matches_played"] > 0)
    {
        float winRate = (float)stats["total_matches_won"] / stats["total_matches_played"] * 100.0f;
        statsInfo << "\n|cFFFFD700Tournament Win Rate: |cFF90EE90" << std::fixed << std::setprecision(1) << winRate << "%|r";
    }
    
    statsInfo << "\n|cFF9370DB==========================================|r";
    
    ChatHandler(player->GetSession()).PSendSysMessage("%s", statsInfo.str().c_str());
    
    SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
}

void npc_1v1arena::ShowTournamentStats(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Tournament Menu|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENTS);
    
    // Show global tournament statistics
    QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*) as total_tournaments, "
        "SUM(total_participants) as total_participants, "
        "AVG(total_participants) as avg_participants "
        "FROM arena_tournament_history"
    );
    
    if (result)
    {
        auto fields = result->Fetch();
        uint32 totalTournaments = fields[0].Get<uint32>();
        uint32 totalParticipants = fields[1].Get<uint32>();
        float avgParticipants = fields[2].Get<float>();
        
        std::ostringstream globalStats;
        globalStats << "\n|cFFFFA500========== Global Tournament Stats ==========|r";
        globalStats << "\n|cFFFFD700Total Tournaments Completed: |cFFFFFFFF" << totalTournaments << "|r";
        globalStats << "\n|cFFFFD700Total Participants: |cFFFFFFFF" << totalParticipants << "|r";
        globalStats << "\n|cFFFFD700Average Participants: |cFFFFFFFF" << std::fixed << std::setprecision(1) << avgParticipants << "|r";
        globalStats << "\n|cFFFFA500=========================================|r";
        
        ChatHandler(player->GetSession()).PSendSysMessage("%s", globalStats.str().c_str());
    }
    
    // Show top tournament champions
    QueryResult champResult = CharacterDatabase.Query(
        "SELECT winner_name, COUNT(*) as wins FROM arena_tournament_history "
        "GROUP BY winner_guid ORDER BY wins DESC LIMIT 10"
    );
    
    if (champResult)
    {
        std::ostringstream champions;
        champions << "\n|cFFFFD700========== Tournament Champions ==========|r";
        
        uint32 rank = 1;
        do
        {
            auto fields = champResult->Fetch();
            std::string winnerName = fields[0].Get<std::string>();
            uint32 wins = fields[1].Get<uint32>();
            
            std::string rankColor;
            if (rank == 1) rankColor = "FFD700";      // Gold
            else if (rank == 2) rankColor = "C0C0C0"; // Silver
            else if (rank == 3) rankColor = "CD7F32"; // Bronze
            else rankColor = "FFFFFF";                 // White
            
            champions << "\n|cFF" << rankColor << "#" << rank << ". " << winnerName 
                     << " (" << wins << " wins)|r";
            rank++;
            
        } while (champResult->NextRow());
        
        champions << "\n|cFFFFD700=======================================|r";
        
        ChatHandler(player->GetSession()).PSendSysMessage("%s", champions.str().c_str());
    }
    
    SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
}

void npc_1v1arena::ShowAdminTournamentMenu(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Main Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_MAIN_MENU);
    
    // Enhanced Tournament Creation Options
    AddGossipItemFor(player, GOSSIP_ICON_VENDOR, "|cFF00FF00|TInterface/ICONS/INV_Misc_Trophy_06:30:30:-20:0|t|r |cFF00FF00Create Tournament (Quick)|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_ADMIN_CREATE_BASIC);
    
    AddGossipItemFor(player, GOSSIP_ICON_VENDOR, "|cFFFFD700|TInterface/ICONS/INV_Misc_Trophy_05:30:30:-20:0|t|r |cFFFFD700Create Tournament (Advanced)|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_ADMIN_CREATE_ADVANCED);
    
    // Tournament Management
    AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "|cFF32CD32|TInterface/ICONS/Achievement_Arena_2v2_5:30:30:-20:0|t|r |cFF32CD32Start Tournament|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_ADMIN_START_TOURNAMENT);
    
    AddGossipItemFor(player, GOSSIP_ICON_DOT, "|cFFFF0000|TInterface/ICONS/Spell_Shadow_SacrificialShield:30:30:-20:0|t|r |cFFFF0000Cancel Tournament|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_ADMIN_CANCEL_TOURNAMENT);
    
    // Configuration Options
    AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "|cFF9370DB|TInterface/ICONS/INV_Misc_Gem_01:30:30:-20:0|t|r |cFF9370DBReward Settings|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_ADMIN_REWARD_SETTINGS);
    
    AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "|cFF20B2AA|TInterface/ICONS/INV_Misc_Gear_01:30:30:-20:0|t|r |cFF20B2AAGeneral Settings|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_ADMIN_GENERAL_SETTINGS);
    
    // View Options
    AddGossipItemFor(player, GOSSIP_ICON_TABARD, "|cFF9370DB|TInterface/ICONS/Achievement_Arena_2v2_3:30:30:-20:0|t|r |cFF9370DBView All Tournaments|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_ADMIN_VIEW_TOURNAMENTS);
    
    SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
}

bool npc_1v1arena::RegisterForTournament(Player* player, uint32 tournamentId)
{
    return sTournamentSystem->RegisterPlayer(tournamentId, player);
}

bool npc_1v1arena::IsPlayerAdmin(Player* player)
{
    return player->GetSession()->GetSecurity() >= SEC_GAMEMASTER;
}

std::string npc_1v1arena::FormatTime(time_t timestamp)
{
    if (timestamp == 0)
        return "N/A";
        
    struct tm* timeinfo = localtime(&timestamp);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

std::string npc_1v1arena::FormatDuration(uint32 seconds)
{
    uint32 days = seconds / 86400;
    uint32 hours = (seconds % 86400) / 3600;
    uint32 minutes = (seconds % 3600) / 60;
    
    std::ostringstream duration;
    if (days > 0)
        duration << days << "d ";
    if (hours > 0)
        duration << hours << "h ";
    if (minutes > 0)
        duration << minutes << "m";
        
    return duration.str();
}

std::string npc_1v1arena::GetClassColorById(uint8 classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR: return "C79C6E";      // Brown
        case CLASS_PALADIN: return "F58CBA";      // Pink
        case CLASS_HUNTER: return "ABD473";       // Green
        case CLASS_ROGUE: return "FFF569";        // Yellow
        case CLASS_PRIEST: return "FFFFFF";       // White
        case CLASS_DEATH_KNIGHT: return "C41F3B"; // Red
        case CLASS_SHAMAN: return "0070DE";       // Blue
        case CLASS_MAGE: return "69CCF0";         // Light Blue
        case CLASS_WARLOCK: return "9482C9";      // Purple
        case CLASS_DRUID: return "FF7D0A";        // Orange
        default: return "FFFFFF";                 // White
    }
}

// Enhanced Admin Tournament Functions
void npc_1v1arena::ShowBasicTournamentCreation(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Admin Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN);
    
    ChatHandler handler(player->GetSession());
    
    // Show current configuration defaults
    std::ostringstream info;
    info << "\n|cFFFFD700========== Quick Tournament Creation ==========|r";
    info << "\n|cFFFFD700This will create a tournament with default settings:|r";
    info << "\n|cFF00FF00Entry Fee: |cFFFFFFFF" << (sConfigMgr->GetOption<uint32>("Tournament.DefaultEntryFee", 100) / 10000) << " gold|r";
    info << "\n|cFF00FF00Registration: |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.DefaultRegistrationHours", 24) << " hours|r";
    info << "\n|cFF00FF00Max Players: |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.DefaultMaxParticipants", 16) << "|r";
    info << "\n|cFF00FF00Winner Prize: |cFFFFFFFF" << (sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardGold", 500) / 100) << " gold|r";
    info << "\n|cFFFF6600Type tournament name in chat:|r |cFFFFFFFFExample: Arena Championship|r";
    
    handler.PSendSysMessage("%s", info.str().c_str());
    
    // Store creation type in player's session for later use
    player->GetSession()->SetInGuildInvite(true); // Using as flag for basic creation
    
    CloseGossipMenuFor(player);
}

void npc_1v1arena::ShowAdvancedTournamentCreation(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Admin Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN);
    
    ChatHandler handler(player->GetSession());
    
    std::ostringstream info;
    info << "\n|cFFFFD700========== Advanced Tournament Creation ==========|r";
    info << "\n|cFFFFD700Step-by-step tournament creation with full customization:|r";
    info << "\n|cFF00FF001. Tournament Name & Description|r";
    info << "\n|cFF00FF002. Entry Fee & Registration Time|r";  
    info << "\n|cFF00FF003. Participant Limits & Restrictions|r";
    info << "\n|cFF00FF004. Reward Configuration (Gold/Items/Titles)|r";
    info << "\n|cFF00FF005. Prize Distribution (Winner/Runner-up/Semi-finalists)|r";
    info << "\n|cFF00FF006. Review & Create|r";
    info << "\n|cFFFF6600This feature will guide you through each step.|r";
    info << "\n|cFFFF0000[Advanced creation system - Coming soon!]|r";
    
    handler.PSendSysMessage("%s", info.str().c_str());
    
    CloseGossipMenuFor(player);
}

void npc_1v1arena::ShowStartTournamentMenu(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Admin Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN);
    
    ChatHandler handler(player->GetSession());
    
    // Get tournaments that can be started
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, current_participants, max_participants, UNIX_TIMESTAMP(registration_end) "
        "FROM arena_tournaments WHERE status IN ('registration', 'ready') ORDER BY id DESC"
    );
    
    if (!result)
    {
        handler.PSendSysMessage("|cFFFF0000No tournaments available to start.|r");
        CloseGossipMenuFor(player);
        return;
    }
    
    std::ostringstream info;
    info << "\n|cFFFFD700========== Tournaments Ready to Start ==========|r";
    
    do
    {
        auto fields = result->Fetch();
        uint32 tournamentId = fields[0].Get<uint32>();
        std::string name = fields[1].Get<std::string>();
        uint32 current = fields[2].Get<uint32>();
        uint32 max = fields[3].Get<uint32>();
        time_t regEnd = fields[4].Get<time_t>();
        
        std::string status = (time(nullptr) > regEnd) ? "Ready" : "Registration";
        std::string statusColor = (time(nullptr) > regEnd) ? "00FF00" : "FFD700";
        
        info << "\n|cFF00FFFF[" << tournamentId << "]|r |cFFFFFFFF" << name << "|r";
        info << "\n   |cFF9370DBParticipants:|r |cFFFFFFFF" << current << "/" << max << "|r";
        info << "\n   |cFF9370DBStatus:|r |c" << statusColor << status << "|r";
        
        if (current >= 2)
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, ("|cFF00FF00Start: " + name + "|r").c_str(), 
                GOSSIP_SENDER_MAIN, 5000 + tournamentId);
        
        info << "\n";
        
    } while (result->NextRow());
    
    handler.PSendSysMessage("%s", info.str().c_str());
    SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
}

void npc_1v1arena::ShowCancelTournamentMenu(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Admin Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN);
    
    ChatHandler handler(player->GetSession());
    
    // Get active tournaments
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, status, current_participants "
        "FROM arena_tournaments WHERE status IN ('registration', 'ready', 'active') ORDER BY id DESC"
    );
    
    if (!result)
    {
        handler.PSendSysMessage("|cFFFF0000No active tournaments to cancel.|r");
        CloseGossipMenuFor(player);
        return;
    }
    
    std::ostringstream info;
    info << "\n|cFFFFD700========== Active Tournaments ==========|r";
    
    do
    {
        auto fields = result->Fetch();
        uint32 tournamentId = fields[0].Get<uint32>();
        std::string name = fields[1].Get<std::string>();
        std::string status = fields[2].Get<std::string>();
        uint32 participants = fields[3].Get<uint32>();
        
        info << "\n|cFF00FFFF[" << tournamentId << "]|r |cFFFFFFFF" << name << "|r";
        info << "\n   |cFF9370DBStatus:|r |cFFFFD700" << status << "|r";
        info << "\n   |cFF9370DBParticipants:|r |cFFFFFFFF" << participants << "|r";
        
        AddGossipItemFor(player, GOSSIP_ICON_DOT, ("|cFFFF0000Cancel: " + name + "|r").c_str(), 
            GOSSIP_SENDER_MAIN, 6000 + tournamentId, 
            ("Are you sure you want to cancel tournament '" + name + "'? This will refund all entry fees.").c_str(), 0, false);
        
        info << "\n";
        
    } while (result->NextRow());
    
    handler.PSendSysMessage("%s", info.str().c_str());
    SendGossipMenuFor(player, NPC_TEXT_ENTRY_1v1, creature->GetGUID());
}

void npc_1v1arena::ShowRewardSettingsMenu(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Admin Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN);
    
    ChatHandler handler(player->GetSession());
    
    std::ostringstream info;
    info << "\n|cFFFFD700========== Current Reward Settings ==========|r";
    info << "\n|cFF00FF00Default Winner Gold:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardGold", 500) << " copper|r";
    info << "\n|cFF00FF00Default Winner Item:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardItem", 0) << "|r";
    info << "\n|cFF00FF00Default Winner Title:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerTitle", 0) << "|r";
    info << "\n|cFFC0C0C0Runner-up Gold:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.RunnerUpRewardGold", 100) << " copper|r";
    info << "\n|cFFCD853FSemi-finalist Gold:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.SemiFinalistRewardGold", 25) << " copper|r";
    info << "\n|cFF9370DBEntry Fee Default:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.DefaultEntryFee", 100) << " copper|r";
    info << "\n|cFF20B2AAMax Participants:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.DefaultMaxParticipants", 16) << "|r";
    info << "\n|cFFDC143CRegistration Hours:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.DefaultRegistrationHours", 24) << "|r";
    info << "\n\n|cFFFF6600Use tournament config commands to modify these settings.|r";
    
    handler.PSendSysMessage("%s", info.str().c_str());
    
    CloseGossipMenuFor(player);
}

void npc_1v1arena::ShowGeneralSettingsMenu(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Admin Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN);
    
    ChatHandler handler(player->GetSession());
    
    std::ostringstream info;
    info << "\n|cFFFFD700========== Tournament System Settings ==========|r";
    info << "\n|cFF00FF00Server Announcements:|r |c" << (sConfigMgr->GetOption<bool>("Tournament.EnableServerAnnouncements", true) ? "00FF00Enabled" : "FF0000Disabled") << "|r";
    info << "\n|cFF00FF00Arena Restrictions:|r |c" << (sConfigMgr->GetOption<bool>("Tournament.RestrictArenasDuringTournament", true) ? "00FF00Enabled" : "FF0000Disabled") << "|r";
    info << "\n|cFF00FF00Forfeit After Attempts:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.ForfeitAfterAttempts", 3) << "|r";
    info << "\n|cFF00FF00Auto Start:|r |c" << (sConfigMgr->GetOption<bool>("Tournament.AutoStartWhenRegistrationEnds", true) ? "00FF00Enabled" : "FF0000Disabled") << "|r";
    info << "\n|cFF00FF00Minimum Level:|r |cFFFFFFFF" << sConfigMgr->GetOption<uint32>("Tournament.MinimumLevel", 80) << "|r";
    info << "\n|cFF00FF00Allow Same IP:|r |c" << (sConfigMgr->GetOption<bool>("Tournament.AllowSameIP", false) ? "00FF00Yes" : "FF0000No") << "|r";
    info << "\n\n|cFFFF6600Use '.tournament config' command to view all settings.|r";
    info << "\n|cFFFF6600Modify settings in 1v1arena.conf file.|r";
    
    handler.PSendSysMessage("%s", info.str().c_str());
    
    CloseGossipMenuFor(player);
}

void npc_1v1arena::ShowAllTournamentsAdmin(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Admin Menu|r", GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_ADMIN);
    
    ChatHandler handler(player->GetSession());
    
    // Get all tournaments with detailed info
    QueryResult result = CharacterDatabase.Query(
        "SELECT id, name, status, current_participants, max_participants, "
        "UNIX_TIMESTAMP(registration_start), UNIX_TIMESTAMP(registration_end), "
        "UNIX_TIMESTAMP(tournament_start), UNIX_TIMESTAMP(tournament_end), "
        "winner_reward_gold, entry_fee "
        "FROM arena_tournaments ORDER BY id DESC LIMIT 10"
    );
    
    if (!result)
    {
        handler.PSendSysMessage("|cFFFF0000No tournaments found.|r");
        CloseGossipMenuFor(player);
        return;
    }
    
    std::ostringstream info;
    info << "\n|cFFFFD700========== All Tournaments (Recent 10) ==========|r";
    
    do
    {
        auto fields = result->Fetch();
        uint32 id = fields[0].Get<uint32>();
        std::string name = fields[1].Get<std::string>();
        std::string status = fields[2].Get<std::string>();
        uint32 current = fields[3].Get<uint32>();
        uint32 max = fields[4].Get<uint32>();
        time_t regStart = fields[5].Get<time_t>();
        time_t regEnd = fields[6].Get<time_t>();
        time_t tStart = fields[7].Get<time_t>();
        time_t tEnd = fields[8].Get<time_t>();
        uint32 winnerGold = fields[9].Get<uint32>();
        uint32 entryFee = fields[10].Get<uint32>();
        
        std::string statusColor;
        if (status == "registration") statusColor = "FFD700";
        else if (status == "ready") statusColor = "00FF00";
        else if (status == "active") statusColor = "00FFFF";
        else if (status == "finished") statusColor = "C0C0C0";
        else statusColor = "FF0000";
        
        info << "\n|cFF00FFFF[" << id << "]|r |cFFFFFFFF" << name << "|r";
        info << "\n   |cFF9370DBStatus:|r |c" << statusColor << status << "|r";
        info << "\n   |cFF9370DBParticipants:|r |cFFFFFFFF" << current << "/" << max << "|r";
        info << "\n   |cFF9370DBEntry Fee:|r |cFFFFD700" << (entryFee / 10000) << "g|r";
        info << "\n   |cFF9370DBWinner Prize:|r |cFFFFD700" << (winnerGold / 10000) << "g|r";
        
        if (tStart > 0)
            info << "\n   |cFF9370DBStarted:|r |cFFFFFFFF" << FormatTime(tStart) << "|r";
        else
            info << "\n   |cFF9370DBReg. End:|r |cFFFFFFFF" << FormatTime(regEnd) << "|r";
        
        info << "\n";
        
    } while (result->NextRow());
    
    handler.PSendSysMessage("%s", info.str().c_str());
    
    CloseGossipMenuFor(player);
}

void npc_1v1arena::CreateTournamentFromMenu(Player* player, const std::string& name, const std::string& description,
    uint32 entryFee, uint32 registrationHours, uint32 maxParticipants,
    uint32 winnerGold, uint32 winnerItem, uint32 winnerTitle,
    uint32 runnerUpGold, uint32 semiFinalistGold, uint32 winnerCount)
{
    // Create tournament using the enhanced system
    uint32 tournamentId = sTournamentSystem->CreateTournament(
        name, description, entryFee, registrationHours, maxParticipants,
        player->GetGUID().GetCounter(), winnerGold, winnerItem, winnerTitle
    );
    
    ChatHandler handler(player->GetSession());
    
    if (tournamentId > 0)
    {
        std::ostringstream success;
        success << "|cFF00FF00Tournament created successfully!|r";
        success << "\n|cFFFFD700Tournament ID:|r |cFFFFFFFF" << tournamentId << "|r";
        success << "\n|cFFFFD700Name:|r |cFFFFFFFF" << name << "|r";
        success << "\n|cFFFFD700Entry Fee:|r |cFFFFD700" << (entryFee / 10000) << " gold|r";
        success << "\n|cFFFFD700Registration:|r |cFFFFFFFF" << registrationHours << " hours|r";
        success << "\n|cFFFFD700Max Players:|r |cFFFFFFFF" << maxParticipants << "|r";
        success << "\n|cFFFFD700Winner Prize:|r |cFFFFD700" << (winnerGold / 10000) << " gold|r";
        if (winnerItem > 0)
            success << "\n|cFFFFD700Winner Item:|r |cFF00FF00Item ID " << winnerItem << "|r";
        if (winnerTitle > 0)
            success << "\n|cFFFFD700Winner Title:|r |cFFFF6600Title ID " << winnerTitle << "|r";
        
        handler.PSendSysMessage("%s", success.str().c_str());
        
        // Announce to server
        std::ostringstream announce;
        announce << "|cFFFFD700[Tournament]|r New tournament '|cFF00FF00" << name 
                 << "|r' created by GM! Entry fee: |cFFFFD700" << (entryFee / 10000) 
                 << "|r gold. Registration open for |cFF00FFFF" << registrationHours << "|r hours!";
        sWorld->SendServerMessage(SERVER_MSG_STRING, announce.str().c_str());
    }
    else
    {
        handler.PSendSysMessage("|cFFFF0000Failed to create tournament! Check server logs for details.|r");
    }
}

void npc_1v1arena::ShowTournamentParticipants(Player* player, Creature* creature, uint32 tournamentId)
{
    ClearGossipMenuFor(player);
    
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cFF87CEEB<- Back to Tournament Details|r", 
        GOSSIP_SENDER_MAIN, NPC_ARENA_1V1_ACTION_TOURNAMENT_DETAILS_BASE + tournamentId);
    
    ChatHandler handler(player->GetSession());
    
    // Get tournament name
    QueryResult tournamentResult = CharacterDatabase.Query(
        "SELECT name, status, current_participants FROM arena_tournaments WHERE id = {}", tournamentId
    );
    
    if (!tournamentResult)
    {
        handler.PSendSysMessage("|cFFFF0000Tournament not found!|r");
        ShowTournamentDetails(player, creature, tournamentId);
        return;
    }
    
    auto tFields = tournamentResult->Fetch();
    std::string tournamentName = tFields[0].Get<std::string>();
    std::string status = tFields[1].Get<std::string>();
    uint32 participantCount = tFields[2].Get<uint32>();
    
    // Get all registered participants
    QueryResult result = CharacterDatabase.Query(
        "SELECT reg.character_name, reg.registration_time, c.class, c.level "
        "FROM arena_tournament_registrations reg "
        "LEFT JOIN characters c ON reg.player_guid = c.guid "
        "WHERE reg.tournament_id = {} AND reg.status = 'confirmed' "
        "ORDER BY reg.registration_time ASC",
        tournamentId
    );
    
    std::ostringstream participants;
    participants << "\n|cFFFFD700========== Tournament Participants ==========|r";
    participants << "\n|cFFFFD700Tournament:|r |cFFFFFFFF" << tournamentName << "|r";
    participants << "\n|cFFFFD700Status:|r ";
    
    if (status == "registration") participants << "|cFF00FF00Registration Open|r";
    else if (status == "ready") participants << "|cFFFFD700Ready to Start|r";
    else if (status == "active") participants << "|cFF00FFFF Active|r";
    else if (status == "finished") participants << "|cFFC0C0C0Finished|r";
    else participants << "|cFFFF0000" << status << "|r";
    
    participants << "\n|cFFFFD700Total Participants:|r |cFFFFFFFF" << participantCount << "|r";
    participants << "\n\n|cFFFFD700--- Participant List ---|r";
    
    if (!result)
    {
        participants << "\n|cFFFF0000No participants registered yet.|r";
    }
    else
    {
        uint32 count = 1;
        do
        {
            auto fields = result->Fetch();
            std::string name = fields[0].Get<std::string>();
            time_t regTime = fields[1].Get<time_t>();
            uint8 playerClass = fields[2].Get<uint8>();
            uint32 level = fields[3].Get<uint32>();
            
            std::string classColor = GetClassColorById(playerClass);
            std::string className = GetClassNameById(playerClass);
            
            participants << "\n|cFFFFFFFF" << count << ".|r |c" << classColor << name << "|r";
            participants << " |cFFFFFFFF(L" << level << " " << className << ")|r";
            participants << " |cFF9370DB" << FormatTime(regTime) << "|r";
            
            count++;
        } while (result->NextRow());
    }
    
    participants << "\n|cFFFFD700============================================|r";
    
    handler.PSendSysMessage("%s", participants.str().c_str());
    
    CloseGossipMenuFor(player);
}

class team_1v1arena : public ArenaTeamScript
{
public:
    team_1v1arena() : ArenaTeamScript("team_1v1arena", {
        ARENATEAMHOOK_ON_GET_SLOT_BY_TYPE,
        ARENATEAMHOOK_ON_GET_ARENA_POINTS,
        ARENATEAMHOOK_ON_TYPEID_TO_QUEUEID,
        ARENATEAMHOOK_ON_QUEUEID_TO_ARENA_TYPE,
        ARENATEAMHOOK_ON_SET_ARENA_MAX_PLAYERS_PER_TEAM
    }) {}

    void OnGetSlotByType(const uint32 type, uint8& slot) override
    {
        if (type == ARENA_TEAM_1V1)
        {
            slot = sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3);
        }
    }

    void OnGetArenaPoints(ArenaTeam* at, float& points) override
    {
        if (at->GetType() == ARENA_TEAM_1V1)
        {
            const auto Members = at->GetMembers();
            uint8 playerLevel = sCharacterCache->GetCharacterLevelByGuid(Members.front().Guid);

            if (playerLevel >= sConfigMgr->GetOption<uint32>("Arena1v1.ArenaPointsMinLevel", 70))
                points *= sConfigMgr->GetOption<float>("Arena1v1.ArenaPointsMulti", 0.64f);
            else
                points *= 0;
        }
    }

    void OnTypeIDToQueueID(const BattlegroundTypeId, const uint8 arenaType, uint32& _bgQueueTypeId) override
    {
        if (arenaType == ARENA_TYPE_1V1)
        {
            _bgQueueTypeId = bgQueueTypeId;
        }
    }

    void OnQueueIdToArenaType(const BattlegroundQueueTypeId _bgQueueTypeId, uint8& arenaType) override
    {
        if (_bgQueueTypeId == bgQueueTypeId)
        {
            arenaType = ARENA_TYPE_1V1;
        }
    }

    void OnSetArenaMaxPlayersPerTeam(const uint8 type, uint32& maxPlayersPerTeam) override
    {
        if (type == ARENA_TYPE_1V1)
        {
            maxPlayersPerTeam = 1;
        }
    }
};

// Forward declarations
void AddSC_TournamentSystem();
void AddSC_tournament_commandscript();

void AddSC_npc_1v1arena()
{
    new configloader_1v1arena();
    new playerscript_1v1arena();
    new npc_1v1arena();
    new team_1v1arena();
    AddSC_TournamentSystem();
    AddSC_tournament_commandscript();
}
