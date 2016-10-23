# teeworlds-fng2-mod
FNG mod for teeworlds, that advances the original FNG idea by golden spikes and other features

Compile:
Windows:
in the cmd:
cmd /k ""C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"" x86 

bam server_release

Note: Maybe change the Visual Studio Path to your installation / version. Please report, if there are any problems with older visual c++ compiler

Linux:
bam server_release

Note: tested with gcc5 under debian8, report, if older versions don't work


MORE THAN 16 SLOT SERVER:
change these values:
src/engine/shared/network.h - NET_MAX_CLIENTS to the value you want
src/engine/shared/protocol.h - MAX_CLIENTS to the value you want(must be the same as NET_MAX_CLIENTS!!)

AND DON'T FORGOT TO UPDATE YOUR .cfg -> sv_max_clients and your specator slot votes

start.sh is a automatic restart script for linux... start it as screen ./start.sh
