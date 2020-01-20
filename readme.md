# teeworlds-fng2-mod
FNG mod for teeworlds, that advances the original FNG idea by golden spikes and other features

COMPILE:
You need a valid C++11 compiler to compile this source code

Windows:
Use cmake-gui and choose the root directory of this project, then generate for the compiler you want to use

Linux:
in project root directory:
mkdir build
cd build
cmake ..
make -j16

MORE THAN 16 SLOT SERVER:
change these values:
src/engine/shared/protocol.h - MAX_PLAYERS to the value you want(must be maximal as much as MAX_CLIENTS!!)

AND DON'T FORGOT TO UPDATE YOUR .cfg -> sv_max_clients and your player slot votes

start.sh is a automatic restart script for linux... start it as screen ./start.sh(copy to your build directory). You maybe need to write chmod 700 ./start.sh first

What is new compared to openfng?
-Golden Spikes
-Support for upto 64 players
-New Score Display (sv_score_display) that calculates points based on all stats(deaths, hits etc.)
-Smooth Freeze Mode(sv_smooth_freeze_mode), to make being frozen more smooth(no input movement)
-Emotional tees(sv_emotional_tees) to enable ddrace like eye emotions
-server commands: stats, emote etc.
-no bans when leaving, while being frozen. The character does not getting killed and can be killed into spikes
