@ECHO OFF

rem debug, release, etc.
SET type=%1
rem full path to the compiled DLL
SET target=%2

::SET destination=d:\GAMES\Fallout2\@RP\ddraw.dll

SET pdb="%~dpn2.pdb"

rem Disabled for Sonora-KR local experiments: ducible can fail under restricted
rem desktop sessions even when the DLL was linked successfully.
rem IF EXIST ducible.exe (
rem     IF EXIST %pdb% (
rem         ducible %target% %pdb%
rem     ) ELSE (
rem         ducible %target%
rem     )
rem )

::echo Copying %target% to %destination% ...
::copy %target% %destination%
