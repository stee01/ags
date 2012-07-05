
#include <cctype>
#include "wgt2allg.h"
#include "ali3d.h"
#include "ac/ac_common.h"
#include "ac/gamesetup.h"
#include "gui/guimain.h"
#include "ac/gui.h"
#include "acmain/ac_mouse.h"
#include "acmain/ac_record.h"
#include "media/audio/audio.h"
#include "gui/cscidialog.h"
#include "gui/guidialog.h"
#include "gui/mycontrols.h"
#include "main/game_run.h"

extern char ignore_bounds; // from mousew32
extern IGraphicsDriver *gfxDriver;
extern volatile int timerloop; // ac_timer
extern GameSetup usetup;

//extern void get_save_game_path(int slotNum, char *buffer);
//extern char saveGameDirectory[260];


//-----------------------------------------------------------------------------
// DIALOG SYSTEM STUFF below

IDriverDependantBitmap *dialogBmp = NULL;
int windowPosX, windowPosY, windowPosWidth, windowPosHeight;
block windowBuffer = NULL;

int windowbackgroundcolor = COL254, pushbuttondarkcolor = COL255;
int pushbuttonlightcolor = COL253;
int topwindowhandle = -1;
int cbuttfont;

int acdialog_font;

int smcode = 0;

#define MAXCONTROLS 20
#define MAXSCREENWINDOWS 5
NewControl *vobjs[MAXCONTROLS];
OnScreenWindow oswi[MAXSCREENWINDOWS];

int controlid = 0;


//-----------------------------------------------------------------------------

void __my_wbutt(int x1, int y1, int x2, int y2)
{
    wsetcolor(COL254);            //wsetcolor(15);
    wbar(x1, y1, x2, y2);
    wsetcolor(0);
    wrectangle(x1, y1, x2, y2);
}

//-----------------------------------------------------------------------------

int WINAPI _export CSCIGetVersion()
{
    return 0x0100;
}

int windowcount = 0, curswas = 0;
int WINAPI _export CSCIDrawWindow(int xx, int yy, int wid, int hit)
{
    ignore_bounds++;
    multiply_up(&xx, &yy, &wid, &hit);
    int drawit = -1;
    for (int aa = 0; aa < MAXSCREENWINDOWS; aa++) {
        if (oswi[aa].buffer == NULL) {
            drawit = aa;
            break;
        }
    }

    if (drawit < 0)
        quit("Too many windows created.");

    windowcount++;
    //  domouse(2);
    xx -= 2;
    yy -= 2;
    wid += 4;
    hit += 4;
    oswi[drawit].buffer = wnewblock(xx, yy, xx + wid, yy + hit);
    oswi[drawit].x = xx;
    oswi[drawit].y = yy;
    wbutt(xx + 1, yy + 1, xx + wid - 1, yy + hit - 1);    // wbutt goes outside its area
    //  domouse(1);
    oswi[drawit].oldtop = topwindowhandle;
    topwindowhandle = drawit;
    windowPosX = xx;
    windowPosY = yy;
    windowPosWidth = wid;
    windowPosHeight = hit;
    return drawit;
}

void WINAPI _export CSCIEraseWindow(int handl)
{
    //  domouse(2);
    ignore_bounds--;
    topwindowhandle = oswi[handl].oldtop;
    wputblock(oswi[handl].x, oswi[handl].y, oswi[handl].buffer, 0);
    wfreeblock(oswi[handl].buffer);
    //  domouse(1);
    oswi[handl].buffer = NULL;
    windowcount--;
}

int WINAPI _export CSCIWaitMessage(CSCIMessage * cscim)
{
    next_iteration();
    wtexttransparent(TEXTFG);
    for (int uu = 0; uu < MAXCONTROLS; uu++) {
        if (vobjs[uu] != NULL) {
            //      domouse(2);
            vobjs[uu]->drawifneeded();
            //      domouse(1);
        }
    }

    windowBuffer = create_bitmap_ex(bitmap_color_depth(abuf), windowPosWidth, windowPosHeight);
    windowBuffer = gfxDriver->ConvertBitmapToSupportedColourDepth(windowBuffer);
    dialogBmp = gfxDriver->CreateDDBFromBitmap(windowBuffer, false, true);

    while (1) {
        timerloop = 0;
        next_iteration();
        refresh_screen();

        cscim->id = -1;
        cscim->code = 0;
        smcode = 0;
        if (kbhit()) {
            int keywas = getch();
            if (keywas == 0)
                keywas = getch() + 300;

            if (keywas == 13) {
                cscim->id = finddefaultcontrol(CNF_DEFAULT);
                cscim->code = CM_COMMAND;
            } else if (keywas == 27) {
                cscim->id = finddefaultcontrol(CNF_CANCEL);
                cscim->code = CM_COMMAND;
            } else if ((keywas < 32) && (keywas != 8)) ;
            else if ((keywas >= 372) & (keywas <= 381) & (finddefaultcontrol(CNT_LISTBOX) >= 0))
                vobjs[finddefaultcontrol(CNT_LISTBOX)]->processmessage(CTB_KEYPRESS, keywas, 0);
            else if (finddefaultcontrol(CNT_TEXTBOX) >= 0)
                vobjs[finddefaultcontrol(CNT_TEXTBOX)]->processmessage(CTB_KEYPRESS, keywas, 0);

            if (cscim->id < 0) {
                cscim->code = CM_KEYPRESS;
                cscim->wParam = keywas;
            }
        }

        if (rec_mgetbutton() != NONE) {
            if (checkcontrols()) {
                cscim->id = controlid;
                cscim->code = CM_COMMAND;
            }
        }

        if (smcode) {
            cscim->code = smcode;
            cscim->id = controlid;
        }

        if (cscim->code > 0)
            break;

        update_polled_stuff_and_crossfade();
        while (timerloop == 0) ;
    }

    gfxDriver->DestroyDDB(dialogBmp);
    dialogBmp = NULL;
    destroy_bitmap(windowBuffer);
    windowBuffer = NULL;
    return 0;
}

