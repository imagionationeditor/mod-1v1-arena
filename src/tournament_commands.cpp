#include "ScriptMgr.h"
#include "Chat.h"
#include "Player.h"
#include "TournamentSystem.h"
#include "Config.h"
#include <sstream>

class tournament_commandscript : public CommandScript
{
public:
    tournament_commandscript() : CommandScript("tournament_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> tournamentCommandTable =
        {
            { "create",   SEC_GAMEMASTER, false, &HandleTournamentCreateCommand,   "Create a new tournament" },
            { "start",    SEC_GAMEMASTER, false, &HandleTournamentStartCommand,    "Start a tournament" },
            { "cancel",   SEC_GAMEMASTER, false, &HandleTournamentCancelCommand,   "Cancel a tournament" },
            { "list",     SEC_GAMEMASTER, false, &HandleTournamentListCommand,     "List all tournaments" },
            { "info",     SEC_GAMEMASTER, false, &HandleTournamentInfoCommand,     "Get tournament info" },
            { "config",   SEC_GAMEMASTER, false, &HandleTournamentConfigCommand,   "Show tournament config" },
            { "test",     SEC_GAMEMASTER, false, &HandleTournamentTestCommand,     "Test tournament rewards" },
            { "register", SEC_PLAYER,     false, &HandleTournamentRegisterCommand, "Register for tournament" },
            { "bracket",  SEC_PLAYER,     false, &HandleTournamentBracketCommand,  "View tournament bracket" },
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "tournament", SEC_PLAYER, false, nullptr, "", tournamentCommandTable },
        };
        return commandTable;
    }

    static bool HandleTournamentCreateCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("Usage: .tournament create \"Tournament Name\" \"Description\" [entryFee] [registrationHours] [maxParticipants] [winnerReward] [itemReward] [titleReward]");
            return true;
        }

        std::string input = args;
        std::vector<std::string> tokens;
        std::stringstream ss(input);
        std::string token;
        bool inQuotes = false;
        std::string currentToken;

        for (char c : input)
        {
            if (c == '"')
            {
                inQuotes = !inQuotes;
                if (!inQuotes && !currentToken.empty())
                {
                    tokens.push_back(currentToken);
                    currentToken.clear();
                }
            }
            else if (c == ' ' && !inQuotes)
            {
                if (!currentToken.empty())
                {
                    tokens.push_back(currentToken);
                    currentToken.clear();
                }
            }
            else
            {
                currentToken += c;
            }
        }
        if (!currentToken.empty())
            tokens.push_back(currentToken);

        if (tokens.size() < 2)
        {
            handler->PSendSysMessage("Error: Name and description are required.");
            return true;
        }

        std::string name = tokens[0];
        std::string description = tokens[1];
        uint32 entryFee = tokens.size() > 2 ? std::stoul(tokens[2]) * 10000 : sConfigMgr->GetOption<uint32>("Tournament.DefaultEntryFee", 50) * 10000;
        uint32 regHours = tokens.size() > 3 ? std::stoul(tokens[3]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultRegistrationDuration", 48);
        uint32 maxParticipants = tokens.size() > 4 ? std::stoul(tokens[4]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultMaxParticipants", 64);
        uint32 winnerReward = tokens.size() > 5 ? std::stoul(tokens[5]) * 10000 : sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardGold", 500) * 10000;
        uint32 itemReward = tokens.size() > 6 ? std::stoul(tokens[6]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardItem", 0);
        uint32 titleReward = tokens.size() > 7 ? std::stoul(tokens[7]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerTitle", 0);

        uint32 tournamentId = sTournamentSystem->CreateTournament(
            name, description, entryFee, regHours, maxParticipants, 
            handler->GetSession()->GetPlayer()->GetGUID().GetCounter(),
            winnerReward, itemReward, titleReward
        );

        if (tournamentId > 0)
        {
            handler->PSendSysMessage("Tournament created successfully! ID: %u", tournamentId);
            handler->PSendSysMessage("Name: %s", name.c_str());
            handler->PSendSysMessage("Entry Fee: %u gold", entryFee / 10000);
            handler->PSendSysMessage("Registration Period: %u hours", regHours);
            handler->PSendSysMessage("Max Participants: %u", maxParticipants);
            handler->PSendSysMessage("Winner Reward: %u gold", winnerReward / 10000);
            if (itemReward > 0)
                handler->PSendSysMessage("Winner Item: ID %u", itemReward);
            if (titleReward > 0)
                handler->PSendSysMessage("Winner Title: ID %u", titleReward);
        }
        else
        {
            handler->PSendSysMessage("Failed to create tournament!");
        }

        return true;
    }

    static bool HandleTournamentStartCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("Usage: .tournament start <tournamentId>");
            return true;
        }

        uint32 tournamentId = atoi(args);
        if (tournamentId == 0)
        {
            handler->PSendSysMessage("Invalid tournament ID!");
            return true;
        }

        if (sTournamentSystem->StartTournament(tournamentId))
        {
            handler->PSendSysMessage("Tournament %u started successfully!", tournamentId);
        }
        else
        {
            handler->PSendSysMessage("Failed to start tournament %u!", tournamentId);
        }

        return true;
    }

    static bool HandleTournamentCancelCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("Usage: .tournament cancel <tournamentId>");
            return true;
        }

        uint32 tournamentId = atoi(args);
        if (tournamentId == 0)
        {
            handler->PSendSysMessage("Invalid tournament ID!");
            return true;
        }

        if (sTournamentSystem->CancelTournament(tournamentId))
        {
            handler->PSendSysMessage("Tournament %u cancelled successfully!", tournamentId);
        }
        else
        {
            handler->PSendSysMessage("Failed to cancel tournament %u!", tournamentId);
        }

        return true;
    }

    static bool HandleTournamentListCommand(ChatHandler* handler, const char* /*args*/)
    {
        std::vector<TournamentInfo> tournaments = sTournamentSystem->GetAllTournaments();
        
        if (tournaments.empty())
        {
            handler->PSendSysMessage("No tournaments found.");
            return true;
        }

        handler->PSendSysMessage("=== Tournament List ===");
        for (const auto& tournament : tournaments)
        {
            handler->PSendSysMessage("ID: %u | %s | Status: %s | Participants: %u/%u", 
                tournament.id, tournament.name.c_str(), 
                sTournamentSystem->GetTournamentStatusString(tournament.status).c_str(),
                tournament.currentParticipants, tournament.maxParticipants);
        }

        return true;
    }

    static bool HandleTournamentInfoCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("Usage: .tournament info <tournamentId>");
            return true;
        }

        uint32 tournamentId = atoi(args);
        if (tournamentId == 0)
        {
            handler->PSendSysMessage("Invalid tournament ID!");
            return true;
        }

        TournamentInfo info = sTournamentSystem->GetTournamentInfo(tournamentId);
        if (info.id == 0)
        {
            handler->PSendSysMessage("Tournament not found!");
            return true;
        }

        handler->PSendSysMessage("=== Tournament Info ===");
        handler->PSendSysMessage("ID: %u", info.id);
        handler->PSendSysMessage("Name: %s", info.name.c_str());
        handler->PSendSysMessage("Description: %s", info.description.c_str());
        handler->PSendSysMessage("Entry Fee: %u gold", info.entryFee / 10000);
        handler->PSendSysMessage("Participants: %u/%u", info.currentParticipants, info.maxParticipants);
        handler->PSendSysMessage("Status: %s", sTournamentSystem->GetTournamentStatusString(info.status).c_str());
        handler->PSendSysMessage("Winner Reward: %u gold", info.winnerRewardGold / 10000);

        return true;
    }

    static bool HandleTournamentConfigCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->PSendSysMessage("=== Tournament Configuration ===");
        handler->PSendSysMessage("System Enabled: %s", sConfigMgr->GetOption<bool>("Tournament.Enable", true) ? "Yes" : "No");
        handler->PSendSysMessage("Default Entry Fee: %u gold", sConfigMgr->GetOption<uint32>("Tournament.DefaultEntryFee", 50));
        handler->PSendSysMessage("Default Winner Reward: %u gold", sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardGold", 500));
        handler->PSendSysMessage("Default Runner-up Reward: %u gold", sConfigMgr->GetOption<uint32>("Tournament.RunnerUpRewardGold", 100));
        handler->PSendSysMessage("Default Semi-finalist Reward: %u gold", sConfigMgr->GetOption<uint32>("Tournament.SemiFinalistRewardGold", 25));
        handler->PSendSysMessage("Default Winner Item: %u", sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardItem", 0));
        handler->PSendSysMessage("Default Winner Title: %u", sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerTitle", 0));
        handler->PSendSysMessage("Default Registration Duration: %u hours", sConfigMgr->GetOption<uint32>("Tournament.DefaultRegistrationDuration", 48));
        handler->PSendSysMessage("Default Max Participants: %u", sConfigMgr->GetOption<uint32>("Tournament.DefaultMaxParticipants", 64));
        handler->PSendSysMessage("Min Participants to Start: %u", sConfigMgr->GetOption<uint32>("Tournament.MinimumParticipantsToStart", 2));
        handler->PSendSysMessage("Forfeit After Attempts: %u", sConfigMgr->GetOption<uint32>("Tournament.ForfeitAfterAttempts", 3));
        handler->PSendSysMessage("Server Announcements: %s", sConfigMgr->GetOption<bool>("Tournament.EnableServerAnnouncements", true) ? "Enabled" : "Disabled");
        handler->PSendSysMessage("Auto Cleanup Days: %u", sConfigMgr->GetOption<uint32>("Tournament.AutoCleanupDays", 30));
        
        return true;
    }

    static bool HandleTournamentTestCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("Usage: .tournament test rewards [gold] [item] [title]");
            handler->PSendSysMessage("This will give test rewards to your character");
            return true;
        }

        std::string command = args;
        if (command.find("rewards") == 0)
        {
            std::vector<std::string> tokens;
            std::stringstream ss(args);
            std::string token;
            while (ss >> token)
                tokens.push_back(token);

            uint32 goldAmount = tokens.size() > 1 ? std::stoul(tokens[1]) * 10000 : 100 * 10000; // Default 100 gold
            uint32 itemId = tokens.size() > 2 ? std::stoul(tokens[2]) : 0;
            uint32 titleId = tokens.size() > 3 ? std::stoul(tokens[3]) : 0;

            Player* player = handler->GetSession()->GetPlayer();
            
            // Give gold
            if (goldAmount > 0)
            {
                player->ModifyMoney(goldAmount);
                handler->PSendSysMessage("Added %u gold to your character", goldAmount / 10000);
            }
            
            // Give item (basic implementation)
            if (itemId > 0)
            {
                handler->PSendSysMessage("Item reward would be given: ID %u", itemId);
                // Note: Full item implementation would require ItemTemplate validation
            }
            
            // Give title (basic implementation)
            if (titleId > 0)
            {
                handler->PSendSysMessage("Title reward would be given: ID %u", titleId);
                // Note: Full title implementation would require CharTitles validation
            }
            
            handler->PSendSysMessage("Test rewards completed!");
        }
        else
        {
            handler->PSendSysMessage("Unknown test command. Available: rewards");
        }
        
        return true;
    }

    static bool HandleTournamentRegisterCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("Usage: .tournament register <tournamentId>");
            return true;
        }

        uint32 tournamentId = atoi(args);
        if (tournamentId == 0)
        {
            handler->PSendSysMessage("Invalid tournament ID!");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (sTournamentSystem->RegisterPlayer(tournamentId, player))
        {
            handler->PSendSysMessage("Successfully registered for tournament %u!", tournamentId);
        }
        else
        {
            handler->PSendSysMessage("Failed to register for tournament %u!", tournamentId);
        }

        return true;
    }

    static bool HandleTournamentBracketCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->PSendSysMessage("Usage: .tournament bracket <tournamentId>");
            return true;
        }

        uint32 tournamentId = atoi(args);
        if (tournamentId == 0)
        {
            handler->PSendSysMessage("Invalid tournament ID!");
            return true;
        }

        std::vector<TournamentRound> rounds = sTournamentSystem->GetTournamentBracket(tournamentId);
        
        if (rounds.empty())
        {
            handler->PSendSysMessage("No bracket data available for tournament %u.", tournamentId);
            return true;
        }

        handler->PSendSysMessage("=== Tournament Bracket ===");
        for (const auto& round : rounds)
        {
            handler->PSendSysMessage("== %s ==", round.roundName.c_str());
            
            for (const auto& match : round.matches)
            {
                if (match.player2Guid == 0)
                {
                    handler->PSendSysMessage("Match %u: %s (BYE)", match.matchNumber, match.player1Name.c_str());
                }
                else if (match.status == MATCH_STATUS_COMPLETED)
                {
                    std::string winnerName = (match.winnerGuid == match.player1Guid) ? match.player1Name : match.player2Name;
                    std::string loserName = (match.winnerGuid == match.player1Guid) ? match.player2Name : match.player1Name;
                    handler->PSendSysMessage("Match %u: %s def. %s", match.matchNumber, winnerName.c_str(), loserName.c_str());
                }
                else
                {
                    std::string status = (match.status == MATCH_STATUS_ACTIVE) ? "(Active)" : "(Pending)";
                    handler->PSendSysMessage("Match %u: %s vs %s %s", match.matchNumber, 
                        match.player1Name.c_str(), match.player2Name.c_str(), status.c_str());
                }
            }
        }

        return true;
    }
};

void AddSC_tournament_commandscript()
{
    new tournament_commandscript();
}
