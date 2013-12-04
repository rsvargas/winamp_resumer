/*
 * WinAmp resumer plug-in
 *
 * Based on general plug-in framework by Justin Frankel/Nullsoft.
 *
 * Author:  Eddie Mansu
 *
 * The source can freely be modified, reused & redistributed for non-
 * profitable uses. Use for commercial purposes prohibited.
 *
 * CHANGELOG:
 * v1.1:
 *   - this version has been modified so that it no longer resumes unless 
 *     the song it's about to resume is the same one that was playing when 
 *     it  last saved information.
 *
 * v1.1b: 
 *   - Fixed a buffer bug.
 *
 * v1.2:
 *   - Fixed a bug and added a feature allowing songs to be resumed from 
 *     the beginning instead of where they left off.
 *
 * v1.3 (2013/12/04):
 *   - Support for newer windows and winamp versions (corrected the ini file path);
 *
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include "wa_ipc.h"
#include "gen.h"
#include "resource.h"
#include "../gen_tray/WINAMPCMD.H"
#include <math.h>


/* General defines */
#define     INI_BUFSIZE                 256						/* buffer */
#define     LOG_BUFSIZE                 INI_BUFSIZE             /* log file name buffer size */
#define     SONG_NAME_BUF               1024					/* Max length of a song name to retrieve */
#define     INI_SECTNAME                "ResumerPlugin"			/* duh */
#define     GEN_BUFSIZE                 INI_BUFSIZE				/* another buffer size */
#define     NUM_EQS                     10						/* number of EQ settings */
#define     EQSIZE                      (2*NUM_EQS)+NUM_EQS		/* max size of EQ setting string */
#define     WM_BUTTON_PLAY              WINAMP_BUTTON2			/* PLAY seems more logical */
#define     PREAMP                      10						/* PreAmp id for IPC_[GS]ETEQDATA */
#define     EQENABLED                   11						/* EqEnabled id for IPC_[GS]ETEQDATA */
#define     TIMER_ID                    3733					/* The ID of our timer */

/* Defaults */
#define		DEFAULT_SAVEONEXIT			0
#define		DEFAULT_RESUME				1
#define		DEFAULT_SAVEWHILEPLAYING	1
#define		DEFAULT_FORCEFLUSH			0
#define		DEFAULT_RESUMEATBEGINNING	0

/* INI key names */
#define		KEY_RESUME					"Resume"
#define		KEY_SAVEONEXIT				"SaveOnExit"
#define		KEY_SAVEEVERY				"SaveEvery"
#define		KEY_PLAYLISTLOC				"PlaylistLocation"
#define		KEY_SONGLOC					"SongLocation"
#define		KEY_EQ						"Eq"
#define		KEY_EQPREAMP				"EqPreamp"
#define		KEY_EQENABLED				"EqEnabled"
#define		KEY_SAVEWHILEPLAYING		"SaveOnlyWhilePlaying"
#define		KEY_FORCEFLUSH				"ForceFlush"
#define		KEY_SONG_NAME				"SongName"
#define		KEY_RESUMEATBEGINNING		"ResumeAtBeginning"

/* avoid stupid CRT silliness */
BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}


/* Function prototypes */
BOOL CALLBACK ConfigProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
int init();
void config();
void quit();
void CALLBACK TimerProc(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD uTime);
char *get_winamp_ini_path(char *dirbuff,const int size);
char *get_log_path(char *dirbuff, const int size);
void WritePrivateProfileInt(const char *sect,const char *varname,const int value,const char *inifile);
void save_state();
int num_digits(int number);
int get_num(char *string, int which_num, char delim);
void do_timer();

/* Setup the module information for WinAmp */
winampGeneralPurposePlugin module = {
	GPPHDR_VER,
	"Resumer Plug-in v1.3",
	init,
	config,
	quit,
	NULL,
	NULL
};

/* This is global because it makes things much cleaner.  It's constant once
it gets set in init(), then other functions use it. */
static char ini_path[INI_BUFSIZE];
static char log_path[LOG_BUFSIZE];

/* Here's what we export so WinAmp knows how to talk to us */
__declspec ( dllexport ) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
	return &module;
}

void mylog( const char* fmt, ... )
{
#ifdef _DEBUG
    FILE* fp = fopen( log_path, "a+" );
    if( fp )
    {
        va_list args;
        va_start( args, fmt );
        vfprintf( fp, fmt, args );
        va_end( args );
        fputs( "\n", fp );
    }
    fclose(fp);
#endif
}

