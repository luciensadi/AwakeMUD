# Fetch from the database any characters waiting to be unlinked, then remove
# them at MudVault.
# DELETE https://mudvault.org/api/rewards/characters/unlink?character_name=Despair
# Headers:
#   X-API-Key: mv_<mud_id>_<your_api_key>
# Response:
# {
#   "success": true,
#   "message": "Character unlinked successfully",
#   "character_name": "Despair",
#   "was_verified": true
# }
# NOTE: character_name is CASE-SENSITIVE; always send exactly what the DB stores.

import logging
import urllib.parse

import requests

from utils.database import (
  get_characters_to_unlink,
  set_linking_status
)

logger = logging.getLogger(__name__)

UNLINK_URL = "https://mudvault.org/api/rewards/characters/unlink"


def unlink_characters(x_api_key: str):
  logger.info("Starting run of unlinker script.")

  # Each row is (idnum, character_name).
  for idnum, name in (items_to_process := get_characters_to_unlink()):
    try:
      response = requests.delete(
        url=f"{UNLINK_URL}?character_name={urllib.parse.quote(name)}",
        headers={
          "X-API-Key": x_api_key,
        },
        timeout=30
      )

      response.raise_for_status()

      set_linking_status(idnum, "unlinking_succeeded")
      logger.info(f"Successfully unlinked {name} (idnum {idnum}).")
    except Exception as e:
      # Leave the status untouched so a later run can retry.
      logger.error(f"Failed to unlink {name} (idnum {idnum}): {e!r}")

  logger.info(f"Successfully completed with {len(items_to_process)} {'items' if len(items_to_process) != 1 else 'item'} processed.")
