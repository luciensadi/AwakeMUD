import socket, select, sys
import os
import threading
import time
import re
import asyncio, concurrent.futures

import discord
from dotenv import load_dotenv

NOTIFY_DISCORD_ON_CONNECTION_CHANGE = True

load_dotenv()
token = os.getenv('DISCORD_TOKEN')
mud_username = os.getenv('MUD_USERNAME')
mud_password = os.getenv('MUD_PASSWORD')

logged_in = False
getting_who = False

sanitization_replacements = [
    ("`", "'"),  # Prevent escaping the Discord comment block.
    ("discord.gg", "<discord>"),  # Prevent auto-previewing Discord URLs.
]

message_queues = {}

def strip_and_send(msg, channel):
    send_message_to_discord(msg.strip(), channel)

discord_channels = {
    "OOC": {
        "idnum": int(os.getenv('DISCORD_OOC_CHANNEL')),
        "print_to_mud": True
    },
    "newbie": {
        "idnum": int(os.getenv('DISCORD_NEWBIE_CHANNEL')),
        "print_to_mud": True
    },
    "misc": {
        "idnum": int(os.getenv('DISCORD_MISC_CHANNEL')),
        "print_to_mud": False
    },
    "channel-8": {
        "idnum": int(os.getenv('DISCORD_RADIO_CHANNEL_8')),
        "print_to_mud": False
    },
    "helplog": {
        "idnum": int(os.getenv('DISCORD_HELPLOG_CHANNEL')),
        "print_to_mud": False
    }
}

# The list of Discord channel objects.
channels = {}

# The telnet socket that both Discord and the client will communicate with.
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# The string the who text will be printed t.
who_text = ""

def send_message_to_discord(message, channel, skip_sanitization=False, block_comment=False):
    """
    Performs certain common transformations on the message (ex: strip(), sanitization, etc)

    :param message: A string representing the message to send to Discord.
    :param channel: The channel to send it to. Must match one of the message queues previously declared.
    :param block_comment: If true, wraps string in triple back-ticks to make it a block quote. Defaults to false.
    """

    assert channel in discord_channels.keys(), f"Channel {channel} is not recognized, must be one of {discord_channels.keys()}."

    sendable_message = message.strip()

    if not skip_sanitization:
        for item in sanitization_replacements:
            sendable_message = sendable_message.replace(item[0], item[1])

    if block_comment:
        sendable_message = f"```{sendable_message}```"

    print(f"Enqueueing message for Discord channel '{channel}': ({sendable_message})")
    message_queues[channel].put_nowait(sendable_message)


def send_message_to_mud(message):
    pass


def telnet(client, host, port):
    pool = concurrent.futures.ThreadPoolExecutor()
    channel = None
    global getting_who, logged_in, who_text, s

    s.settimeout(2)

    # connect to remote host
    s.connect((host, port))

    print('Connected to remote host')

    connlog_ignored_user_list = [
        mud_username.lower(),
        "aeternitas",
        "lucien"
    ]

    reconnect = False
    while 1:
        time.sleep(1)
        try:
            if reconnect:
                logged_in = False
                print("Reconnecting...")
                # connect to remote host
                try:
                    s.connect((host, port))
                except Exception as e:
                    print(f'Unable to connect: {e}')
                    s.close()
                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    continue
                reconnect = False

            socket_list = [sys.stdin, s]

            # Get the list sockets which are readable
            read_sockets, write_sockets, error_sockets = select.select(socket_list , [], [])
            who_text = ""
            for sock in read_sockets:
                # incoming message from remote server
                if sock == s:
                    data = sock.recv(4096)
                    if not data:
                        print('Connection closed')
                        if NOTIFY_DISCORD_ON_CONNECTION_CHANGE:
                          send_message_to_discord("Lost connection to the MUD.", "OOC", skip_sanitization=True)
                        reconnect = True
                        s.close()
                        break
                    else:
                        data = data.decode('utf-8', 'backslashreplace')
                        if data == "\xff\xf1":
                          continue

                        print(f"Received data: ({data})")
                        if getting_who:
                            print("Getting who in loop.")
                            who_text += data.strip()

                            if "chummers displayed" in data:
                                send_message_to_discord(f'{who_text[:-1]}.', "OOC", block_comment=True)
                                who_text = ""
                                getting_who = False

                        elif "(OOC)" in data:
                            # TODO: use str.splitlines() to break by individual lines
                            data = data.strip()
                            user = data[1:].split(']')[0]
                            if user.lower() != mud_username.lower():
                                start = data.find('"')+1
                                terminus = data.rfind('"')
                                send_text = f"**{user}**: {data[start:terminus]}"
                                send_message_to_discord(send_text, "OOC")

                        elif "|]newbie[|" in data:
                            data = data.strip()
                            splitout = data.split(' |]newbie[| ')
                            user = splitout[0]
                            if user.lower() != mud_username.lower():
                                newline = '\n'
                                send_text = f"**{user}**: {splitout[1].split(newline)[0]}"
                                send_message_to_discord(send_text, "newbie")

                        elif "[MISCLOG" in data and (" bug: " in data or " typo: " in data or " idea: " in data or " praise: " in data):
                            send_message_to_discord(data.strip(), "misc")

                        elif 1 == 0 and "[CONNLOG" in data:
                            data = data.strip()

                            if "has quit the game" in data:
                                status_line = "has quit the game"
                            elif "has lost" in data:
                                status_line = "has unexpectedly disconnected"
                            elif "has reconnected" in data or "has re-logged in" in data:
                                status_line = "has reconnected"

                            # [CONNLOG: [35500] (Paragorn) Paragorn has entered the game.]
                            elif "has entered the game" in data:
                                status_line = "has entered the game"
                            else:
                                # We don't care about whatever this message is.
                                print(f"Debug: Skipping connlog line '{data}' due to it not having useful info.")
                                continue

                            user = data.split(')')[0].split('(')[1].strip()
                            print(f"User from connlog: {user} (message: {data})")
                            if user.lower() in connlog_ignored_user_list:  # Skip users we don't want to announce.
                                print(f"Skipping user connlog for {user} since it's in our ignore list.")
                                continue

                            send_text = f"**{user}** {status_line}."
                            send_message_to_discord(send_text, "OOC")

                        elif "/[8 MHz, " in data and "]: " in data:
                            send_message_to_discord(data.strip(), "channel-8")

                        elif "[HELPLOG: [" in data:
                            send_message_to_discord(data.strip(), "helplog")

                        elif not logged_in and "your handle" in data:
                            print("Sending username to MUD.")
                            s.send(f'{mud_username}\n'.encode())

                        elif not logged_in and "Welcome back. Enter your password" in data:
                            print("Sending password to MUD.")
                            s.send(f'{mud_password}\n'.encode())
                            logged_in = True

                        elif "PRESS RETURN" in data:
                            print("Continuing through MOTD.")
                            s.send("\n".encode())

                        elif "Enter the game" in data:
                            print("Continuing through enter-the-game numerical menu.")
                            s.send("1\n".encode())
                            if NOTIFY_DISCORD_ON_CONNECTION_CHANGE:
                              send_message_to_discord("Connected to the MUD.", "OOC", skip_sanitization=True)
                else:
                    msg = sys.stdin.readline()
                    s.send(msg.encode())
        except IOError as e:
            print(f"Caught IOError: {e}")
            reconnect = True
            


