# Database Generation
AwakeMUD requires MySQL to function. Once you have your MySQL installation set up, perform the following commands:

CREATE DATABASE AwakeMUD;

USE AwakeMUD;

CREATE USER 'awakemud'@'localhost' IDENTIFIED BY 'some unique password here';

GRANT ALL PRIVILEGES ON AwakeMUD.* TO 'awakemud'@'localhost';

After this, execute the commands found in the 'SQL Table Generation' file.

The help_category and help_topic files may be ignored, as their contents were merged into the table generation text file. Once it's certain that they're no longer needed, they will be removed from this repository and this message will be deleted.
