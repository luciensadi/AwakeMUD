/* 
  Contains information defining accounts.

  IN BRIEF: Accounts are OOC entities that represent players on a 1:1 basis (1 account per player).
  - Accounts collect syspoints and other OOC rewards that can be distributed between characters.
  - Idledelete operates at the account level instead of the character level, so if you have a high-TKE character your little guys stick around just as long.
      (NODELETE is still set on a per-character basis.)
  - Characters belonging to a given account can't multiplay with other characters from that account.
  - Staff can edit accounts or close/ban them if enforcement is needed.
  - Ignores are set at the account level unless specifically overridden at the character level.
  - STRETCH: Preferences are set at the account level and overridden at the character level.

  CREATING AN ACCOUNT:
  - Any character not already in an account can ACCOUNT CREATE <accountname>. If it already exists, denied, otherwise specify password.
  - On creation, account name matches your character's name. You can change it with ACCOUNT RENAME, but it's permanent once changed.

  JOINING AN ACCOUNT:
  - Any character that is not part of an account gets login nags to join an account.
  - ACCOUNT JOIN <accountname> asks for the account password, then merges your char and syspoints into it.

  WORKING WITH ACCOUNT:
  - ACCOUNT command lets you view status, list characters, etc.
  - Any account that doesn't have an email specified gets nagged on login to add it.
  - STRETCH: You can switch characters with the account command, like ACCOUNT LOGIN <name>. If that char is associated with your account, you're swapped over.

  DATA: Accounts have the following mutable data:
  - An ID number (immutable)
  - An account name (can only be changed if the account name matches a character in the account; can't be set to an existing character; blocks creation of characters with that name)
  - A unique email not used by any other account
  - A password
  - Syspoints
  - STRETCH: Preferences array that newly-created characters inherit, and that existing ones can default back to

  DB CHANGE:
  - Ignores include an ACCOUNT field, which characters translate into once the account is set for them.
*/

#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "accounts.hpp"

#define ACCOUNTS_NON_STAFF_USAGE_STRING "mort usage string here"
#define ACCOUNTS_STAFF_ONLY_USAGE_STRING "mort usage string here"
#define STAFF_LEVEL_REQUIRED_FOR_ELEVATED_ACTIONS LVL_CONSPIRATOR

ACMD(do_account) {
  char command[MAX_STRING_LENGTH] = { 0 };
  char mode[MAX_STRING_LENGTH] = { 0 };

  FAILURE_CASE(IS_NPC(ch), "You can't be projecting or puppeting while accessing your account.");
  FAILURE_CASE(!*arg, access_level(ch, STAFF_LEVEL_REQUIRED_FOR_ELEVATED_ACTIONS) ? ACCOUNTS_STAFF_ONLY_USAGE_STRING : ACCOUNTS_NON_STAFF_USAGE_STRING);

  // Split out what we have.
  char *remainder = any_one_arg(argument, command);

  // Ensure there's something populated in command.
  FAILURE_CASE(!*command, access_level(ch, STAFF_LEVEL_REQUIRED_FOR_ELEVATED_ACTIONS) ? ACCOUNTS_STAFF_ONLY_USAGE_STRING : ACCOUNTS_NON_STAFF_USAGE_STRING);
  
  // Everyone has access to these commands.
  // TODO: CREATE [confirm]: Makes a new account named after your character, then joins your character to it.
  if (is_abbrev(command, "create")) {
    // TODO: You cannot be in an account.
    // TODO: Confirmation with a notice that you can only have one account.
    // TODO: Must have CONFIRM.
    // TODO: Create an account named after your character.
    // TODO: Transfer email registration if it exists.
    // TODO: Save the newly-created account.
    // TODO: Message that you can use ACCOUNT RENAME once to change your account name.
    return;
  }

  // TODO: RENAME <newname> [confirm]: Changes your auto-generated account's name. Only usable once.
  if (is_abbrev(command, "rename")) {
    char *mode_remainder = any_one_arg(remainder, mode);
    // TODO: You must be in an account.
    // TODO: Your account name must match your character's name.
    // TODO: New name must pass obscenity filter.
    // TODO: Must have CONFIRM.
    // TODO: Rename account, save, and update any existing structs.
    return;
  }

  // TODO: JOIN <accountname>: asks for accountname's password, then joins you to it
  if (is_abbrev(command, "join")) {
    // TODO: You cannot be in an account.
    // TODO: remainder must contain a valid account name
    // TODO: Request input of account's password
    return;
  }

  // Staff-only commands below. Stop folks not of the right level from going further.
  FAILURE_CASE(!access_level(ch, STAFF_LEVEL_REQUIRED_FOR_ELEVATED_ACTIONS), ACCOUNTS_NON_STAFF_USAGE_STRING);

  // 
}

// TODO: Account creation OLC.