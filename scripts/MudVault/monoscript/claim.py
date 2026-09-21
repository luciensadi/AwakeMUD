# Claim a reward as delivered. Run this script on a regular basis.
# POST https://mudvault.org/api/rewards/claim/<reward_id>
# Headers:
#   X-API-Key: mv_<mud_id>_<your_api_key>
# Body: Empty or {}
# Response: HTTP 200 on success

import logging
from uuid import UUID

import requests

from utils.database import (
  get_rewards_to_claim,
  mark_reward_as_claimed
)

logger = logging.getLogger(__name__)

def claim(x_api_key: str):
  logger.info("Starting run of claiming script.")
  
  for reward_id in get_rewards_to_claim():
    try:
      COMPOSED_URL = f"https://mudvault.org/api/rewards/claim/{reward_id}"
      
      response = requests.post(
        url=COMPOSED_URL,
        headers={
          "X-API-Key": x_api_key
        },
        timeout=30
      )
      
      response.raise_for_status()
      
      # get_rewards_to_claim() returns dashless hex strings; convert back to a
      # UUID so the DB layer can key the UPDATE on the binary column.
      mark_reward_as_claimed(UUID(reward_id))
    except Exception as e:
      logger.warning(f"Encountered error while processing reward {reward_id}, skipping. {e!r}")