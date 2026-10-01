# Receive webhooks. If you're putting this behind Lighttpd, use something like this:
# server.modules += ( "mod_proxy", "mod_extforward" )
#
# # Ensure lighttpd passes the client's IP to the backend
# extforward.forwarder = ( "[IP_ADDRESS]" => "trust" )
#
# $HTTP["url"] =~ "^/mudvault-webhook$" {
#     proxy.server = ( "" => ( (
#         "host" => "[IP_ADDRESS]",
#         "port" => 8001
#     ) ) )
#
#     # This ensures the X-Forwarded-For header is sent
#     proxy.header = ( "upgrade" => "enable" )
# }

# Run through Gunicorn like this (from the scripts/MudVault directory, so
# `utils` and `webhooks` resolve; the app object lives in this file):
# gunicorn -w 4 -b [IP_ADDRESS]:8001 webhooks.webhooks:app

import os
import hashlib
import hmac
import logging

from datetime import datetime
from uuid import UUID

from flask import Flask, request
from flask_limiter import Limiter
from flask_limiter.util import get_remote_address
from werkzeug.middleware.proxy_fix import ProxyFix

from utils.database import add_reward_entry

logger = logging.getLogger(__name__)

MUDVAULT_WEBHOOK_SECRET = os.environ.get('MUDVAULT_WEBHOOK_SECRET')
MUDVAULT_MUD_ID = int(os.environ.get('MUDVAULT_MUD_ID', '0') or 0)
if MUDVAULT_MUD_ID == 0:
  raise RuntimeError("MUDVAULT_MUD_ID is not set (or is 0); refusing to start. It must come from mudvault.env / the service environment.")

app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 1024
app.wsgi_app = ProxyFix(app.wsgi_app, x_for=1)

limiter = Limiter(app, key_func=get_remote_address)

def validate_signature(payload_bytes: bytes, header_signature: str) -> bool:
  # 1. Take the entire request body as a string
  # 2. Create an HMAC-SHA256 hash using your webhook_secret as the key
  # 3. Convert to hex and add "sha256=" prefix
  # 4. Compare with X-MudVault-Signature header
  # 5. If they don't match, someone's trying to fake a vote - ignore it!

  if not header_signature or not MUDVAULT_WEBHOOK_SECRET:
    return False

  # Use hmac.compare_digest to prevent timing attacks
  computed_hmac = hmac.new(
      MUDVAULT_WEBHOOK_SECRET.encode(),
      payload_bytes,
      digestmod=hashlib.sha256
  ).hexdigest()

  expected_signature = f"sha256={computed_hmac}"
  return hmac.compare_digest(expected_signature, header_signature)

# POST https://yourmud.com/mudvault-webhook
@app.route('/mudvault-webhook', methods=['POST'])
@limiter.limit("5 per second")
def mudvault_webhook():
  # Headers:
  #   X-MudVault-Signature: sha256=abc123...
  #   Content-Type: application/json

  signature = request.headers.get('X-MudVault-Signature')
  if not signature:
    logger.warning("Received request with missing signature! Is someone trying to spoof rewards?")
    return 'Missing signature', 401

  if not validate_signature(request.get_data(), signature):
    logger.warning("Received invalid signature! Is someone trying to spoof rewards?")
    return 'Invalid signature', 403

  # Body (all fields are required):
  # {
  #   "reward_id": "2df28ed3-c14e-407e-b6e5-da37514eb098",
  #   "vote_id": "ed2718b6-30f0-4b26-9324-19013112f0c6",
  #   "character_name": "Despair",
  #   "reward_token": "de713c9befb04144910a541ed43a8dda",
  #   "voted_at": "2025-10-02T09:55:53.813636+00:00",
  #   "mud_id": 17,
  #   "mud_name": "Your MUD Name"
  # }

  unformatted_data = request.get_json(silent=True)
  if not unformatted_data or not isinstance(unformatted_data, dict):
    logger.error("Got missing or invalid JSON POST body")
    return "Invalid body", 400

  required_fields = ["reward_id", "character_name", "reward_token", "voted_at", "vote_id", "mud_id"]
  if not all(k in unformatted_data for k in required_fields):
    logger.error(f"Schema invalid: got {[x for x in unformatted_data.keys()]}, wanted all of {required_fields}")
    return "Missing required fields", 400

  try:
    formatted_reward_id = UUID(unformatted_data["reward_id"])
    formatted_reward_token = UUID(unformatted_data["reward_token"])
    formatted_voted_at = datetime.fromisoformat(unformatted_data["voted_at"])
    formatted_vote_id = UUID(unformatted_data["vote_id"])

    formatted_mud_id = int(unformatted_data["mud_id"])
    if formatted_mud_id != MUDVAULT_MUD_ID:
      logger.error(f"Wrong MUD ID: Got {formatted_mud_id}, wanted {MUDVAULT_MUD_ID}")
      return "Wrong MUD ID", 400

  except Exception:
    # Log loudly, schema contract not satisfied.
    logger.exception("Webhook payload did not satisfy the schema contract.")
    return "Invalid format", 400

  try:
    add_reward_entry(formatted_reward_id, formatted_vote_id, unformatted_data["character_name"], formatted_reward_token, formatted_voted_at)
  except Exception:
    logger.exception("Unexpected error occurred while storing a reward.")
    return "Internal processing error", 500

  # Log it for routine auditing (never dump headers or raw bodies; they may contain secrets).
  logger.info(f"Accepted valid webhook for reward {formatted_reward_id} (character {unformatted_data['character_name']}).")
  return "Accepted for processing", 202


if __name__ == '__main__':
  # Always run with debug mode disabled.
  app.run(debug=False)
