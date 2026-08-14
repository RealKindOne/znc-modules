# My Collection of ZNC Modules

These modules are built with **version 1.10.x**.

I have modified some of the default ZNC modules:

## Default Modules

- **autoattach**
  - Add SWAP command

- **autovoice**
  - Support multiple hostmasks like autoop.

- **cert**
  - Print certificate in SHA1, SHA256, and SHA512 formats.
  - Allow printing certs with and without `:`.

- **fail2ban**
  - Add columns for ID, Username, First Seen, Last Seen, Expires In.
  - Added command: `Unban <ID>`.

- **keepnick**
  - Support MONITOR (see notes at the top of the module source code).

- **lastseen**
  - Format output for clarity.
  - Show "now" for currently logged-in users.
  - Display IP address.
  - Update webadmin page (overwrite existing `data/lastseen/` folder).

- **log**
  - Uses older `#channel_YYYYMMDD.log` format.
  - No support for extended-join.

- **sasl**
  - Support user.pem file in sasl folder for SASL EXTERNAL authentication.
  - No interface; just place the file in the folder.

- **send_raw**
  - Allow non-admins to use `send_raw` only on themselves.
  - Allow admins to send fake data to ZNC.

- **watch**
  - Support exempt functionality (WARNING: Backup your `.registry` file before use).

## Custom Modules

- **antiidle**
  - Resurrected from git. Uses a channel you need to add manually.

- **autoaccept**
  - Automatically `/accept` known people who message you.

- **channelurl**
  - Hide channel URL numeric from clients.

- **clientrestrict**
  - Prevent IRC clients from sending specific commands based on Client ID.

- **disconnectonban**
  - Disconnect from network when banned.

- **forcehostusernames**
  - Force ZNC to send `CAP REQ :userhost-in-names`, which is normally hidden.

- **glined**
  - Put G-Line quits in a single window.

- **invite**
  - Allow people to send channel invites

- **klined**
  - Put K-Line quits in a single window

- **lowercasechan**
  - Lowercase the channel name to your IRC client for all channel events
  - This include channels from `/whois`

- **monitor**
  - Use MONITOR

- **myinsanity**
  - Counts users, channels, and networks you are on

- **notice**
  - Convert NOTICE from people on a list into PRIVMSG

- **partyline**
  - Resurrected from git.
  - Multi-network support. Join partyline channel on only one network instead of all
  - PRIVMSG's via /msg ?NICK are still sent to all networks

- **pretendclient**
  - Make znc disconnect/connect from irc networks like a real client

- **rawlog**
  - Raw log

- **roulette**
  - Game of roulette

- **shutdown**
  - Shutdown your ZNC remotely. Does not save config. Run at own risk.

- **stripinput**
  - Strip clientside input

- **striptopic**
  - Strip /topic message

- **zlined**
  - Put Z-Line quits in a single window