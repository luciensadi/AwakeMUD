# Invoked on a regular basis (e.g. every 5 mins) to poll the MudVault API. Schema:
# GET https://mudvault.org/api/rewards/pending/<mud_id>
# Headers:
#   X-API-Key: mv_<mud_id>_<your_api_key>
# Response:
# {
#   "mud_id": 123,
#   "pending_rewards": [
#     {
#       "reward_id": "2df28ed3-c14e-407e-b6e5-da37514eb619",
#       "vote_id": "ed2718b6-30f0-4b26-9324-19013112f0c6",
#       "character_name": "Despair",
#       "reward_token": "de713c9befb04144910a541ed36a9dda",
#       "voted_at": "2025-10-02T09:55:53.813636+00:00",
#       "delivery_attempts": 0
#     }
#   ],
#   "count": 1,
#   "timestamp": "2025-10-02T10:54:56.656Z"
# }

import logging

from datetime import datetime
from uuid import UUID

import requests

from utils.database import add_reward_entry

logger = logging.getLogger(__name__)

# We don't bother with try/catch here, if something is so poorly formatted that it breaks then I want it to stop processing anyways.
def process_one_row(reward_dict: dict):
  formatted_reward_id = UUID(reward_dict["reward_id"])
  formatted_reward_token = UUID(reward_dict["reward_token"])
  formatted_voted_at = datetime.fromisoformat(reward_dict["voted_at"])
  formatted_vote_id = UUID(reward_dict["vote_id"])
  
  # Push it to the DB. Expect the MUD to actually apply it.
  add_reward_entry(formatted_reward_id, formatted_vote_id, reward_dict["character_name"], formatted_reward_token, formatted_voted_at)


def poll(x_api_key: str, mud_id: str):
  logger.info("Starting run of polling script.")
  
  COMPOSED_URL = f"https://mudvault.org/api/rewards/pending/{mud_id}"
  
  response = requests.get(
    url=COMPOSED_URL,
    headers={
      "X-API-Key": x_api_key
    },
    timeout=30
  )
  
  response.raise_for_status()
  
  required_keys = ["reward_id", "reward_token", "voted_at", "vote_id", "character_name"]
  for item in (items_to_process := response.json().get("pending_rewards", [])):
    logger.info(f"Processing entry: {item}")
    missing = [k for k in required_keys if k not in item]
    if missing:
      logger.error(f"Entry {item} was missing required keys: {missing}; skipping it.")
      continue
    try:
      process_one_row(item)
    except Exception:
      # One malformed entry shouldn't kill the rest of the batch.
      logger.exception(f"Failed to process entry {item}; skipping it.")

  logger.info(f"Successfully completed with {len(items_to_process)} {'items' if len(items_to_process) != 1 else 'item'} processed.")