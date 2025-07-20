# Arena Tournament NPC Installation Guide

## Installation Steps:

### 1. Execute SQL in Database
```sql
-- Execute in world database:
SOURCE path/to/arena_npc_spawn.sql;
```

### 2. Created NPC List:

#### 🏰 **NPC ID: 190000**
**Name:** Arena Master  
**Subname:** 1v1 Arena & Tournament Manager  
**Script:** npc_1v1arena

#### 📍 **Spawn Locations:**

1. **Stormwind City (Alliance)**
   - Location: Near Arena Battlemaster
   - Coordinates: X: -8759.84, Y: 877.65, Z: 96.97
   - GUID: 800001

2. **Orgrimmar (Horde)**
   - Location: Near Arena Battlemaster
   - Coordinates: X: 1983.92, Y: -4794.2, Z: 56.7
   - GUID: 800002

3. **Dalaran (Neutral)**
   - Location: Arena area
   - Coordinates: X: 5776.17, Y: 721.3, Z: 641.9
   - GUID: 800003

4. **Shattrath City (Outland)**
   - Location: Lower City
   - Coordinates: X: -1749.63, Y: 5138.56, Z: -39.87
   - GUID: 800004

5. **Ironforge (Optional)**
   - Coordinates: X: -4624.66, Y: -926.87, Z: 501.06
   - GUID: 800005

6. **Thunder Bluff (Optional)**
   - Coordinates: X: -1196.53, Y: 29.84, Z: 176.05
   - GUID: 800006

### 3. Useful Commands:

#### Delete all NPCs:
```sql
DELETE FROM creature WHERE id1 = 190000;
```

#### Change NPC position:
```sql
UPDATE creature SET position_x = X, position_y = Y, position_z = Z WHERE guid = GUID_NUMBER;
```

#### Add NPC at custom location:
```sql
INSERT INTO creature (guid, id1, map, position_x, position_y, position_z, orientation, spawntimesecs) 
VALUES (YOUR_GUID, 190000, MAP_ID, X, Y, Z, ORIENTATION, 300);
```

### 4. Additional Settings:

#### Change NPC model:
```sql
UPDATE creature_template SET modelid1 = NEW_MODEL_ID WHERE entry = 190000;
```

#### Change name and subname:
```sql
UPDATE creature_template SET name = 'New Name', subname = 'New Subtitle' WHERE entry = 190000;
```

### 5. Important Notes:

- **GUIDs must be unique** - If conflicts occur, use higher numbers
- **Map IDs:**
  - 0 = Eastern Kingdoms (Stormwind, Ironforge)
  - 1 = Kalimdor (Orgrimmar, Thunder Bluff)
  - 530 = Outland (Shattrath)
  - 571 = Northrend (Dalaran)

### 6. Installation Verification:

```sql
-- Check template
SELECT * FROM creature_template WHERE entry = 190000;

-- Check spawns
SELECT * FROM creature WHERE id1 = 190000;
```

---

## 🎮 NPC Features:

- **Regular Players:** 1v1 Arena, Tournaments, Statistics, Registration
- **GM/Admin:** Complete tournament management, settings, rewards

## 🔧 Troubleshooting:

If NPC doesn't appear:
1. Restart the server
2. Execute `.reload creature_template` command
3. Check for GUID conflicts
4. Verify coordinates are correct

### Common Issues:

#### GUID Conflicts:
```sql
-- Find existing GUIDs to avoid conflicts
SELECT guid FROM creature WHERE guid BETWEEN 800001 AND 800010;

-- Use different GUID range if needed
UPDATE creature SET guid = 900001 WHERE guid = 800001;
```

#### Script Not Loading:
- Make sure the module is compiled and loaded
- Check server startup logs for script errors
- Verify ScriptName matches exactly: `npc_1v1arena`

#### NPC Not Responsive:
```sql
-- Verify npcflag is set correctly
UPDATE creature_template SET npcflag = 1 WHERE entry = 190000;
```

---

## 🎯 In-Game GM Commands:

### NPC Management:
```
.npc add 190000                    # Spawn NPC at current location
.npc delete                        # Delete selected NPC
.npc info                          # Show NPC information
.go creature 800001                # Teleport to Stormwind Arena Master
.reload creature_template          # Reload NPC templates
```

### Finding Coordinates:
```
.gps                               # Show current coordinates
.appear PLAYER_NAME                # Teleport to player location
.summon PLAYER_NAME                # Summon player to your location
```

---

## 📊 Database Queries:

### Check Installation:
```sql
-- Verify template exists
SELECT entry, name, subname, ScriptName FROM creature_template WHERE entry = 190000;

-- Check all spawned NPCs
SELECT guid, map, position_x, position_y, position_z FROM creature WHERE id1 = 190000;

-- Verify tournament tables exist
SHOW TABLES LIKE 'arena_tournament%';
```

### Maintenance:
```sql
-- Remove inactive tournaments (older than 30 days)
DELETE FROM arena_tournaments WHERE created_at < DATE_SUB(NOW(), INTERVAL 30 DAY) AND status = 'finished';

-- Reset stuck tournaments
UPDATE arena_tournaments SET status = 'cancelled' WHERE status IN ('registration', 'ready') AND registration_end < NOW();
```

---

## 🌍 Multiple Language Support:

To change NPC text language, modify the creature_template:

```sql
-- English (Default)
UPDATE creature_template SET name = 'Arena Master', subname = '1v1 Arena & Tournament Manager' WHERE entry = 190000;

-- Spanish
UPDATE creature_template SET name = 'Maestro de Arena', subname = 'Gestor de Arena 1v1 y Torneos' WHERE entry = 190000;

-- French  
UPDATE creature_template SET name = 'Maître d\'Arène', subname = 'Gestionnaire d\'Arène 1v1 et Tournois' WHERE entry = 190000;

-- German
UPDATE creature_template SET name = 'Arena Meister', subname = '1v1 Arena & Turnier Manager' WHERE entry = 190000;
```

---

## 🔐 Security Notes:

- Only GMs with `SEC_GAMEMASTER` or higher can access admin functions
- Tournament creation via chat requires GM privileges
- All admin actions are logged in server logs
- Player registration requires sufficient gold and arena team (for rated queues)

---

## 📋 Quick Reference:

| Function | Player Access | GM Access | Description |
|----------|--------------|-----------|-------------|
| View Tournaments | ✅ | ✅ | See active tournaments |
| Register for Tournament | ✅ | ✅ | Join tournament (with entry fee) |
| View Brackets | ✅ | ✅ | See tournament progress |
| Create Tournament | ❌ | ✅ | Admin only - create new tournaments |
| Start Tournament | ❌ | ✅ | Admin only - begin tournament |
| Cancel Tournament | ❌ | ✅ | Admin only - cancel with refunds |
| View Settings | ❌ | ✅ | Admin only - system configuration |
