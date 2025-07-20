-- ============================================================================
-- Quick NPC Management Scripts
-- ============================================================================

-- ========================================
-- حذف کامل NPC ها
-- ========================================
DELETE FROM `creature` WHERE `id1` = 190000;
DELETE FROM `creature_template` WHERE `entry` = 190000;

-- ========================================
-- نصب سریع - فقط شهرهای اصلی
-- ========================================

-- Template
INSERT INTO `creature_template` (`entry`, `modelid1`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `scale`, `rank`, `unit_class`, `unit_flags`, `type`, `InhabitType`, `HealthModifier`, `ScriptName`) VALUES
(190000, 16925, 'Arena Master', '1v1 Arena & Tournament Manager', 80, 80, 35, 1, 1, 0, 1, 768, 7, 3, 10, 'npc_1v1arena');

-- Stormwind (Alliance)
INSERT INTO `creature` (`guid`, `id1`, `map`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`) VALUES
(800001, 190000, 0, -8759.84, 877.65, 96.97, 0.63, 300);

-- Orgrimmar (Horde)
INSERT INTO `creature` (`guid`, `id1`, `map`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`) VALUES
(800002, 190000, 1, 1983.92, -4794.2, 56.7, 3.19, 300);

-- Dalaran (Neutral)
INSERT INTO `creature` (`guid`, `id1`, `map`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`) VALUES
(800003, 190000, 571, 5776.17, 721.3, 641.9, 0.63, 300);

-- ========================================
-- دستورات مفید در بازی
-- ========================================

-- .reload creature_template
-- .npc add 190000
-- .go creature 800001
-- .npc delete