/* Initialization routine
Resume all the saved settings (if configured to do so), and setup the
timer so we know how often to save state. */
int init()
{
	int loc_in_playlist = 0;
	int loc_in_song = 0;
	int ii = 0;
    int result = 0;
	char eq_string[EQSIZE];
	int ini_song_name_length = 0;
	char *song_filename = NULL;
	char *song_title = NULL;
	int song_name_len = 0;
	char *str_to_check = NULL;
	char song_name[SONG_NAME_BUF];

	/* Initialize our global ini_path and log_path*/
	get_winamp_ini_path(ini_path, INI_BUFSIZE);
    get_log_path(log_path, LOG_BUFSIZE );

	/* If we're supposed to, resume playing with saved settings. */

	if (GetPrivateProfileInt(INI_SECTNAME, KEY_RESUME, 0, ini_path)) {
		/* Resume requested */

		/* Get the following information from the last saved state:
		location in playlist, time in song, EQ settings */
		
		/* Get location in playlist */
		loc_in_playlist = GetPrivateProfileInt(INI_SECTNAME, KEY_PLAYLISTLOC, 0, ini_path);

		/* Get location in song */
		loc_in_song = GetPrivateProfileInt(INI_SECTNAME, KEY_SONGLOC, 0, ini_path);

		/* Get EQ settings from INI and set them up in WinAmp */

		GetPrivateProfileString(INI_SECTNAME, KEY_EQ, "0,0,0,0,0,0,0,0,0,0", eq_string, EQSIZE, ini_path);

		for ( ii = 0 ; ii < NUM_EQS ; ii++ ) {
			/* First request info so WinAmp knows what to modify */
			SendMessage(module.hwndParent, WM_WA_IPC, ii, IPC_GETEQDATA);
			/* Now assign the new data */
			SendMessage(module.hwndParent, WM_WA_IPC, get_num(eq_string, ii+1, ','), IPC_SETEQDATA);
		}

		/* Set the Preamp */
		SendMessage(module.hwndParent, WM_WA_IPC, PREAMP, IPC_GETEQDATA);
		SendMessage(module.hwndParent, WM_WA_IPC, GetPrivateProfileInt(INI_SECTNAME, KEY_EQPREAMP, 0, ini_path), IPC_SETEQDATA);
		
		/* Set the enabled status of the EQ */
		SendMessage(module.hwndParent, WM_WA_IPC, EQENABLED, IPC_GETEQDATA);
		SendMessage(module.hwndParent, WM_WA_IPC, GetPrivateProfileInt(INI_SECTNAME, KEY_EQENABLED, 0, ini_path), IPC_SETEQDATA);

		/* Attempt to determine if we're playing the same song as we were when we exited */
		/* i.e., the user didn't exit WinAmp in the middle of a song, and then load a new song by double-clicking it or */
		/* something.  That would cause the plug-in to resume that song in the same position as the first one left */
		/* off. */
		/* This addition recommended by a few of my friends, and Dave Glaeser. */


		ini_song_name_length = GetPrivateProfileString(INI_SECTNAME, KEY_SONG_NAME, "", song_name, SONG_NAME_BUF, ini_path);

		song_filename = strdup((char*)SendMessage(module.hwndParent, WM_WA_IPC, loc_in_playlist, IPC_GETPLAYLISTFILE));
		song_title = strdup((char*)SendMessage(module.hwndParent, WM_WA_IPC, loc_in_playlist, IPC_GETPLAYLISTTITLE));

		if ( (song_filename != NULL) && (song_title != NULL) ) {
			song_name_len = strlen(song_title) + strlen(song_filename) + 1;
			str_to_check = (char*)malloc(sizeof(char)*(song_name_len+1));
			_snprintf(str_to_check, SONG_NAME_BUF, "%s-%s", song_filename, song_title);
		}
		else
			str_to_check = NULL;
		
        if ( (ini_song_name_length == 0) || /* Nothing returned, the user didn't have this feature before. */
            ((str_to_check != NULL) &&
            (strcmp(str_to_check, song_name) == 0)) ) /* They match */
        {
            /* Jump to loc_in_playlist */
            result = SendMessage(module.hwndParent, WM_WA_IPC, loc_in_playlist, IPC_SETPLAYLISTPOS);
            mylog( "SetPLaylistPos=%d, res=%d", loc_in_playlist, result );

            /* Start playing selected track */
            
            //result = SendMessage(module.hwndParent, WM_WA_IPC, 0, IPC_STARTPLAY);
            result = SendMessage(module.hwndParent,WM_COMMAND,MAKEWPARAM(WM_BUTTON_PLAY,0),0);
            mylog( "StartPlay, res=%u ", result);

            /* Jump to saved location within track */
            result = SendMessage(module.hwndParent, WM_WA_IPC, loc_in_song, IPC_JUMPTOTIME);
            mylog( "JumpToTime=%dms, res=%u ",  loc_in_song, result);


        }

		if (song_filename != NULL) 
			free(song_filename);
		if (song_title != NULL) 
			free(song_title);
		if (str_to_check != NULL) 
			free(str_to_check);
	}

	/* Fire off an alarm clock to let us know when we need to write out the state of WinAmp */
	
	/* Setup the timer */
	do_timer();

	return 0;
}