class DiscordClient(discord.Client):
    async def on_member_update(self, before, after):
        if after.nick == "Lucien" or after.nick == "lucien":
            if after.id != "Salamantis#0627":
                try:
                    await after.edit(nick=after.id, reason="Nickname locked to prevent confusion.")
                except Exception:
                    pass

    async def on_message(self, message):
        """
        Reacts to a message posted in Discord.

        :param message: The message that was posted (a Discord-library object)
        """
        # print(f"Processing Discord message from {message.author}: {message.content}")
        global getting_who, who_text, s
        if message.author == client.user:
            # print("Skipping-- it's from us.")
            return

        channel_key = None
        for key, channel in discord_channels.items():
            if channel["idnum"] == message.channel.id:
                if channel["print_to_mud"]:
                    channel_key = key
                break
        if channel_key is None:
            return

        if "!who" in message.content:
            # print("Getting who list")
            await message.add_reaction('🤔')
            getting_who = True
            who_text = ""
            s.send("who\n".encode())
            return

        # Convert mentions.
        send_text = message.content.strip()
        for mentioned in message.mentions:
            print(f"Mentioned user {mentioned.mention} with display name {mentioned.display_name}")
            send_text = send_text.replace(mentioned.mention, f'{mentioned.display_name}')

        # Convert banned characters.
        send_text = re.sub(r'[^\w\d ~!@#$%&*()_+`\-=\[\]\\{}|;:\'\",./<>?]', '', send_text).strip()
        if len(send_text) <= 1:
            print("Nothing to send after stripping of banned characters!")
            return

        print(f"Sending to MUD {channel_key} channel: '{send_text}' from {message.author.display_name}")
        s.send(f'{channel_key} [{message.author.display_name}]: {send_text}\n'.encode())


    async def on_ready(self):
        """
        Called when the Discord client is connected and ready.
        """
        print(f'{client.user} has connected to Discord!')

        # Print out connection info for guilds.
        for guild in client.guilds:
            print(
                f'{client.user} is connected to the following guild:\n'
                f'{guild.name}(id: {guild.id})'
            )

        # Initialize our channel objects for sending messages.
        for channel_key, channel_dict in discord_channels.items():
            print(f"  {channel_key}")
            channels[channel_key] = client.get_channel(channel_dict["idnum"])
        print(f"Initialization of {len(discord_channels)} channels complete.")

        # Eternal iteration. Every second, check for messages in channel queues, and if they exist then send them.
        while True:
            await asyncio.sleep(1)
            message = None

            for channel, queue in message_queues.items():
                try:
                    message = queue.get_nowait()
                    print(f"Sending {channel} message to Discord: '{message}'")
                    await channels[channel].send(message)
                except asyncio.QueueEmpty:
                    pass


if __name__ == "__main__":
    print("Initializing message queues...")
    for key in discord_channels.keys():
        print(f"  {key}")
        message_queues[key] = asyncio.Queue()
    print(f"Initialization of {len(discord_channels)} queues complete.")

    client = DiscordClient()

    x = threading.Thread(target=telnet, args=(client, os.getenv('MUD_HOST'), int(os.getenv('MUD_PORT'))), daemon=True)
    x.start()

    client.run(token)
