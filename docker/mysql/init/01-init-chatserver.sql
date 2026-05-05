CREATE TABLE IF NOT EXISTS `User` (
    `id` INT PRIMARY KEY AUTO_INCREMENT,
    `name` VARCHAR(50) NOT NULL UNIQUE,
    `password` VARCHAR(50) NOT NULL,
    `state` ENUM('online', 'offline') DEFAULT 'offline'
);

CREATE TABLE IF NOT EXISTS `Friend` (
    `userid` INT NOT NULL,
    `friendid` INT NOT NULL,
    PRIMARY KEY (`userid`, `friendid`)
);

CREATE TABLE IF NOT EXISTS `AllGroup` (
    `id` INT PRIMARY KEY AUTO_INCREMENT,
    `groupname` VARCHAR(50) NOT NULL UNIQUE,
    `groupdesc` VARCHAR(200) DEFAULT ''
);

CREATE TABLE IF NOT EXISTS `GroupUser` (
    `groupid` INT NOT NULL,
    `userid` INT NOT NULL,
    `grouprole` ENUM('creator', 'normal') DEFAULT 'normal',
    PRIMARY KEY (`groupid`, `userid`)
);

CREATE TABLE IF NOT EXISTS `OfflineMessage` (
    `userid` INT NOT NULL,
    `message` VARCHAR(500) NOT NULL
);

CREATE TABLE IF NOT EXISTS `FriendRequest` (
    `requester_id` INT NOT NULL,
    `target_id` INT NOT NULL,
    `status` ENUM('pending', 'accepted', 'rejected') DEFAULT 'pending',
    `handled_at` DATETIME NULL,
    PRIMARY KEY (`requester_id`, `target_id`)
);

CREATE TABLE IF NOT EXISTS `GroupRequest` (
    `requester_id` INT NOT NULL,
    `group_id` INT NOT NULL,
    `status` ENUM('pending', 'accepted', 'rejected') DEFAULT 'pending',
    `handled_at` DATETIME NULL,
    PRIMARY KEY (`requester_id`, `group_id`)
);
