# Fetch from the database any characters waiting to be linked, then attempt to
# verify them against MudVault.
# POST https://mudvault.org/api/rewards/verify/confirm
# Headers:
#   X-API-Key: mv_<mud_id>_<your_api_key>
#   Content-Type: application/json
# Body:
# {
#   "verification_code": "ABC123",
#   "character_name": "Despair"
# }
# Response: HTTP 200 on success, 400 on invalid/expired code.
# NOTE: character_name is CASE-SENSITIVE; always send exactly what the DB stores.

import logging
import requests

from utils.database import (
  get_characters_to_link,
  set_linking_status
)

logger = logging.getLogger(__name__)

VERIFY_CONFIRM_URL = "https://mudvault.org/api/rewards/verify/confirm"


def link_characters(x_api_key: str):
  logger.info("Starting run of linker script.")

  items_to_process = get_characters_to_link()

  # Each row is (idnum, character_name, verification_code).
  for idnum, name, code in items_to_process:
    try:
      response = requests.post(
        url=VERIFY_CONFIRM_URL,
        headers={
          "X-API-Key": x_api_key,
          "Content-Type": "application/json",
        },
        json={
          "verification_code": code,
          "character_name": name,
        },
        timeout=30
      )

      response.raise_for_status()

      set_linking_status(idnum, "linking_succeeded")
      logger.info(f"Successfully linked {name} (idnum {idnum}).")
    except requests.HTTPError as e:
      if e.response is not None and e.response.status_code == 400:
        # Invalid/expired code: the game will let the player retry with a new one.
        set_linking_status(idnum, "linking_failed")
        logger.warning(f"MudVault rejected the verification code for {name} (idnum {idnum}); marked linking_failed.")
      else:
        # Anything else (5xx, network blips): leave status alone so we retry later.
        logger.exception(f"HTTP error while linking {name} (idnum {idnum}); leaving status for retry. {e!r}")
    except Exception:
      # Leave the status untouched so a later run can retry.
      logger.exception(f"Unexpected error while linking {name} (idnum {idnum}); leaving status for retry.")

  logger.info(f"Successfully completed with {len(items_to_process)} {'items' if len(items_to_process) != 1 else 'item'} processed.")
