/*
 
Winamp Auto DJ

This plugin remembers songs played together in a database.
It uses this data to choose a good song to play after the 
last song in a playlist finishes.
 
*/
#include "stdafx.h"
#include "wa_ipc.h"
#include "ipc_pe.h"
#include <stdio.h>
#include <windows.h>
#include "WinampAutoDJ.h"
#include <wchar.h>
#include <winnls.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sqlite3.h>
#include <varargs.h>

#define WINAMP_BUTTON1 40044
#define WINAMP_BUTTON2 40045
#define WINAMP_BUTTON3 40046
#define WINAMP_BUTTON4 40047
#define WINAMP_BUTTON5 40048

#define DB_PATH "Winamp-AutoDJ.db"
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

void enqueue_file(wchar_t* file, int index);
wchar_t* find_new_song();
void remember_song_pairs(wchar_t* prev , wchar_t* curr);
void nuke_manually_overridden(wchar_t* prev, wchar_t* curr);
void non_stop();
void openDB();
int getSongID(wchar_t* filename);

sqlite3* db;

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

wchar_t previous_song[MAX_PATH-1] = L"";

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message==WM_WA_IPC && lParam==IPC_PLAYING_FILEW){
		//remember 
		//int pos=SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_GETLISTPOS);
		//wchar_t * current_song_ptr = (wchar_t *) SendMessage(plugin.hwndParent,WM_WA_IPC,pos,IPC_GETPLAYLISTFILEW);
		wchar_t * current_song =(wchar_t*) wParam;
		if (wcslen(previous_song) != 0)
			remember_song_pairs(previous_song,current_song);
		wcscpy_s(previous_song, current_song);


	} 
	else if (message==WM_WA_IPC && lParam==IPC_ISPLAYING){
		//add new file to playlist and play it
		int len=SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_GETLISTLENGTH);
		int pos=SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_GETLISTPOS);
		
		// better IPC_GET_NEXT_PLITEM  combined with IPC_GETLISTPOS
		if (SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_ISPLAYING) == 0 && len-1 == pos){
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
  openDB();
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
	sqlite3_close(db);
}


/*void debug(const wchar_t* format,...){
	va_list argptr;
	va_start(argptr, format);
	size_t maxsize = 1024 * sizeof(wchar_t);
	wchar_t* msg = (wchar_t*) malloc(maxsize);
	vswprintf(msg, maxsize, format, argptr);
	MessageBox(plugin.hwndParent, msg, L"debug", MB_OK);
	va_end(argptr);
	free(msg);
}*/

