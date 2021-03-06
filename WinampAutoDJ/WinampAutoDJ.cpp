/*
 
Winamp Auto DJ

This plugin remembers songs played together in a database.
It uses this data to choose a good song to play after the 
last song in a playlist finishes.
 
*/
#include "stdafx.h"
#include "wa_ipc.h"
#include <stdio.h>
#include <windows.h>
#include "WinampAutoDJ.h"
#include <wchar.h>
#include <winnls.h>
#include <string>
#include <iostream>
#include <fstream>

//basic arithmetic operations for weighting
int autodj_promote(int value){
	//is used every time two songs are played consecutively
	//todo: check for min = 0 ,max = intmax
	return value+1;
}
int autodj_demote(int value){
	//is used every time when a song is skipped
	// try setting it straight to 0, or decreasing by different value
	//todo: check for min = 0 ,max = intmax
	return value-1;
}


// these are callback functions/events which will be called by Winamp
int  init(void);
void config(void);
void quit(void);

void enqueue_file(wchar_t* file);
wchar_t* find_new_song();
void remember_song_pairs(wchar_t* prev , wchar_t* curr);
void nuke_manually_overridden(wchar_t* prev, wchar_t* curr);
void non_stop();

// this structure contains plugin information, version, name...
// GPPHDR_VER is the version of the winampGeneralPurposePlugin (GPP) structure
winampGeneralPurposePlugin plugin = {
  GPPHDR_VER,  // version of the plugin, defined in "winampAutoDJ.h"
  PLUGIN_NAME, // name/title of the plugin, defined in "winampAutoDJ.h"
  init,        // function name which will be executed on init event
  config,      // function name which will be executed on config event
  quit,        // function name which will be executed on quit event
  0,           // handle to Winamp main window, loaded by winamp when this dll is loaded
  0            // hinstance to this dll, loaded by winamp when this dll is loaded
};
 

// Message catching
WNDPROC lpWndProcOld = NULL;

wchar_t previous_song[MAX_PATH-1];

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message==WM_WA_IPC && lParam==IPC_PLAYING_FILEW){
		//remember 
		//int pos=SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_GETLISTPOS);
		//wchar_t * current_song_ptr = (wchar_t *) SendMessage(plugin.hwndParent,WM_WA_IPC,pos,IPC_GETPLAYLISTFILEW);
		wchar_t * current_song =(wchar_t*) wParam;
		remember_song_pairs(previous_song,current_song);
		wcscpy_s(previous_song, current_song);


	} 
	else if (message==WM_WA_IPC && lParam==IPC_ISPLAYING){
		//add new file to playlist and play it
		//int len=SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_GETLISTLENGTH);
		//int pos=SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_GETLISTPOS);
		
		if (SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_ISPLAYING) == 0 ){
			non_stop();
			//MessageBox(plugin.hwndParent,msg,L"Winamp Lparam",MB_OK);
			
		}
	}
 	return CallWindowProc(lpWndProcOld,hwnd,message,wParam,lParam);
}

// event functions follow
 
int init() {
  //A basic messagebox that tells you the 'init' event has been triggered.
  //If everything works you should see this message when you start Winamp once your plugin has been installed.
  //You can change this later to do whatever you want (including nothing)
  //MessageBox(plugin.hwndParent, L"Init event triggered for winampAutoDJ, yay. Plugin installed successfully!", L"", MB_OK);
  lpWndProcOld = (WNDPROC)SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)MainWndProc);
  //todo: setup Database connection
  return 0;
}
 
void config() {
  //A basic messagebox that tells you the 'config' event has been triggered.
  //You can change this later to do whatever you want (including nothing)
 
	wchar_t msg[1024];
 
	int index = 0;
	int version = SendMessage(plugin.hwndParent,WM_WA_IPC,index,IPC_GETVERSION);
	int majVersion = WINAMP_VERSION_MAJOR(version);
	int minVersion = WINAMP_VERSION_MINOR(version);
 
	wsprintf(msg,L"The version of Winamp is %x.%x (%x)\n",
	  majVersion,
	  minVersion,
	  	 version
	  );
 
	MessageBox(plugin.hwndParent,msg,L"Winamp Version",MB_OK);
	//find_new_song();
}
 




void quit() {
  //A basic messagebox that tells you the 'quit' event has been triggered.
  //If everything works you should see this message when you quit Winamp once your plugin has been installed.
  //You can change this later to do whatever you want (including nothing)
  //MessageBox(0, L"Quit event triggered for gen_myplugin.", L"", MB_OK);
}


std::string utf8_encode(const std::wstring &wstr){
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}
 
// This is an export function called by winamp which returns this plugin info.
// We wrap the code in 'extern "C"' to ensure the export isn't mangled if used in a CPP file.
extern "C" __declspec(dllexport) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
  return &plugin;
}

wchar_t* find_new_song(){ 
	//find most fitting song to previously played song
	//add to playlist
	wchar_t* song = L"E:\\filz\\audio\\Rock\\F-Zero X - Guitar Arrange\\01 - The Long Distance Of Murder.mp3";
	return song;
}

void non_stop(){
	//MessageBox(plugin.hwndParent,L"non_stop()",L"Debug",MB_OK);
	//if playback stops after last playlist song
	int len=SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_GETLISTLENGTH);
	enqueue_file(find_new_song());
	//  play
	//SendMessage(plugin.hwndParent,WM_WA_IPC,len-1,IPC_SETPLAYLISTPOS);
	//SendMessage(plugin.hwndParent,WM_COMMAND,MAKEWPARAM(WINAMP_BUTTON2,0),0);
	SendMessage(plugin.hwndParent,WM_WA_IPC,len-1,IPC_STARTPLAY);
}

void enqueue_file(wchar_t* file){

	COPYDATASTRUCT cds = {0};
	cds.dwData = IPC_ENQUEUEFILE;
	cds.lpData = (void*)utf8_encode(file);
	cds.cbData = wcslen((wchar_t*)cds.lpData)+1;  // include space for null char
	SendMessage(plugin.hwndParent,WM_COPYDATA,0,(LPARAM)&cds);
	wchar_t msg[1024];
	wsprintf(msg,L"Enqueued %ls (%d)\n",
	  file,wcslen((wchar_t*)cds.lpData)+1
	  );
	std::ofstream myfile;
	myfile.open ("debug.txt");
	myfile << utf8_encode(msg);
	myfile.close();
}

void remember_song_pairs(wchar_t* prev, wchar_t* curr){
	// if new song starts playing 
	// (IMPROVE: better not when song was set by non_stop() function )
	//   set songpair(previous - current) in DB : autodj_promote(current_value)
	if (wcscmp(prev,curr)!=0){
		wchar_t msg[3*MAX_PATH];
		wsprintf(msg,L"Remembering: \nPREV : %ls\nCURR: %ls\n",prev,curr);
		MessageBox(plugin.hwndParent,msg,L"remember_song_pairs",MB_OK);
	}
}

void nuke_manually_overridden(wchar_t* prev, wchar_t* curr){
	// if song skipped or manually changed
	//   write songpair(previous - current) to DB: 0
}

