-- ============================================================================
-- 1v1 Arena Tournament NPC Spawn Scripts
-- ============================================================================

-- Delete existing NPCs (if any)
DELETE FROM `creature` WHERE `id1` = 190000;
DELETE FROM `creature_template` WHERE `entry` = 190000;

-- Create NPC Template
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `modelid1`, `modelid2`, `modelid3`, `modelid4`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `scale`, `rank`, `dmgschool`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `trainer_type`, `trainer_spell`, `trainer_class`, `trainer_race`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `resistance1`, `resistance2`, `resistance3`, `resistance4`, `resistance5`, `resistance6`, `spell1`, `spell2`, `spell3`, `spell4`, `spell5`, `spell6`, `spell7`, `spell8`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `InhabitType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `DamageModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(190000, 0, 0, 0, 0, 0, 16925, 0, 0, 0, 'Arena Master', '1v1 Arena & Tournament Manager', 'Speak', 0, 80, 80, 2, 35, 1, 1, 1.14286, 1, 0, 0, 2000, 2000, 1, 1, 1, 768, 2048, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 3, 1, 10, 1, 1, 1, 1, 0, 0, 1, 0, 2, 'npc_1v1arena', 0);

-- Spawn NPC in major cities
-- Stormwind (Alliance) - Near Arena Battlemaster
INSERT INTO `creature` (`guid`, `id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`) VALUES
(800001, 190000, 0, 1519, 5148, 1, 1, 0, -8759.84, 877.65, 96.97, 0.63, 300, 0, 0, 8508, 0, 0, 0, 0, 0, '', 0);

-- Orgrimmar (Horde) - Near Arena Battlemaster  
INSERT INTO `creature` (`guid`, `id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`) VALUES
(800002, 190000, 1, 1637, 5170, 1, 1, 0, 1983.92, -4794.2, 56.7, 3.19, 300, 0, 0, 8508, 0, 0, 0, 0, 0, '', 0);

-- Dalaran (Neutral) - Arena area
INSERT INTO `creature` (`guid`, `id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`) VALUES
(800003, 190000, 571, 4395, 4560, 1, 1, 0, 5776.17, 721.3, 641.9, 0.63, 300, 0, 0, 8508, 0, 0, 0, 0, 0, '', 0);

-- Shattrath (Outland) - Lower City
INSERT INTO `creature` (`guid`, `id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`) VALUES
(800004, 190000, 530, 3703, 3688, 1, 1, 0, -1749.63, 5138.56, -39.87, 2.18, 300, 0, 0, 8508, 0, 0, 0, 0, 0, '', 0);

-- Optional: Ironforge (Alliance)
INSERT INTO `creature` (`guid`, `id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`) VALUES
(800005, 190000, 0, 1537, 1, 1, 1, 0, -4624.66, -926.87, 501.06, 2.29, 300, 0, 0, 8508, 0, 0, 0, 0, 0, '', 0);

-- Optional: Thunder Bluff (Horde)
INSERT INTO `creature` (`guid`, `id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`) VALUES
(800006, 190000, 1, 1638, 1638, 1, 1, 0, -1196.53, 29.84, 176.05, 4.62, 300, 0, 0, 8508, 0, 0, 0, 0, 0, '', 0);