std::string CP_encode(const std::wstring &wstr, int CP ){
    int size_needed = WideCharToMultiByte(CP, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte (CP, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}
 
// This is an export function called by winamp which returns this plugin info.
// We wrap the code in 'extern "C"' to ensure the export isn't mangled if used in a CPP file.
extern "C" __declspec(dllexport) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
  return &plugin;
}

void openDB(){
	char *zErrMsg = 0;
	int rc;

	rc = sqlite3_open(DB_PATH, &db);

	if (rc){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		exit(0);
	}
	else{
		fprintf(stderr, "Opened database successfully\n");
	}

	char* create_table_songs =
		"CREATE TABLE songs " \
		"(id INTEGER PRIMARY KEY AUTOINCREMENT, " \
		"artist TEXT(50) NULL, " \
		"title TEXT(100) NULL, " \
		"trackno INTEGER NULL, " \
		"album TEXT(50)  NULL, " \
		"cover BLOB      NULL, " \
		"year INTEGER    NULL, " \
		"genre TEXT(30)  NULL, " \
		"filename TEXT(200) NOT NULL UNIQUE, " \
		"md5sum TEXT(32) NULL );";

	char* create_table_ratings = \
		"CREATE TABLE IF NOT EXISTS ratings("  \
		"SONG1_ID  INT  NOT NULL," \
		"SONG2_ID  INT  NOT NULL," \
		"RATING    INT  DEFAULT 0,"\
		"PRIMARY KEY (SONG1_ID, SONG2_ID)"\
		");";

	rc = sqlite3_exec(db, create_table_songs, NULL, 0, NULL);
	rc = sqlite3_exec(db, create_table_ratings, NULL, 0, NULL);
}

wchar_t* find_new_song(){ 
	//find most fitting song to previously played song
	//add to playlist

	int prev_id = getSongID(previous_song);
	
	//wchar_t msg[1024];
	//wsprintf(msg, L"Find new for ID: %d\n", prev_id);
	//MessageBox(plugin.hwndParent, msg, L"Find new", MB_OK);

	sqlite3_stmt* stmt;
	sqlite3_prepare_v2(db, "select max(rating) from ratings where song1_id = ?;", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, prev_id);

	int retval = sqlite3_step(stmt);
	int maxrating = 0;
	int next_id;

	if (retval == SQLITE_ROW){
		maxrating = sqlite3_column_int(stmt, 0);
	}
	if (maxrating != 0){
		sqlite3_prepare_v2(db, "select Song2_id from ratings where song1_id = ? and rating = ? ORDER BY Random() LIMIT 1;", -1, &stmt, NULL);
		sqlite3_bind_int(stmt, 1, prev_id);
		sqlite3_bind_int(stmt, 2, maxrating);
		sqlite3_step(stmt);
		next_id = sqlite3_column_int(stmt, 0);
	}
	else{
		//choose random song
		sqlite3_prepare_v2(db, "select id from songs ORDER BY Random() LIMIT 1;", -1, &stmt, NULL);
		sqlite3_step(stmt);
		next_id = sqlite3_column_int(stmt, 0);
	}
	//next_id should be available
	sqlite3_prepare_v2(db, "select filename from songs WHERE ID = ?;", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, next_id);
	sqlite3_step(stmt);

	wchar_t* song = (wchar_t*)sqlite3_column_text16(stmt, 0);
	
	//wsprintf(msg, L"Found: %ls\n", song);
	//MessageBox(plugin.hwndParent, msg, L"Found", MB_OK);
	//wchar_t* song = L"E:\\filz\\audio\\other\\02 Rattled by the Rush.mp3";
	return song;
}

void non_stop(){
	//MessageBox(plugin.hwndParent,L"non_stop()",L"Debug",MB_OK);
	//if playback stops after last playlist song
	int len=SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_GETLISTLENGTH);
	wchar_t* new_song = find_new_song();
	enqueue_file(new_song,len);
	
	//handle new file playing ungracefully
	//ToDo: shoot message to event loop for more gracefullness
	remember_song_pairs(previous_song, new_song);
	wcscpy_s(previous_song, new_song);
	// play
	SendMessage(plugin.hwndParent,WM_WA_IPC,len,IPC_SETPLAYLISTPOS);
	SendMessage(plugin.hwndParent,WM_COMMAND,MAKEWPARAM(WINAMP_BUTTON2,0),0);
	//SendMessage(plugin.hwndParent,WM_WA_IPC,0,IPC_STARTPLAY);
}

void enqueue_file(wchar_t* file, int index){
	//returns the playlist index of newly enqueued file


	/*COPYDATASTRUCT cds = {0};
	cds.dwData = IPC_PLAYFILE;
	cds.cbData = sizeof(wchar_t)*(wcslen(file) + 1);  // include space for null char
	//cds.lpData = malloc(cds.cbData);
	//memcpy(cds.lpData, file, cds.cbData);
	std::string buffer = CP_encode(file,CP_UTF8);
	TCHAR * buffer2 = (TCHAR*) malloc(cds.cbData);
	memcpy(buffer2, buffer.c_str(), (wcslen(file) + 1));
	//cds.lpData = (void*) &buffer.c_str();
	cds.lpData = (void*)buffer2;
	(void*)file;
	SendMessage(plugin.hwndParent,WM_COPYDATA,0,(LPARAM)&cds);*/
	fileinfoW fileinfo;
	wcscpy_s(fileinfo.file, MAX_PATH, file);
	HWND pe = (HWND)SendMessage(plugin.hwndParent, WM_WA_IPC, IPC_GETWND_PE, IPC_GETWND);
	//fileinfo.index = (int)SendMessage(pe, WM_WA_IPC, IPC_PE_GETCURINDEX, 0) + 1;
	fileinfo.index = index;
	COPYDATASTRUCT insert;
	insert.dwData = IPC_PE_INSERTFILENAMEW;
	insert.lpData = (void*)&fileinfo;
	insert.cbData = sizeof(fileinfoW);
	SendMessage(pe, WM_COPYDATA, 0, (LPARAM)&insert);
	
	//return index;
	/*wchar_t msg[1024];
	wsprintf(msg,L"Enqueued %ls at position (%d)\n",
		f.file, f.index
	  );
	std::ofstream myfile;
	myfile.open ("debug.txt");
	myfile << CP_encode(msg,CP_OEMCP);
	myfile.close();*/
}

