//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================

//
// Quit game procedure
//

#include "gfx/ali3d.h"
#include "ac/cdaudio.h"
#include "ac/draw.h"
#include "ac/record.h"
#include "ac/translation.h"
#include "ac/walkbehind.h"
#include "debug/agseditordebugger.h"
#include "debug/debug_log.h"
#include "debug/debugger.h"
#include "debug/out.h"
#include "font/fonts.h"
#include "game/game_objects.h"
#include "main/graphics_mode.h"
#include "main/main.h"
#include "main/mainheader.h"
#include "main/quit.h"
#include "ac/spritecache.h"
#include "gfx/graphicsdriver.h"
#include "gfx/bitmap.h"
#include "core/assetmanager.h"

using AGS::Common::Bitmap;
namespace BitmapHelper = AGS::Common::BitmapHelper;
namespace Out = AGS::Common::Out;

extern int spritewidth[MAX_SPRITES],spriteheight[MAX_SPRITES];
extern SpriteCache spriteset;
extern int our_eip;
extern char pexbuf[STD_BUFFER_SIZE];
extern int proper_exit;
extern char check_dynamic_sprites_at_exit;
extern int editor_debugging_initialized;
extern IAGSEditorDebugger *editor_debugger;
extern int need_to_stop_cd;
extern Bitmap *_old_screen;
extern Bitmap *_sub_screen;
extern int use_cdplayer;
extern IGraphicsDriver *gfxDriver;

bool handledErrorInEditor;

void quit_tell_editor_debugger(char *qmsg)
{
    if (editor_debugging_initialized)
    {
        if ((qmsg[0] == '!') && (qmsg[1] != '|'))
        {
            handledErrorInEditor = send_exception_to_editor(&qmsg[1]);
        }
        send_message_to_editor("EXIT");
        editor_debugger->Shutdown();
    }
}

void quit_stop_cd()
{
    if (need_to_stop_cd)
        cd_manager(3,0);
}

void quit_shutdown_scripts()
{
    ccUnregisterAllObjects();
}

void quit_check_dynamic_sprites(char *qmsg)
{
    if ((qmsg[0] == '|') && (check_dynamic_sprites_at_exit) && 
        (game.Options[OPT_DEBUGMODE] != 0)) {
            // game exiting normally -- make sure the dynamic sprites
            // have been deleted
            for (int i = 1; i < spriteset.elements; i++) {
                if (game.SpriteFlags[i] & SPF_DYNAMICALLOC)
                    debug_log("Dynamic sprite %d was never deleted", i);
            }
    }
}

void quit_shutdown_platform(char *qmsg)
{
    platform->AboutToQuitGame();

    our_eip = 9016;

    platform->ShutdownPlugins();

    quit_check_dynamic_sprites(qmsg);    

    // allegro_exit assumes screen is correct
	if (_old_screen) {
		BitmapHelper::SetScreenBitmap( _old_screen );
	}

    platform->FinishedUsingGraphicsMode();

    if (use_cdplayer)
        platform->ShutdownCDPlayer();
}

void quit_shutdown_audio()
{
    our_eip = 9917;
    game.Options[OPT_CROSSFADEMUSIC] = 0;
    stopmusic();
#ifndef PSP_NO_MOD_PLAYBACK
    if (usetup.ModPlayer)
        remove_mod_player();
#endif

    // Quit the sound thread.
    audioThread.Stop();

    remove_sound();
}

void quit_check_for_error_state(char *&qmsg, char *alertis)
{
    if (qmsg[0]=='|') ; //qmsg++;
    else if (qmsg[0]=='!') { 
        qmsg++;

        if (qmsg[0] == '|')
            strcpy (alertis, "Abort key pressed.\n\n");
        else if (qmsg[0] == '?') {
            strcpy(alertis, "A fatal error has been generated by the script using the AbortGame function. Please contact the game author for support.\n\n");
            qmsg++;
        }
        else
            sprintf(alertis,"An error has occurred. Please contact the game author for support, as this "
            "is likely to be a scripting error and not a bug in AGS.\n"
            "(ACI version %s)\n\n", EngineVersion.LongString.GetCStr());

        strcat (alertis, get_cur_script(5) );

        if (qmsg[0] != '|')
            strcat(alertis,"\nError: ");
        else
            qmsg = "";
    }
    else if (qmsg[0] == '%') {
        qmsg++;

        sprintf(alertis, "A warning has been generated. This is not normally fatal, but you have selected "
            "to treat warnings as errors.\n"
            "(ACI version %s)\n\n%s\n", EngineVersion.LongString.GetCStr(), get_cur_script(5));
    }
    else sprintf(alertis,"An internal error has occurred. Please note down the following information.\n"
        "If the problem persists, post the details on the AGS Technical Forum.\n"
        "(ACI version %s)\n"
        "\nError: ", EngineVersion.LongString.GetCStr());
}