/* This is called by WinAmp. */
void config()
{
	DialogBox(module.hDllInstance,MAKEINTRESOURCE(IDD_DIALOG1),module.hwndParent, ConfigProc);
}


/* Saves state if necessary, kills the timer. This is called by WinAmp. */
void quit()
{
	/* Kill the timer */
	KillTimer(module.hwndParent, TIMER_ID);

#if 0
	/* This feature is currently not implemented. */
	/* Should we save on exit? */
	if ( GetPrivateProfileInt(INI_SECTNAME, KEY_SAVEONEXIT, 0, ini_path) ) {
		save_state();
	}
#endif
}

/* This function is called every X seconds to save the current state of WinAmp
by the timer initialized in init() */
void CALLBACK TimerProc(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD uTime)
{
	save_state();
}

/* The config routine */
BOOL CALLBACK ConfigProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	char tempStr[GEN_BUFSIZE];

	switch (uMsg) {
		case WM_COMMAND :
			switch (LOWORD(wParam)) {
				case IDCANCEL : 
					EndDialog(hwndDlg, 0);
					break;
				case IDOK :
					/* Save the user preferences */
					GetDlgItemText(hwndDlg, IDC_EDIT_SAVENUMSECS, tempStr, GEN_BUFSIZE);
					WritePrivateProfileString(INI_SECTNAME, KEY_SAVEEVERY, tempStr, ini_path);
					/*WritePrivateProfileString(INI_SECTNAME, KEY_SAVEONEXIT, (IsDlgButtonChecked(hwndDlg, IDC_CHECK_SAVEONEXIT) ? "1" : "0"), ini_path);*/
					WritePrivateProfileString(INI_SECTNAME, KEY_RESUME, (IsDlgButtonChecked(hwndDlg, IDC_CHECK_RESUMEONSTART) ? "1" : "0"), ini_path);
					WritePrivateProfileString(INI_SECTNAME, KEY_SAVEWHILEPLAYING, (IsDlgButtonChecked(hwndDlg, IDC_CHECK_SAVEWHILEPLAYING) ? "1" : "0"), ini_path);
					WritePrivateProfileString(INI_SECTNAME, KEY_FORCEFLUSH, (IsDlgButtonChecked(hwndDlg, IDC_CHECK_FORCEFLUSH) ? "1" : "0"), ini_path);
					WritePrivateProfileString(INI_SECTNAME, KEY_RESUMEATBEGINNING, (IsDlgButtonChecked(hwndDlg, IDC_CHECK_RESUMEATBEGINNING) ? "1" : "0"), ini_path);
					/* Setup the timer if necessary */
					do_timer();
					EndDialog(hwndDlg, 0);
					break;
			}
			break;
		case WM_INITDIALOG :
			/* Load the saved configuration settings */
			GetPrivateProfileString(INI_SECTNAME, KEY_SAVEEVERY, 0, tempStr, GEN_BUFSIZE, ini_path);
			SetDlgItemText(hwndDlg, IDC_EDIT_SAVENUMSECS, tempStr);
			/*CheckDlgButton(hwndDlg, IDC_CHECK_SAVEONEXIT, (GetPrivateProfileInt(INI_SECTNAME, KEY_SAVEONEXIT, DEFAULT_SAVEONEXIT, ini_path)==1 ? BST_CHECKED : BST_UNCHECKED));*/
			CheckDlgButton(hwndDlg, IDC_CHECK_RESUMEONSTART, (GetPrivateProfileInt(INI_SECTNAME, KEY_RESUME, DEFAULT_RESUME, ini_path)==1 ? BST_CHECKED : BST_UNCHECKED));
			CheckDlgButton(hwndDlg, IDC_CHECK_SAVEWHILEPLAYING, (GetPrivateProfileInt(INI_SECTNAME, KEY_SAVEWHILEPLAYING, DEFAULT_SAVEWHILEPLAYING, ini_path) ? BST_CHECKED : BST_UNCHECKED));
			CheckDlgButton(hwndDlg, IDC_CHECK_FORCEFLUSH, (GetPrivateProfileInt(INI_SECTNAME, KEY_FORCEFLUSH, DEFAULT_FORCEFLUSH, ini_path)==1 ? BST_CHECKED : BST_UNCHECKED));
			CheckDlgButton(hwndDlg, IDC_CHECK_RESUMEATBEGINNING, (GetPrivateProfileInt(INI_SECTNAME, KEY_RESUMEATBEGINNING, DEFAULT_RESUMEATBEGINNING, ini_path)==1 ? BST_CHECKED : BST_UNCHECKED));
			break;
	}
	return FALSE;
}