void file2db(wchar_t* filename){
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "INSERT INTO songs (filename) values(?) ;", -1, &stmt, 0);
	sqlite3_bind_text16(stmt, 1, std::wstring(filename).c_str(), -1, SQLITE_TRANSIENT);
	//int rc = sqlite3_open(DB_PATH, &db);
	sqlite3_step(stmt);
}

int getSongID(wchar_t* filename){
	sqlite3_stmt* stmt;
	char* sql = "SELECT id FROM songs WHERE filename=?";
	int retval = sqlite3_prepare_v2( db, sql, -1, &stmt, NULL);
	sqlite3_bind_text16(stmt, 1, std::wstring(filename).c_str(), -1, SQLITE_TRANSIENT);
	retval = sqlite3_step(stmt);
	int id = -1;
	if (retval == SQLITE_ROW){
		id = (int)sqlite3_column_int(stmt, 0);
	}
	else{
		//ToDo: make clean
		file2db(filename);
		sqlite3_reset(stmt);
		sqlite3_bind_text16(stmt, 1, std::wstring(filename).c_str(), -1, SQLITE_TRANSIENT);
		retval = sqlite3_step(stmt);
		id = (int)sqlite3_column_int(stmt, 0);
	}
	//free(stmt);
	return id;
}

int get_rating(int prev_id, int curr_id){
	int rating = 0; //default

	sqlite3_stmt* stmt;
	sqlite3_prepare_v2(db,"SELECT rating FROM ratings WHERE SONG1_ID = ? and SONG2_ID = ? ;", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, prev_id);
	sqlite3_bind_int(stmt, 2, curr_id);
	int retval = 0;
	
	retval = sqlite3_step(stmt);

	if ( retval == SQLITE_ROW ){
		rating = sqlite3_column_int(stmt, 0);
		//debug(L"rating: %d\n", rating);
	}
	else 
	{
		sqlite3_prepare_v2(db, "INSERT INTO ratings(SONG1_ID, SONG2_ID) values(?, ?);", -1, &stmt, NULL);
		sqlite3_bind_int(stmt, 1, prev_id);
		sqlite3_bind_int(stmt, 2, curr_id);
		sqlite3_step(stmt);
	}
	return rating;
}

void remember_song_pairs(wchar_t* prev, wchar_t* curr){
	// if new song starts playing 
	// (IMPROVE: better not when song was set by non_stop() function )
	//   set songpair(previous - current) in DB : autodj_promote(current_value)
	
	//Write to SQLite
	int prev_id = getSongID(prev); 
	int curr_id = getSongID(curr); 
	int rating = get_rating(prev_id,curr_id); 
	if (wcscmp(prev, curr) != -10){
		//debug(L"Remembering: \nPREV : %ls\nCURR: %ls\nRating: %d\n", prev, curr, rating);
	}
	rating = autodj_promote(rating);
	sqlite3_stmt* stmt;
	sqlite3_prepare_v2(db, "UPDATE ratings SET RATING = ? WHERE SONG1_ID = ? AND SONG2_ID = ?;", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, rating);
	sqlite3_bind_int(stmt, 2, prev_id);
	sqlite3_bind_int(stmt, 3, curr_id);
	sqlite3_step(stmt);
	
}

void nuke_manually_overridden(wchar_t* prev, wchar_t* curr){
	// if song skipped or manually changed
	//   write songpair(previous - current) to DB: 0
}
