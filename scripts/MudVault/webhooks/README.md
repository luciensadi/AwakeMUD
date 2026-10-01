1. Change permissions on mudvault.env to 600, then update it to have your actual MudVault API key
2. Change the templated values in mudvault_webhooks.service, then move it to systemd's directory (e.g. `/etc/systemd/system/mudvault-webhook.service`)
3. Run the following:

`sudo systemctl daemon-reload` (reload files)
`sudo systemctl start mudvault-webhook` (start this service)
`sudo systemctl enable mudvault-webhook` (enable it to run on boot)
`sudo systemctl status mudvault-webhook` (check status)

4. Update your proxy's config to allow the webhook path. An example Lighttpd proxy config is in webhooks.py.