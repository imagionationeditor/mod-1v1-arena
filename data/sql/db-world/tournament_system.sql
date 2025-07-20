-- 1v1 Arena Tournament System Tables

-- جدول تورنمنت‌ها
CREATE TABLE IF NOT EXISTS `arena_tournaments` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `name` VARCHAR(255) NOT NULL,
    `description` TEXT,
    `entry_fee` INT DEFAULT 500000, -- 50 gold default
    `registration_start` DATETIME NOT NULL,
    `registration_end` DATETIME NOT NULL,
    `tournament_start` DATETIME NULL,
    `tournament_end` DATETIME NULL,
    `status` ENUM('registration', 'ready', 'active', 'finished', 'cancelled') DEFAULT 'registration',
    `max_participants` INT DEFAULT 64,
    `current_participants` INT DEFAULT 0,
    `winner_reward_gold` INT DEFAULT 5000000, -- 500 gold
    `winner_reward_item` INT DEFAULT 0,
    `winner_title` INT DEFAULT 0,
    `created_by` INT NOT NULL,
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP,
    `bracket_type` ENUM('single_elimination', 'double_elimination') DEFAULT 'single_elimination'
);

-- جدول ثبت‌نام‌ها
CREATE TABLE IF NOT EXISTS `arena_tournament_registrations` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `tournament_id` INT NOT NULL,
    `player_guid` INT NOT NULL,
    `character_name` VARCHAR(255) NOT NULL,
    `registration_time` DATETIME DEFAULT CURRENT_TIMESTAMP,
    `entry_fee_paid` BOOLEAN DEFAULT FALSE,
    `status` ENUM('registered', 'confirmed', 'disqualified') DEFAULT 'registered',
    FOREIGN KEY (`tournament_id`) REFERENCES `arena_tournaments`(`id`) ON DELETE CASCADE,
    UNIQUE KEY `unique_registration` (`tournament_id`, `player_guid`)
);

-- جدول مراحل مسابقه
CREATE TABLE IF NOT EXISTS `arena_tournament_rounds` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `tournament_id` INT NOT NULL,
    `round_number` INT NOT NULL,
    `round_name` VARCHAR(100), -- Quarter Finals, Semi Finals, Finals
    `status` ENUM('pending', 'active', 'completed') DEFAULT 'pending',
    `start_time` DATETIME NULL,
    `end_time` DATETIME NULL,
    FOREIGN KEY (`tournament_id`) REFERENCES `arena_tournaments`(`id`) ON DELETE CASCADE
);

-- جدول مسابقات
CREATE TABLE IF NOT EXISTS `arena_tournament_matches` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `tournament_id` INT NOT NULL,
    `round_id` INT NOT NULL,
    `match_number` INT NOT NULL,
    `player1_guid` INT NOT NULL,
    `player2_guid` INT NOT NULL,
    `player1_name` VARCHAR(255) NOT NULL,
    `player2_name` VARCHAR(255) NOT NULL,
    `winner_guid` INT NULL,
    `status` ENUM('pending', 'active', 'completed', 'forfeit') DEFAULT 'pending',
    `match_start` DATETIME NULL,
    `match_end` DATETIME NULL,
    `join_attempts_player1` INT DEFAULT 0,
    `join_attempts_player2` INT DEFAULT 0,
    `battleground_id` INT NULL,
    FOREIGN KEY (`tournament_id`) REFERENCES `arena_tournaments`(`id`) ON DELETE CASCADE,
    FOREIGN KEY (`round_id`) REFERENCES `arena_tournament_rounds`(`id`) ON DELETE CASCADE
);

-- جدول تاریخچه تورنمنت‌ها
CREATE TABLE IF NOT EXISTS `arena_tournament_history` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `tournament_id` INT NOT NULL,
    `winner_guid` INT NOT NULL,
    `winner_name` VARCHAR(255) NOT NULL,
    `total_participants` INT NOT NULL,
    `tournament_duration_minutes` INT,
    `total_matches` INT,
    `rewards_given` TEXT, -- JSON format for detailed rewards
    `finished_at` DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (`tournament_id`) REFERENCES `arena_tournaments`(`id`)
);

-- جدول آمار بازیکنان
CREATE TABLE IF NOT EXISTS `arena_tournament_player_stats` (
    `player_guid` INT PRIMARY KEY,
    `tournaments_participated` INT DEFAULT 0,
    `tournaments_won` INT DEFAULT 0,
    `total_matches_played` INT DEFAULT 0,
    `total_matches_won` INT DEFAULT 0,
    `total_gold_earned` INT DEFAULT 0,
    `titles_earned` INT DEFAULT 0,
    `last_tournament_date` DATETIME NULL,
    `best_ranking` INT DEFAULT 0,
    `current_season_points` INT DEFAULT 0
);

-- اینسرت کردن تورنمنت نمونه
INSERT INTO `arena_tournaments` (`name`, `description`, `entry_fee`, `registration_start`, `registration_end`, `winner_reward_gold`, `created_by`) 
VALUES 
('Weekly Championship', 'هفتگی قهرمانی ارنای 1v1 - جایزه ویژه برای برندگان!', 500000, NOW(), DATE_ADD(NOW(), INTERVAL 2 DAY), 5000000, 1);