/* Save the state of WinAmp */
void save_state()
{
	int ii;
	int eq;
	char eq_string[EQSIZE];
	int string_pos = 0;
	int list_pos;
	char *song_title;
	char *song_filename;
	char *str_to_write;
	char *tmpstr;
	int str_to_write_len;

	/* Only save if configured to do so currently */
	if ( SendMessage(module.hwndParent, WM_WA_IPC, 0, IPC_ISPLAYING) != 1 ) {
		/* WinAmp is not playing */
		/* Exit this procedure if we're not supposed to save while not playing */
		if ( GetPrivateProfileInt(INI_SECTNAME, KEY_SAVEWHILEPLAYING, DEFAULT_SAVEWHILEPLAYING, ini_path) == 1 )
			/* We're not supposed to save while not playing */
			return;
	}

	/* Write out the playlist */
	SendMessage(module.hwndParent, WM_WA_IPC, 0, IPC_WRITEPLAYLIST);

	/* Write out the current playlist position */
	list_pos = SendMessage(module.hwndParent, WM_WA_IPC, 0, IPC_GETLISTPOS);
	WritePrivateProfileInt(INI_SECTNAME, KEY_PLAYLISTLOC, list_pos, ini_path);

	/* Write out the position in the current song */
	/* If we're only supposed to resume from the beginning, always write 0. */
	if ( GetPrivateProfileInt(INI_SECTNAME, KEY_RESUMEATBEGINNING, DEFAULT_RESUMEATBEGINNING, ini_path) == 1 )
		WritePrivateProfileInt(INI_SECTNAME, KEY_SONGLOC, 0, ini_path);
	else
		WritePrivateProfileInt(INI_SECTNAME, KEY_SONGLOC, SendMessage(module.hwndParent, WM_WA_IPC, 0, IPC_GETOUTPUTTIME), ini_path);

	/* Write out the current equalizer settings */

	for ( ii = 0 ; ii < NUM_EQS ; ii++ ) {
		eq = SendMessage(module.hwndParent, WM_WA_IPC, ii, IPC_GETEQDATA);
		if (ii == 0) 
			sprintf(eq_string+string_pos, "%d", eq);
		else
			sprintf(eq_string+string_pos, ",%d", eq);
		/* Add to the length of the string the number of digits + the comma */
		string_pos += num_digits(eq) + (ii!=0);
	}

	/* Write out the EQ settings */
	WritePrivateProfileString(INI_SECTNAME, KEY_EQ, eq_string, ini_path);
	WritePrivateProfileInt(INI_SECTNAME, KEY_EQPREAMP, SendMessage(module.hwndParent, WM_WA_IPC, PREAMP, IPC_GETEQDATA), ini_path);
	WritePrivateProfileInt(INI_SECTNAME, KEY_EQENABLED, SendMessage(module.hwndParent, WM_WA_IPC, EQENABLED, IPC_GETEQDATA), ini_path);

	/* Write out the song name */
	tmpstr = (char*)SendMessage(module.hwndParent, WM_WA_IPC, list_pos, IPC_GETPLAYLISTFILE);
	if ( !tmpstr ) {
		/* Can't get filename, don't save state.  This is most likely because the playlist has been cleared. */
		WritePrivateProfileInt(INI_SECTNAME, KEY_SONGLOC, 0, ini_path);
		return;
	}
	else
		song_filename = strdup(tmpstr);

	tmpstr = (char*)SendMessage(module.hwndParent, WM_WA_IPC, list_pos, IPC_GETPLAYLISTTITLE);
	if ( !tmpstr ) {
		/* Again, playlist has probably be cleared.  Don't save state. */
		WritePrivateProfileInt(INI_SECTNAME, KEY_SONGLOC, 0, ini_path);		
		return;
	}
	else
		song_title = strdup(tmpstr);

	str_to_write_len = strlen(song_title) + strlen(song_filename) + 1;

	str_to_write = (char*)malloc(sizeof(char)*(str_to_write_len+1));
	_snprintf(str_to_write, SONG_NAME_BUF, "%s-%s", song_filename, song_title);
	
	WritePrivateProfileString(INI_SECTNAME, KEY_SONG_NAME, str_to_write, ini_path);
	free(song_filename);
	free(song_title);
	free(str_to_write);

	/* Force disk cache flush if requested */
	if ( GetPrivateProfileInt(INI_SECTNAME, KEY_FORCEFLUSH, DEFAULT_FORCEFLUSH, ini_path) ) 
		/* Note:  this feature only works because I'm linking to commode.obj! */
		_flushall();
}


