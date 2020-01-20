# teeworlds-fng2-mod
#### This is a FNG mod for teeworlds, that advances the original FNG idea by golden spikes and other features.

## COMPILE:
You need a valid C++11 compiler to compile this source code

### Windows:
Use `cmake-gui.exe` and choose the root directory of this project, then choose a build directory(e.g. rootdir/build), then generate the build files for the compiler you want to use.  
With Visual Studio you will get a .sln project file, just open it and compile the code.  
With MinGW you need to open a MSys shell and type `make -j16` in the build directory.  

### Linux:
Open a terminal/shell in the project root directory and type:  
`mkdir build`   
`cd build`   
`cmake ..`   
`make -j16`  

### MORE THAN 16 SLOT SERVER:  
Change these values:  
`src/engine/shared/protocol.h` - MAX_PLAYERS to the value you want(must be maximal as much as MAX_CLIENTS!!)

AND DON'T FORGOT TO UPDATE YOUR .cfg -> `sv_max_clients` and `sv_player_slots`, aswell as your player slot votes.

## Run:
### Windows:  
Shift rightclick your build directory and click on `Open Powershell here` or `Open CMD here`.  
Then copy fng.cfg from the root directory to the build directory and type in the Powershell/CMD:  
`.\fng2_srv -f fng.cfg`

### Linux:
`start.sh` is a automatic restart script for linux, simply open a terminal/shell and do the following steps:  
Copy `fng.cfg` to your build directory, then start it as `screen ./start.sh`(copy screen.sh to your build directory and type `chmod 700 ./start.sh`)  

## Configure:
Open `fng.cfg` it will explain most settings you can change.

## Feature overview
What is new compared to OpenFNG?
- Golden Spikes
- Support for upto 64 players
- New Score Display (sv_score_display) that calculates points based on all stats(deaths, hits etc.)
- Smooth Freeze Mode(sv_smooth_freeze_mode), to make being frozen more smooth(no input movement)
- Emotional tees(sv_emotional_tees) to enable ddrace like eye emotions
- Server commands: stats, emote etc.
- No bans when leaving, while being frozen. The frozen character is not getting kicked and can be killed into spikes
