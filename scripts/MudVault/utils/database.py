import logging
import os
from datetime import datetime, timezone
from uuid import UUID

import mysql.connector
from mysql.connector import errorcode

logger = logging.getLogger(__name__)

# DB schema (mudvault_votes, mudvault_character_linking) is owned by the
# SQL/migrations agent; see SQL/Migrations/add_votes.sql for the definitions.


def _connect():
  return mysql.connector.connect(
    host=os.environ.get("MYSQL_HOST", "localhost"),
    user=os.environ["MYSQL_USER"],
    password=os.environ["MYSQL_PASSWORD"],
    database=os.environ.get("MYSQL_DATABASE", "AwakeMUD"),
    # No session time_zone pin: the whole stack (this Python side AND the C++
    # game) uses the MySQL server's default time zone. DATETIMEs in the
    # mudvault tables are stored as naive SERVER-LOCAL wall time, and
    # mv_boot() verifies the game host agrees with the DB server's clock.
    # If the DB ever moves to a host in a different time zone, mv_boot()
    # disables voting until the server's time zone/clock is aligned.
  )


# Returns a list of (idnum, character_name, verification_code) rows in the given status.
def _get_characters_with_linking_status(status: str) -> list[tuple[int, str, str]]:
  cnx = None
  cursor = None
  try:
    cnx = _connect()
    cursor = cnx.cursor()
    cursor.execute("SELECT idnum, character_name, verification_code FROM mudvault_character_linking WHERE linking_status = %s;", (status,))
    return list(cursor.fetchall())
  except Exception:
    logger.exception(f"_get_characters_with_linking_status({status}): Exception during SQL query.")
    return []
  finally:
    if cursor is not None:
      cursor.close()
    if cnx is not None:
      cnx.close()


# Returns a list of (idnum, character_name, verification_code) tuples to verify.
def get_characters_to_link() -> list[tuple[int, str, str]]:
  return _get_characters_with_linking_status("needs_linking")


# Returns a list of (idnum, character_name) tuples to unlink.
def get_characters_to_unlink() -> list[tuple[int, str]]:
  return [(idnum, name) for idnum, name, _ in _get_characters_with_linking_status("needs_unlinking")]


# Updates the linking status of a character, keyed by idnum.
def set_linking_status(idnum: int, status: str):
  cnx = None
  cursor = None
  try:
    cnx = _connect()
    cursor = cnx.cursor()
    cursor.execute("UPDATE mudvault_character_linking SET linking_status = %s WHERE idnum = %s;", (status, idnum))
    cnx.commit()
  except Exception:
    logger.exception(f"set_linking_status({idnum}, {status}): Exception during SQL query.")
  finally:
    if cursor is not None:
      cursor.close()
    if cnx is not None:
      cnx.close()


# Adds an entry. Returns True on insert; False on duplicate key (webhooks and
# polling both insert, so dedup happens on the reward_id primary key) or error.
def add_reward_entry(reward_id: UUID, vote_id: UUID, character_name: str, reward_token: UUID, voted_at: datetime) -> bool:
  # mysql-connector-python serializes tz-aware datetimes using their wall-clock
  # fields WITHOUT converting zones, and the C++ game reads voted_at back via
  # UNIX_TIMESTAMP(), which interprets it in the DB session zone. The
  # convention is naive SERVER-LOCAL wall time, so normalize every incoming
  # timestamp: treat a naive value as UTC (the MudVault API contract), then
  # convert to this host's local time zone and strip the tzinfo. This only
  # holds while the DB server's time zone matches this host's clock -- the
  # same assumption mv_boot() enforces on the game side.
  if voted_at is not None:
    if voted_at.tzinfo is None:
      voted_at = voted_at.replace(tzinfo=timezone.utc)
    voted_at = voted_at.astimezone().replace(tzinfo=None)

  cnx = None
  cursor = None
  try:
    cnx = _connect()
    cursor = cnx.cursor()
    cursor.execute(
      "INSERT INTO mudvault_votes (character_name, vote_id_bin, reward_id_bin, reward_token_bin, voted_at) VALUES (%s, %s, %s, %s, %s);",
      (character_name,
       vote_id.bytes,
       reward_id.bytes,
       reward_token.bytes,
       voted_at))
    cnx.commit()
    return True
  except mysql.connector.Error as e:
    if e.errno == errorcode.ER_DUP_ENTRY:
      logger.info(f"add_reward_entry: Reward {reward_id} already recorded; ignoring duplicate.")
      return False
    logger.exception("add_reward_entry: Exception during SQL query.")
    return False
  except Exception:
    logger.exception("add_reward_entry: Exception during SQL query.")
    return False
  finally:
    if cursor is not None:
      cursor.close()
    if cnx is not None:
      cnx.close()


# Returns a list of reward IDs (dashless, lowercase hex) that the MUD has delivered
# (redeemed_at set) but which have not yet been claimed with MudVault.
def get_rewards_to_claim() -> list[str]:
  cnx = None
  cursor = None
  try:
    cnx = _connect()
    cursor = cnx.cursor()
    cursor.execute("SELECT reward_id_hex FROM mudvault_votes WHERE redeemed_at IS NOT NULL AND claimed_at IS NULL;")
    return [reward_id_hex.replace("-", "").lower() for (reward_id_hex,) in cursor.fetchall()]
  except Exception:
    logger.exception("get_rewards_to_claim: Exception during SQL query.")
    return []
  finally:
    if cursor is not None:
      cursor.close()
    if cnx is not None:
      cnx.close()


# Companion to get_rewards_to_claim(), called when a reward is successfully claimed.
def mark_reward_as_claimed(reward_id: UUID):
  cnx = None
  cursor = None
  try:
    cnx = _connect()
    cursor = cnx.cursor()
    cursor.execute("UPDATE mudvault_votes SET claimed_at = NOW() WHERE reward_id_bin = %s;", (reward_id.bytes,))
    cnx.commit()
  except Exception:
    logger.exception("mark_reward_as_claimed: Exception during SQL query.")
  finally:
    if cursor is not None:
      cursor.close()
    if cnx is not None:
      cnx.close()
