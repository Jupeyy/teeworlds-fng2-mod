# teeworlds-fng2-mod
FNG mod for teeworlds, that advances the original FNG idea by golden spikes and other features

COMPILE:
Note: Under every OS except Windows, you might want to install the mysql client libraries

You need a valid C++11 compiler to compile this source code

Windows:
in the cmd:
cmd /k ""C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"" x86 

bam server_release

Note: Maybe change the Visual Studio Path to your installation / version. Please report, if there are any problems with older visual c++ compiler

You can also use debug.bat or release.bat to compile directly.

Linux:
bam server_release

Note: tested with gcc5 under debian8, report, if older versions don't work


MORE THAN 16 SLOT SERVER:
change these values:
src/engine/shared/network.h - NET_MAX_CLIENTS to the value you want
src/engine/shared/protocol.h - MAX_CLIENTS to the value you want(must be the same as NET_MAX_CLIENTS!!)

AND DON'T FORGOT TO UPDATE YOUR .cfg -> sv_max_clients and your specator slot votes

start.sh is a automatic restart script for linux... start it as screen ./start.sh. You maybe need to write chmod 700 ./start.sh first

What is new compared to openfng?
-Golden Spikes
-Support for upto 256 players
-New Score Display (sv_score_display) that calculates points based on all stats(deaths, hits etc.)
-Smooth Freeze Mode(sv_smooth_freeze_mode), to make being frozen more smooth(no input movement)
-Emotional tees(sv_emotional_tees, and sv_emote_wheel for ddnet client) to enable ddrace like eye emotions
-sv_highbandwith to enable high map download
-server commands: stats, whisper, emote etc.
-generally better DDNet support(like whisper command)
-no bans when leaving, while being frozen. The character does not getting killed and can be killed into spikes

--for modding possibilities, you can start more than 1 gameserver too
see the standard fng.cfg to see some features