void quit_destroy_subscreen()
{
    // close graphics mode (Win) or return to text mode (DOS)
    delete _sub_screen;
	_sub_screen = NULL;
}

void quit_message_on_exit(char *qmsg, char *alertis)
{
    // successful exit displays no messages (because Windoze closes the dos-box
    // if it is empty).
    if (qmsg[0]=='|') ;
    else if (!handledErrorInEditor)
    {
        // Display the message (at this point the window still exists)
        sprintf(pexbuf,"%s\n",qmsg);
        strcat(alertis,pexbuf);
        platform->DisplayAlert(alertis);
    }
}

void quit_release_data()
{
    // wipe all the interaction structs so they don't de-alloc the children twice
    ResetRoomStates();
    troom.Free();

    /*  _CrtMemState memstart;
    _CrtMemCheckpoint(&memstart);
    _CrtMemDumpStatistics( &memstart );*/
}

void quit_print_last_fps(char *qmsg)
{
    /*  // print the FPS if there wasn't an error
    if ((play.DebugMode!=0) && (qmsg[0]=='|'))
    printf("Last cycle fps: %d\n",fps);*/
}

void quit_delete_temp_files()
{
    al_ffblk	dfb;
    int	dun = al_findfirst("~ac*.tmp",&dfb,FA_SEARCH);
    while (!dun) {
        unlink(dfb.name);
        dun = al_findnext(&dfb);
    }
    al_findclose (&dfb);
}

void free_globals()
{
#if defined (WINDOWS_VERSION)
    if (wArgv)
    {
        LocalFree(wArgv);
        wArgv = NULL;
    }
#endif
}

// TODO: move to test unit
extern Bitmap *test_allegro_bitmap;
extern IDriverDependantBitmap *test_allegro_ddb;
void allegro_bitmap_test_release()
{
	delete test_allegro_bitmap;
	if (test_allegro_ddb)
		gfxDriver->DestroyDDB(test_allegro_ddb);
}

char return_to_roomedit[30] = "\0";
char return_to_room[150] = "\0";
// quit - exits the engine, shutting down everything gracefully
// The parameter is the message to print. If this message begins with
// an '!' character, then it is printed as a "contact game author" error.
// If it begins with a '|' then it is treated as a "thanks for playing" type
// message. If it begins with anything else, it is treated as an internal
// error.
// "!|" is a special code used to mean that the player has aborted (Alt+X)
void quit(const char *quitmsg) {

    // Need to copy it in case it's from a plugin (since we're
    // about to free plugins)
    char qmsgbufr[STD_BUFFER_SIZE];
    strncpy(qmsgbufr, quitmsg, STD_BUFFER_SIZE);
    qmsgbufr[STD_BUFFER_SIZE - 1] = 0;
    char *qmsg = &qmsgbufr[0];

	allegro_bitmap_test_release();

    handledErrorInEditor = false;

    quit_tell_editor_debugger(qmsg);

    our_eip = 9900;

    stop_recording();

    quit_stop_cd();

    our_eip = 9020;

    quit_shutdown_scripts();

    quit_shutdown_platform(qmsg);

    our_eip = 9019;

    quit_shutdown_audio();
    
    our_eip = 9901;

    char alertis[1500]="\0";
    quit_check_for_error_state(qmsg, alertis);

    shutdown_font_renderer();
    our_eip = 9902;

    quit_destroy_subscreen();

    our_eip = 9907;

    close_translation();

    our_eip = 9908;

    thisroom.Free();
    CharActiveSprites.Free();
    ObjActiveSprites.Free();

    graphics_mode_shutdown();

    quit_message_on_exit(qmsg, alertis);

    // remove the game window
    allegro_exit();

    WalkBehindPlacements.Free();
    platform->PostAllegroExit();

    our_eip = 9903;

    quit_release_data();

    quit_print_last_fps(qmsg);

    quit_delete_temp_files();

    Common::AssetManager::DestroyInstance();

    proper_exit=1;

    Out::FPrint("***** ENGINE HAS SHUTDOWN");

    shutdown_debug_system();
    free_globals();

    our_eip = 9904;
    exit(EXIT_NORMAL);
}

extern "C" {
    void quit_c(char*msg) {
        quit(msg);
    }
}
