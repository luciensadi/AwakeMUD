import logging
import os

from .polling import poll
from .claim import claim
from .link import link_characters
from .unlink import unlink_characters

logger = logging.getLogger(__name__)

if __name__ == "__main__":
  logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")

  logger.info("Beginning run of MudVault monoscript.")

  MUDVAULT_MUD_ID = (os.getenv("MUDVAULT_MUD_ID") or "").strip()
  MUDVAULT_API_KEY = (os.getenv("MUDVAULT_API_KEY") or "").strip()

  if not all([MUDVAULT_MUD_ID, MUDVAULT_API_KEY]):
    raise EnvironmentError("Missing MUDVAULT environment variables.")

  x_api_key = f"mv_{MUDVAULT_MUD_ID}_{MUDVAULT_API_KEY}"

  try:
    unlink_characters(x_api_key)
  except Exception as e:
    logger.error(f"Got exception during unlink_characters(): {e!r}")

  try:
    link_characters(x_api_key)
  except Exception as e:
    logger.error(f"Got exception during link_characters(): {e!r}")

  try:
    claim(x_api_key)
  except Exception as e:
    logger.error(f"Got exception during claim(): {e!r}")

  try:
    poll(x_api_key, MUDVAULT_MUD_ID)
  except Exception as e:
    logger.error(f"Got exception during poll(): {e!r}")

  logger.info("MudVault monoscript run completed.")