int WINAPI _export CSCICreateControl(int typeandflags, int xx, int yy, int wii, int hii, char *title)
{
    multiply_up(&xx, &yy, &wii, &hii);
    int usec = -1;
    for (int hh = 1; hh < MAXCONTROLS; hh++) {
        if (vobjs[hh] == NULL) {
            usec = hh;
            break;
        }
    }

    if (usec < 0)
        quit("Too many controls created");

    int type = typeandflags & 0x00ff;     // 256 control types
    if (type == CNT_PUSHBUTTON) {
        if (wii == -1)
            wii = wgettextwidth(title, cbuttfont) + 20;

        vobjs[usec] = new MyPushButton(xx, yy, wii, hii, title);

    } else if (type == CNT_LISTBOX) {
        vobjs[usec] = new MyListBox(xx, yy, wii, hii);
    } else if (type == CNT_LABEL) {
        vobjs[usec] = new MyLabel(xx, yy, wii, title);
    } else if (type == CNT_TEXTBOX) {
        vobjs[usec] = new MyTextBox(xx, yy, wii, title);
    } else
        quit("Unknown control type requested");

    vobjs[usec]->typeandflags = typeandflags;
    wtexttransparent(TEXTFG);
    vobjs[usec]->wlevel = topwindowhandle;
    //  domouse(2);
    vobjs[usec]->draw();
    //  domouse(1);
    return usec;
}

void WINAPI _export CSCIDeleteControl(int haa)
{
    delete vobjs[haa];
    vobjs[haa] = NULL;
}

int WINAPI _export CSCISendControlMessage(int haa, int mess, int wPar, long lPar)
{
    if (vobjs[haa] == NULL)
        return -1;
    return vobjs[haa]->processmessage(mess, wPar, lPar);
}

void multiply_up_to_game_res(int *x, int *y)
{
    x[0] = get_fixed_pixel_size(x[0]);
    y[0] = get_fixed_pixel_size(y[0]);
}

void multiply_up(int *x1, int *y1, int *x2, int *y2)
{
    multiply_up_to_game_res(x1, y1);
    multiply_up_to_game_res(x2, y2);

    // adjust for 800x600
    if ((GetBaseWidth() == 400) || (GetBaseWidth() == 800)) {
        x1[0] = (x1[0] * 5) / 4;
        x2[0] = (x2[0] * 5) / 4;
        y1[0] = (y1[0] * 3) / 2;
        y2[0] = (y2[0] * 3) / 2;
    }
    else if (GetBaseWidth() == 1024)
    {
        x1[0] = (x1[0] * 16) / 10;
        x2[0] = (x2[0] * 16) / 10;
        y1[0] = (y1[0] * 384) / 200;
        y2[0] = (y2[0] * 384) / 200;
    }
}

int checkcontrols()
{
    smcode = 0;
    for (int kk = 0; kk < MAXCONTROLS; kk++) {
        if (vobjs[kk] != NULL) {
            if (vobjs[kk]->mouseisinarea()) {
                controlid = kk;
                return vobjs[kk]->pressedon();
            }
        }
    }
    return 0;
}

int finddefaultcontrol(int flagmask)
{
    for (int ff = 0; ff < MAXCONTROLS; ff++) {
        if (vobjs[ff] == NULL)
            continue;

        if (vobjs[ff]->wlevel != topwindowhandle)
            continue;

        if (vobjs[ff]->typeandflags & flagmask)
            return ff;
    }

    return -1;
}

int GetBaseWidth () {
    return BASEWIDTH;
}