/* General-purpose routines */

/* Get the path of the winamp.ini file */
char *get_winamp_ini_path(char *dirbuff,const int size)
{
    if(SendMessage(module.hwndParent,WM_WA_IPC,0,IPC_GETVERSION) >= 0x2900)
    {
        // this gets the string of the full ini file path
        lstrcpyn(dirbuff,(char*)SendMessage(module.hwndParent,WM_WA_IPC,0,IPC_GETINIFILE),size);
    }
    else
    {
        char* p = dirbuff;
        p += GetModuleFileName(0,dirbuff,size) - 1;
        while(p && *p != '.'){p--;}
        lstrcpyn(p+1,"ini",size);
    }
    return dirbuff;
}

/* get the log file name (for debug purposes) */
char *get_log_path(char *dirbuff, const int size)
{
    int count;
    const char* filename = "\\resumer.log";

    count = GetEnvironmentVariable( "USERPROFILE", dirbuff, size );
    if( count < size )
        strncat( dirbuff, filename, size );
    else
        strncpy( dirbuff, filename, size );
    return dirbuff;
}

/* Write an integer out to the INI, function used from Pacemaker by
Olli Parviainen */
void WritePrivateProfileInt(const char *sect,const char *varname,const int value,const char *inifile)
{
	char temp[16];
	wsprintf(temp,"%d",value);
	WritePrivateProfileString(sect,varname,temp,inifile);
}

/* Find out how many digits are in the specified number */
int num_digits(int number) 
{
    return (int)log10( (double)number);
}

/* get the (which_num)th item from a string of delim delimited numbers:
i.e., get_num("1,2,3,4,5,6,7,8,9,10", "5", ',') returns 5.
*/
int get_num(char *string, int which_num, char delim)
{
	// Gets a number from the string delimited by delim
	int ii=0;
	int jj=0;
	int kk=0;
	char tempstr[5];

	while ( jj != which_num-1 ) {
		if ( string[ii] == '\0' ) 
			return -1;
		if ( string[ii++] == delim )
			jj++;

	}

	// ii is position of beginning of requested number
	while ( (string[ii] != '\0') && (string[ii] != delim) )
		tempstr[kk++] = string[ii++];

	tempstr[kk] = '\0';

	return atoi(tempstr);
}

/* Setup the timer.. kill the old one and make the new one if necessary */
void do_timer() 
{
	int save_state_interval;
	static BOOL timerActive = FALSE;

	save_state_interval = GetPrivateProfileInt(INI_SECTNAME, KEY_SAVEEVERY, 0, ini_path);

	if ( timerActive ) {
		KillTimer(module.hwndParent, TIMER_ID);
	}
	if ( save_state_interval != 0 ) {
		timerActive = TRUE;
		SetTimer(module.hwndParent, TIMER_ID, (UINT)(save_state_interval*1000), (TIMERPROC)TimerProc);
	}

}
