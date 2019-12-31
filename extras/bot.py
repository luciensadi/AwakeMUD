# bot.py
import socket, select, string, sys
import os
import threading
import time
import re
import asyncio, concurrent.futures

import discord
from dotenv import load_dotenv

load_dotenv()
token = os.getenv('DISCORD_TOKEN')
discord_channel = int(os.getenv('DISCORD_CHANNEL'))
mud_username = os.getenv('MUD_USERNAME')
mud_password = os.getenv('MUD_PASSWORD')
mud_host = os.getenv('MUD_HOST')
mud_port = int(os.getenv('MUD_PORT'))

logged_in = False
getting_who = False

sanitization_replacements = [
    ("`", "'"),  # Prevent escaping the Discord comment block.
    ("discord.gg", "<discord>"),  # Prevent auto-previewing Discord URLs.
]

# for passing messages from mud -> discord
message_queue = asyncio.Queue()

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

who_text = ""


def send_message_to_discord(message, skip_sanitization=False, block_comment=False):
    """
    Performs certain common transformations on the message (ex: strip(), sanitization, etc)

    :param message: A string representing the message to send to Discord.
    :param block_comment: If true, wraps string in triple back-ticks to make it a block quote. Defaults to false.
    """

    sendable_message = message.strip()

    if not skip_sanitization:
        for item in sanitization_replacements:
            sendable_message = sendable_message.replace(item[0], item[1])

    if block_comment:
        sendable_message = f"```{sendable_message}```"

    # print(f"Enqueueing message for Discord: ({sendable_message})")
    message_queue.put_nowait(sendable_message)

def send_message_to_mud(message):
    pass


def telnet(client):
    pool = concurrent.futures.ThreadPoolExecutor()
    channel = None
    global getting_who, logged_in, who_text, s
    s.settimeout(2)

    # connect to remote host
    s.connect((mud_host, mud_port))

    print('Connected to remote host')

    connlog_ignored_user_list = [
        mud_username.lower(),
        "testmort",
        "aeternitas"
    ]

    reconnect = False
    while 1:
        time.sleep(1)
        if reconnect:
            logged_in = False
            print("Reconnecting...")
            # connect to remote host
            try:
                s.connect((mud_host, mud_port))
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
                    send_message_to_discord("Lost connection to the MUD.",
                                            skip_sanitization=True)
                    reconnect = True
                    s.close()
                    break
                else:
                    data = data.decode('utf-8', 'backslashreplace')
                    print(f"Received data: ({data})")
                    if getting_who:
                        print("Getting who in loop.")
                        who_text += data.strip()

                        if "chummers displayed" in data:
                            send_message_to_discord(f'{who_text[:-1]}.', block_comment=True)
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
                            send_message_to_discord(send_text)

                    elif "[CONNLOG" in data:
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
                        send_message_to_discord(send_text)

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
                        send_message_to_discord("Connected to the MUD.", skip_sanitization=True)
            else:
                msg = sys.stdin.readline()
                s.send(msg.encode())


class DiscordClient(discord.Client):
    async def on_message(self, message):
        """
        Reacts to a message posted in Discord.

        :param message: The message that was posted (a Discord-library object)
        """
        # print(f"Processing Discord message from {message.author}: {message.content}")
        global getting_who, who_text
        if message.author == client.user:
            # print("Skipping-- it's from us.")
            return
        if message.channel.id != discord_channel:
            # print("Skipping-- it's not from our channel.")
            return
        if "!who" in message.content:
            # print("Getting who list")
            await message.add_reaction('🤔')
            getting_who = True
            who_text = ""
            s.send("who\n".encode())
            return

        # print("Preparing to send it to the MUD.")

        # Convert mentions.
        send_text = message.content.strip()
        for mentioned in message.mentions:
            # print(f"Mentioned user {mentioned.mention} with display name {mentioned.display_name}")
            send_text = send_text.replace(mentioned.mention, f'@{mentioned.display_name}')

        # Convert banned characters.
        send_text = re.sub(r'[^\w\d ~!@#$%&*()_+`\-=\[\]\\{}\|;:\'\",./<>?]', '', send_text).strip()
        if len(send_text) <= 1:
            # print("Nothing to send after stripping of banned characters!")
            return
        print(f"Sending to MUD: '{send_text}' from {message.author.display_name}")
        s.send(f'ooc ^m[^n{message.author.display_name}^m]^n: {send_text}\n'.encode())

    async def on_ready(self):
        """
        Called when the Discord client is connected and ready.
        """
        print(f'{client.user} has connected to Discord!')
        channel = client.get_channel(discord_channel)
        for guild in client.guilds:
            print(
                f'{client.user} is connected to the following guild:\n'
                f'{guild.name}(id: {guild.id})'
            )
        while True:
            await asyncio.sleep(1)
            try:
                message = message_queue.get_nowait()
            except asyncio.QueueEmpty:
                continue

            print(f"Sending message to Discord: '{message}'")
            await channel.send(message)


client = DiscordClient()

x = threading.Thread(target=telnet, args=(client,), daemon=True)
x.start()

client.run(token)

