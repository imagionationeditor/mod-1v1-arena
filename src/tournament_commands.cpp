#include "ScriptMgr.h"
#include "Chat.h"
#include "Player.h"
#include "TournamentSystem.h"
#include "Config.h"
#include "TournamentCurrency.h"
#include <sstream>

using namespace Acore::ChatCommands;

class tournament_commandscript : public CommandScript
{
public:
    tournament_commandscript() : CommandScript("tournament_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable tournamentCommandTable =
        {
            { "create",   HandleTournamentCreateCommand,   SEC_GAMEMASTER, Console::No, "Create a new tournament" },
            { "start",    HandleTournamentStartCommand,    SEC_GAMEMASTER, Console::No, "Start a tournament" },
            { "cancel",   HandleTournamentCancelCommand,   SEC_GAMEMASTER, Console::No, "Cancel a tournament" },
            { "list",     HandleTournamentListCommand,     SEC_GAMEMASTER, Console::No, "List all tournaments" },
            { "info",     HandleTournamentInfoCommand,     SEC_GAMEMASTER, Console::No, "Get tournament info" },
            { "config",   HandleTournamentConfigCommand,   SEC_GAMEMASTER, Console::No, "Show tournament config" },
            { "test",     HandleTournamentTestCommand,     SEC_GAMEMASTER, Console::No, "Test tournament rewards" },
            { "register", HandleTournamentRegisterCommand, SEC_PLAYER,     Console::No, "Register for tournament" },
            { "bracket",  HandleTournamentBracketCommand,  SEC_PLAYER,     Console::No, "View tournament bracket" },
        };

        static ChatCommandTable commandTable =
        {
            { "tournament", tournamentCommandTable },
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

    uint32 entryFee;
    uint32 regHours;
    uint32 maxParticipants;
    uint32 winnerReward;
    uint32 itemReward;
    uint32 titleReward;
    
    try {
        uint32 baseEntryFee = tokens.size() > 2 ? std::stoul(tokens[2]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultEntryFee", 50);
        entryFee = TournamentCurrency::ToCopper(baseEntryFee);
        regHours = tokens.size() > 3 ? std::stoul(tokens[3]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultRegistrationDuration", 48);
        maxParticipants = tokens.size() > 4 ? std::stoul(tokens[4]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultMaxParticipants", 64);
        uint32 baseWinnerReward = tokens.size() > 5 ? std::stoul(tokens[5]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardGold", 500);
        winnerReward = TournamentCurrency::ToCopper(baseWinnerReward);
        itemReward = tokens.size() > 6 ? std::stoul(tokens[6]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardItem", 0);
        titleReward = tokens.size() > 7 ? std::stoul(tokens[7]) : sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerTitle", 0);
    } catch (const std::exception&) {
        handler->PSendSysMessage("Error: invalid numeric argument.");
        return true;
    }

        uint32 tournamentId = sTournamentSystem->CreateTournament(
            name, description, entryFee, regHours, maxParticipants, 
            handler->GetSession()->GetPlayer()->GetGUID().GetCounter(),
            winnerReward, itemReward, titleReward
        );

        if (tournamentId > 0)
        {
            handler->PSendSysMessage("Tournament created successfully! ID: {}", tournamentId);
            handler->PSendSysMessage("Name: {}", name);
            handler->PSendSysMessage("Entry Fee: {} gold", TournamentCurrency::ToGold(entryFee));
            handler->PSendSysMessage("Registration Period: {} hours", regHours);
            handler->PSendSysMessage("Max Participants: {}", maxParticipants);
            handler->PSendSysMessage("Winner Reward: {} gold", TournamentCurrency::ToGold(winnerReward));
            if (itemReward > 0)
                handler->PSendSysMessage("Winner Item: ID {}", itemReward);
            if (titleReward > 0)
                handler->PSendSysMessage("Winner Title: ID {}", titleReward);
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
            handler->PSendSysMessage("Tournament {} started successfully!", tournamentId);
        }
        else
        {
            handler->PSendSysMessage("Failed to start tournament {}!", tournamentId);
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
            handler->PSendSysMessage("Tournament {} cancelled successfully!", tournamentId);
        }
        else
        {
            handler->PSendSysMessage("Failed to cancel tournament {}!", tournamentId);
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
            handler->PSendSysMessage("ID: {} | {} | Status: {} | Participants: {}/{}", 
                tournament.id, tournament.name, 
                sTournamentSystem->GetTournamentStatusString(tournament.status),
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
        handler->PSendSysMessage("ID: {}", info.id);
        handler->PSendSysMessage("Name: {}", info.name);
        handler->PSendSysMessage("Description: {}", info.description);
        handler->PSendSysMessage("Entry Fee: {} gold", TournamentCurrency::ToGold(info.entryFee));
        handler->PSendSysMessage("Participants: {}/{}", info.currentParticipants, info.maxParticipants);
        handler->PSendSysMessage("Status: {}", sTournamentSystem->GetTournamentStatusString(info.status));
        handler->PSendSysMessage("Winner Reward: {} gold", TournamentCurrency::ToGold(info.winnerRewardGold));

        return true;
    }

    static bool HandleTournamentConfigCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->PSendSysMessage("=== Tournament Configuration ===");
        handler->PSendSysMessage("System Enabled: {}", sConfigMgr->GetOption<bool>("Tournament.Enable", true) ? "Yes" : "No");
        handler->PSendSysMessage("Default Entry Fee: {} gold", sConfigMgr->GetOption<uint32>("Tournament.DefaultEntryFee", 50));
        handler->PSendSysMessage("Default Winner Reward: {} gold", sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardGold", 500));
        handler->PSendSysMessage("Default Runner-up Reward: {} gold", sConfigMgr->GetOption<uint32>("Tournament.RunnerUpRewardGold", 100));
        handler->PSendSysMessage("Default Semi-finalist Reward: {} gold", sConfigMgr->GetOption<uint32>("Tournament.SemiFinalistRewardGold", 25));
        handler->PSendSysMessage("Default Winner Item: {}", sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerRewardItem", 0));
        handler->PSendSysMessage("Default Winner Title: {}", sConfigMgr->GetOption<uint32>("Tournament.DefaultWinnerTitle", 0));
        handler->PSendSysMessage("Default Registration Duration: {} hours", sConfigMgr->GetOption<uint32>("Tournament.DefaultRegistrationDuration", 48));
        handler->PSendSysMessage("Default Max Participants: {}", sConfigMgr->GetOption<uint32>("Tournament.DefaultMaxParticipants", 64));
        handler->PSendSysMessage("Min Participants to Start: {}", sConfigMgr->GetOption<uint32>("Tournament.MinimumParticipantsToStart", 2));
        handler->PSendSysMessage("Forfeit After Attempts: {}", sConfigMgr->GetOption<uint32>("Tournament.ForfeitAfterAttempts", 3));
        handler->PSendSysMessage("Server Announcements: {}", sConfigMgr->GetOption<bool>("Tournament.EnableServerAnnouncements", true) ? "Enabled" : "Disabled");
        handler->PSendSysMessage("Auto Cleanup Days: {}", sConfigMgr->GetOption<uint32>("Tournament.AutoCleanupDays", 30));
        
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

    uint32 goldAmount;
    uint32 itemId;
    uint32 titleId;
    
    try {
        uint32 baseGoldAmount = tokens.size() > 1 ? std::stoul(tokens[1]) : 100; // Default 100 gold
        goldAmount = TournamentCurrency::ToCopper(baseGoldAmount);
        itemId = tokens.size() > 2 ? std::stoul(tokens[2]) : 0;
        titleId = tokens.size() > 3 ? std::stoul(tokens[3]) : 0;
    } catch (const std::exception&) {
        handler->PSendSysMessage("Error: invalid numeric argument.");
        return true;
    }

            Player* player = handler->GetSession()->GetPlayer();
            
            // Give gold
            if (goldAmount > 0)
            {
                player->ModifyMoney(goldAmount);
                handler->PSendSysMessage("Added {} gold to your character", TournamentCurrency::ToGold(goldAmount));
            }
            
            // Give item (basic implementation)
            if (itemId > 0)
            {
                handler->PSendSysMessage("Item reward would be given: ID {}", itemId);
                // Note: Full item implementation would require ItemTemplate validation
            }
            
            // Give title (basic implementation)
            if (titleId > 0)
            {
                handler->PSendSysMessage("Title reward would be given: ID {}", titleId);
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
            handler->PSendSysMessage("Successfully registered for tournament {}!", tournamentId);
        }
        else
        {
            handler->PSendSysMessage("Failed to register for tournament {}!", tournamentId);
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
            handler->PSendSysMessage("No bracket data available for tournament {}.", tournamentId);
            return true;
        }

        handler->PSendSysMessage("=== Tournament Bracket ===");
        for (const auto& round : rounds)
        {
            handler->PSendSysMessage("== {} ==", round.roundName);
            
            for (const auto& match : round.matches)
            {
                if (match.player2Guid == 0)
                {
                    handler->PSendSysMessage("Match {}: {} (BYE)", match.matchNumber, match.player1Name);
                }
                else if (match.status == MATCH_STATUS_COMPLETED)
                {
                    std::string winnerName = (match.winnerGuid == match.player1Guid) ? match.player1Name : match.player2Name;
                    std::string loserName = (match.winnerGuid == match.player1Guid) ? match.player2Name : match.player1Name;
                    handler->PSendSysMessage("Match {}: {} def. {}", match.matchNumber, winnerName, loserName);
                }
                else
                {
                    std::string status = (match.status == MATCH_STATUS_ACTIVE) ? "(Active)" : "(Pending)";
                    handler->PSendSysMessage("Match {}: {} vs {} {}", match.matchNumber, 
                        match.player1Name, match.player2Name, status);
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
