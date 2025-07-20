# mod-1v1-arena with Tournament System

[![Core](https://img.shields.io/badge/Core-AzerothCore-orange.svg)](http://www.azerothcore.org/)
[![License](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)

## 🏆 Overview

This is an enhanced version of the popular 1v1 Arena module for AzerothCore that includes a comprehensive tournament system. Players can participate in automated tournaments with bracket progression, multi-tier rewards, and professional tournament management via an intuitive NPC interface.

**Developer**: [Mojispectre]

## ✨ Features

### Core 1v1 Arena Features:
- ⚔️ **Solo Arena Combat** - 1v1 arena matches with queue system
- 🏅 **Rating System** - MMR-based matchmaking and rankings
- 📊 **Statistics Tracking** - Win/loss ratios and match history
- 🛡️ **Queue Management** - Automatic matchmaking with skill-based pairing
- 🎯 **Spectator Mode** - Watch ongoing 1v1 matches

### Tournament System Enhancements:
- 🏆 **Tournament Management** - Complete tournament lifecycle automation
- 📋 **Registration System** - Entry fee tournaments with player registration
- 🎖️ **Multi-Tier Rewards** - Winner, runner-up, and semi-finalist prizes
- 👑 **Admin Interface** - GM-only tournament creation and management
- 🎮 **NPC Integration** - Professional Arena Master NPC interface
- ⚙️ **Configuration System** - Fully configurable rewards and settings
- 📈 **Statistics & History** - Complete tournament tracking and analytics

## 🚀 Installation

### Prerequisites:
- AzerothCore (latest version)
- MySQL/MariaDB database
- CMake build system

### Quick Setup:

1. **Clone the module** to your AzerothCore modules directory:
```bash
cd /path/to/azerothcore/modules/
git clone https://github.com/azerothcore/mod-1v1-arena.git
```

2. **Apply SQL scripts** to your world database:
```bash
# Core arena tables
mysql -u root -p world < data/sql/db-world/base/1v1_arena.sql

# Tournament system tables  
mysql -u root -p world < data/sql/db-world/tournament_system.sql

# NPC spawns (optional - for automatic NPC placement)
mysql -u root -p world < data/sql/db-world/arena_npc_spawn.sql
```

3. **Configure the module** by copying and editing the config file:
```bash
cp conf/1v1arena.conf.dist conf/1v1arena.conf
# Edit conf/1v1arena.conf with your preferred settings
```

4. **Rebuild AzerothCore** with the module:
```bash
cd /path/to/azerothcore/build/
make -j$(nproc)
```

5. **Restart your server** and enjoy!

### Manual NPC Spawning:
If you prefer to spawn NPCs manually:
```
.npc add 190000  # Spawns Arena Master NPC at your location
```

## 🎮 How to Use

### For Players:

1. **Find an Arena Master NPC** in major cities (Stormwind, Orgrimmar, Dalaran, etc.)
2. **Create a 1v1 Arena Team** via the NPC interface
3. **Join the Queue** for solo arena matches
4. **Browse Active Tournaments** and register with entry fees
5. **Compete and Win** to earn rewards and climb rankings!

### Tournament Participation:
- Browse active tournaments with detailed information
- Pay entry fees to register for tournaments
- Automatic bracket progression through arena wins
- Receive tiered rewards based on tournament placement

### For Game Masters:

#### Tournament Management:
```
# Create quick tournament with default settings
Use Arena Master NPC → Admin Menu → Quick Tournament

# Advanced tournament creation  
Use Arena Master NPC → Admin Menu → Advanced Creation

# Chat-based tournament creation
Type tournament name in say/yell chat near Arena Master NPC
```

#### GM Commands:
```
.arena tournament create <name> <entry_fee> <max_participants>
.arena tournament start <tournament_id>
.arena tournament cancel <tournament_id>
.arena tournament list
.arena tournament info <tournament_id>
.reload config        # Reload configuration without restart
```

## ⚙️ Configuration

The module is highly configurable via `conf/1v1arena.conf`:

### Arena Settings:
```ini
# Enable/disable 1v1 arena system
Arena1v1.Enable = 1

# Minimum level requirement
Arena1v1.MinLevel = 80

# Queue restrictions
Arena1v1.RestrictBG = 1
Arena1v1.RestrictRaid = 1
```

### Tournament Settings:
```ini
# Tournament system enable/disable
Arena1v1.Tournament.Enable = 1

# Default tournament settings
Arena1v1.Tournament.DefaultEntryFee = 100
Arena1v1.Tournament.DefaultMaxParticipants = 16

# Reward configuration
Arena1v1.Tournament.WinnerGold = 500
Arena1v1.Tournament.RunnerUpGold = 100
Arena1v1.Tournament.SemiFinalistGold = 25

# Announcements
Arena1v1.Tournament.AnnounceStart = 1
Arena1v1.Tournament.AnnounceWinners = 1
```

## 🏗️ Database Schema

### Core Tables:
- `arena_team_1v1` - Solo arena team management
- `arena_team_member_1v1` - Team membership tracking
- `character_arena_stats_1v1` - Player statistics

### Tournament Tables:
- `arena_tournaments` - Tournament main data
- `tournament_participants` - Player registrations
- `tournament_brackets` - Bracket progression
- `tournament_matches` - Match results
- `tournament_rewards` - Reward distribution
- `tournament_history` - Historical data

## 🎨 NPC Interface

### Main Menu:
- **Create Arena Team** - Set up your 1v1 team
- **Join Queue** - Enter matchmaking system
- **Leave Queue** - Exit queue if needed
- **Statistics** - View your arena performance
- **Tournament Hub** - Browse and join tournaments
- **[GM] Admin Panel** - Tournament management (GM only)

### Tournament Hub:
- **Active Tournaments** - Color-coded tournament status
- **Tournament Details** - Participants, rewards, brackets
- **Registration** - Join tournaments with entry fee
- **My Tournaments** - Personal tournament history
- **Tournament Rules** - System information

### Admin Panel (GM Only):
- **Quick Tournament** - Create tournament with defaults
- **Advanced Creation** - Step-by-step tournament setup
- **Manage Tournaments** - Start/cancel/monitor
- **System Settings** - View current configuration
- **Tournament Analytics** - Statistics and reports

## 🏆 Tournament System

### Tournament Types:
- **Entry Fee Tournaments** - Players pay to participate
- **Free Tournaments** - No cost to join (set entry fee to 0)
- **Special Events** - Custom rewards and rules
- **Seasonal Championships** - Enhanced prize pools

### Bracket System:
- **Single Elimination** - Traditional tournament brackets
- **Automatic Progression** - Winners advance automatically
- **Forfeit Handling** - Automatic advancement for no-shows
- **Real-time Updates** - Live bracket status tracking

### Reward Tiers:
1. **Winner** - Full prize pool (configurable gold + items + titles)
2. **Runner-up** - Second place reward (configurable)
3. **Semi-finalists** - Participation reward (configurable)

## 🛡️ Security & Permissions

- **GM Authentication** - Admin functions require SEC_GAMEMASTER
- **Input Validation** - All user inputs are sanitized
- **SQL Protection** - Prepared statements prevent injection
- **Access Control** - Role-based feature access
- **Audit Logging** - Complete action history tracking

## 🔧 Customization

### Adding Custom Rewards:
```cpp
// In TournamentSystem.cpp
void TournamentSystem::DistributeRewards(uint32 tournamentId) {
    // Add custom item rewards
    player->AddItem(ITEM_ID, QUANTITY);
    
    // Add custom titles
    CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(TITLE_ID);
    player->SetTitle(titleEntry);
}
```

### Custom Tournament Types:
- Modify `tournament_types` enum for new tournament categories
- Add configuration options in `.conf` file
- Implement custom logic in `TournamentSystem` class

## 📊 Statistics & Analytics

### Player Statistics:
- Individual win/loss ratios
- Tournament participation history
- Average placement rankings
- Total rewards earned

### Server Analytics:
- Tournament frequency and popularity
- Player engagement metrics
- Reward distribution analysis
- System performance monitoring

## 🐛 Troubleshooting

### Common Issues:

**NPCs not spawning:**
```sql
-- Check if creature template exists
SELECT * FROM creature_template WHERE entry = 190000;

-- Verify spawn locations
SELECT * FROM creature WHERE id1 = 190000;
```

**Tournament not starting:**
- Verify minimum participants requirement
- Check GM permissions for tournament creator
- Ensure all participants have valid arena teams

**Configuration not loading:**
- Check `conf/1v1arena.conf` file permissions
- Verify configuration syntax
- Use `.reload config` command to refresh settings

### Debug Commands:
```
.arena debug tournament <tournament_id>  # Detailed tournament info
.arena debug player <player_name>        # Player's tournament status
.arena debug system                       # System health check
```

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Guidelines:
- Follow AzerothCore coding standards
- Include comprehensive comments
- Test all new features thoroughly
- Update documentation for changes

## 📝 License

This project is licensed under the GNU Affero General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## 🙏 Credits

- **Original 1v1 Arena Module**: AzerothCore Community
- **Tournament System Enhancement**: [Mojispectre]
- **AzerothCore Team**: For the amazing server emulator
- **Community Contributors**: For testing, feedback, and improvements

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/azerothcore/mod-1v1-arena/issues)
- **Discord**: [AzerothCore Discord](https://discord.gg/gkt4y2x)
- **Documentation**: See `docs/` folder for detailed guides
- **Wiki**: [AzerothCore Wiki](https://www.azerothcore.org/wiki/)

## 🌟 Features Roadmap

### Planned Enhancements:
- [ ] **Round Robin Tournaments** - Alternative tournament format
- [ ] **Team Tournaments** - Multi-player team competitions  
- [ ] **Seasonal Leagues** - Long-term competitive seasons
- [ ] **Live Streaming Integration** - Spectator mode enhancements
- [ ] **Mobile Tournament Management** - Web-based admin interface
- [ ] **Advanced Analytics** - Detailed performance metrics
- [ ] **Custom Arena Maps** - Tournament-specific battlegrounds

---

**Enjoy your enhanced 1v1 Arena experience with comprehensive tournament management!** 🏆

*For detailed installation and usage instructions, please refer to the documentation in the `docs/` folder.*
# ![logo](https://raw.githubusercontent.com/azerothcore/azerothcore.github.io/master/images/logo-github.png) AzerothCore

## mod-1v1-Arena

### This is a module for [AzerothCore](http://www.azerothcore.org)

- Latest build status with azerothcore:

[![Build Status](https://github.com/azerothcore/mod-1v1-arena/workflows/core-build/badge.svg?branch=master&event=push)](https://github.com/azerothcore/mod-1v1-arena)

![icon](https://github.com/azerothcore/mod-1v1-arena/assets/2810187/ba375f70-71c7-4c71-aa3f-d3ab6c6fdda7)

#### Features:

- 1v1 Rated Arena Games
- 1v1 Unrated Arena Games

### This module currently requires:

- AzerothCore v1.0.1+

### How to install

1. Simply place the module under the `modules` folder of your AzerothCore source folder.
2. Re-run cmake and launch a clean build of AzerothCore
3. Run the Sql file into your database.
4. Ready.



![image](https://github.com/user-attachments/assets/e265800d-847d-4e3e-9635-644804eb1b2f)


### Info

This module runs over the 5v5. You can configure it to run over 4v4/3v3..etc As you desire.

If you leave the default value at 3, you can have 1v1, 2v2, 3v3 and 5v5. Unless you want to replace it with one of the existing ones.

## Credits
* [XDev](https://github.com/XdevTLKWoW)
* Adjusted by fr4z3n for azerothcore
* Written by Teiby

AzerothCore: [repository](https://github.com/azerothcore) - [website](http://azerothcore.org) - [discord chat community](https://discord.gg/PaqQRkd)
