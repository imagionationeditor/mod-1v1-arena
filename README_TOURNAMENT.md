# 1v1 Arena Tournament System

## Overview
This enhanced version of the 1v1 Arena module includes a comprehensive tournament system that allows administrators to create and manage competitive tournaments.

## Features

### Tournament System
- **Admin-Created Tournaments**: Game masters can create tournaments with custom settings
- **Registration System**: Players pay an entry fee to register for tournaments
- **Bracket Generation**: Automatic single-elimination bracket generation
- **Tournament Progression**: Automatic round advancement and winner determination
- **Forfeiture System**: Players who fail to join matches 3 times are automatically disqualified
- **Rewards System**: Winners receive gold, items, and titles
- **Real-time Announcements**: Server-wide announcements for tournament events

### Tournament Features
- **Entry Fees**: Configurable gold cost for registration (default: 50 gold)
- **Registration Period**: Time-limited registration window (default: 2 days)
- **Arena Restriction**: During active tournaments, only registered players can queue for 1v1 arena
- **Bracket Viewing**: Players can view tournament brackets and match results
- **Statistics Tracking**: Comprehensive player and tournament statistics
- **Admin Controls**: Full tournament management via in-game commands

## Installation

1. **Database Setup**: Execute the SQL file to create tournament tables:
   ```sql
   SOURCE data/sql/db-world/tournament_system.sql
   ```

2. **Configuration**: Update your worldserver configuration with tournament settings:
   ```ini
   # Tournament system enable/disable
   Tournament.Enable = 1
   
   # Default tournament settings
   Tournament.DefaultEntryFee = 500000  # 50 gold in copper
   Tournament.DefaultReward = 5000000   # 500 gold in copper
   Tournament.MaxParticipants = 64
   ```

3. **Compile**: The module will automatically be included when building AzerothCore.

## Usage

### For Administrators

#### Creating a Tournament
```
.tournament create "Tournament Name" "Description" [entryFee] [registrationHours] [maxParticipants]
```
Example:
```
.tournament create "Weekly Championship" "Fight for glory and gold!" 50 48 32
```

#### Managing Tournaments
```
.tournament start <tournamentId>    # Start a tournament
.tournament cancel <tournamentId>   # Cancel a tournament
.tournament list                    # List all tournaments
.tournament info <tournamentId>     # Get tournament details
```

#### Viewing Tournament Data
```
.tournament bracket <tournamentId>  # View tournament bracket
```

### For Players

#### NPC Interface
Players can interact with the 1v1 Arena NPC to:
- View active tournaments
- Register for tournaments
- Check tournament brackets
- View personal tournament statistics
- See tournament leaderboards

#### Commands
```
.tournament register <tournamentId>  # Register for a tournament
.tournament bracket <tournamentId>   # View tournament bracket
```

## Tournament Flow

1. **Creation**: Admin creates tournament with registration period
2. **Registration**: Players pay entry fee to register during the registration window
3. **Start**: Admin starts the tournament (or auto-starts after registration period)
4. **Bracket Generation**: System creates random seeded single-elimination bracket
5. **Rounds**: Players compete in matches, winners advance
6. **Finals**: Tournament continues until one winner remains
7. **Rewards**: Winner receives gold, items, and titles with server announcement

## Arena Restrictions During Tournaments

When a tournament is active:
- Only registered tournament participants can queue for 1v1 arena
- Regular arena queuing is disabled for non-participants
- This ensures tournament integrity and prevents interference

## Forfeiture System

- Players have 3 attempts to join their tournament match
- If a player fails to join 3 times, they are automatically disqualified
- The opponent advances to the next round
- If both players forfeit, a random winner is selected

## Database Tables

The tournament system uses the following tables:
- `arena_tournaments`: Tournament information
- `arena_tournament_registrations`: Player registrations
- `arena_tournament_rounds`: Tournament rounds
- `arena_tournament_matches`: Individual matches
- `arena_tournament_history`: Completed tournament records
- `arena_tournament_player_stats`: Player statistics

## Configuration Options

### Core Settings
- `Arena1v1.Enable`: Enable/disable the 1v1 arena system
- `Arena1v1.MinLevel`: Minimum level to participate (default: 80)
- `Arena1v1.Costs`: Cost to create an arena team (default: 40 gold)

### Tournament Settings (in conf files)
- `TOURNAMENT_ENABLED`: Enable tournament system
- `TOURNAMENT_DEFAULT_ENTRY_FEE`: Default entry fee in copper
- `TOURNAMENT_DEFAULT_WINNER_REWARD`: Default winner reward in copper
- `TOURNAMENT_DEFAULT_MAX_PARTICIPANTS`: Default max participants

## Troubleshooting

### Common Issues

1. **Players can't register**: Check if registration period is still active and player has enough gold
2. **Tournament won't start**: Ensure minimum 2 players are registered
3. **Matches not progressing**: Check for offline players (auto-forfeit after 3 attempts)

### Logs
Tournament events are logged to the `tournament` log category. Enable debug logging for detailed information:
```ini
Logger.tournament=3,Console Server
```

## API Reference

### TournamentSystem Methods
- `CreateTournament()`: Create new tournament
- `RegisterPlayer()`: Register player for tournament
- `StartTournament()`: Begin tournament
- `ProcessMatchResult()`: Handle match completion
- `GetActiveTournaments()`: Get list of active tournaments

## Contributing

When contributing to the tournament system:
1. Follow AzerothCore coding standards
2. Add appropriate logging for debugging
3. Update this documentation for new features
4. Test with multiple tournament scenarios

## Credits

- Original 1v1 Arena module by various contributors
- Tournament system enhancement developed for AzerothCore
- Based on AzerothCore framework and standards
