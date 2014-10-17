Winamp-AutoDJ
=============

Simplification of https://github.com/Yamavu/AutoDJ as a Winamp Plugin. Also this is my first time really touching C++, please bear with me.

This plugin is (almost) config-free. It's installed as a generic plugin that listens to Winamp events and writes played songs to a SQLite database. It tries to find a fitting song to the most recently played song when no more entries are in the playlist.

This Winamp plugin uses:
* Winamp 5.66 (Build 3512) x86
* Winamp 5.55 SDK 
* SQLite 3.8.4.2 
