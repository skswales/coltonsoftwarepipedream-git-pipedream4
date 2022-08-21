/* datatype.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1989-1998 Colton Software Limited
 * Copyright (C) 1998-2014 R W Colton */

/* PipeDream datatypes */

/* RJM 8-Apr-1989 */

#ifndef __datatype_h
#define __datatype_h

#if RISCOS
#define uchar char
#endif

#ifndef BOOL
#define BOOL int
#endif

typedef uchar BOOLEAN;          /* packed flag */

/* handle of a graphics DDE link */
typedef S32 ghandle;

/* ----------------------------------------------------------------------- */

typedef time_t DATE;

/* time */

#define YEAR_OFFSET 80

/* ----------------------------------------------------------------------- */

typedef int coord;      /* coordinate type for screen, MUST BE SIGNED */
typedef int gcoord;     /* graphics coordinate type */

/*
Column types
*/

/*
 * 29 bit quantities, 30th bit for absolute, 31th for bad
 * -ve used for falling out of loops
*/

typedef S32 COL; typedef COL * P_COL;

#define NO_COL                  ((COL) ((1 << 30)-1))
#define LARGEST_COL_POSSIBLE    ((COL) ((1 << 29)-1))
#define COLNOBITS               LARGEST_COL_POSSIBLE
#define ABSCOLBIT               ((COL) (1 << 29))
#define BADCOLBIT               ((COL) (1 << 30))

typedef S32 ROW; typedef ROW * P_ROW;

#define NO_ROW                  ((ROW) ((1 << 30) -1)
#define LARGEST_ROW_POSSIBLE    ((ROW) ((1 << 29) -1))
#define ROWNOBITS               LARGEST_ROW_POSSIBLE
#define ABSROWBIT               ((ROW) (1 << 29))
#define BADROWBIT               ((ROW) (1 << 30))

/******************************************************************************
* other constants
******************************************************************************/

#define CMD_BUFSIZ  ((S32) 255)        /* size of buffers */
#define LIN_BUFSIZ  ((S32) 1000)
#define MAXFLD      (LIN_BUFSIZ - 1)

/* On RISC OS the worst case is MAXFLD ctrlchars being expanded to
 * four characters each plus an overhead for an initial font change and NULLCH
*/
#if 1
/* SKS after 4.12 23mar92 - reduce to avoid excessive stack consumption (^b and ^c now stripped on load which were the main culprits...) */
#define PAINT_STRSIZ    (LIN_BUFSIZ * 2)
#else
#define PAINT_STRSIZ    (LIN_BUFSIZ * 4)
#endif

/* Output buffer for text slot compilation: worst case is LINBUF full of
 * small SLR, op such as '@A1@'
*/
#define COMPILED_TEXT_BUFSIZ ((LIN_BUFSIZ / 4) * (SLRSIZE + 2))

/* maximum length of a textual slot reference */
#define BUF_MAX_REFERENCE (BUF_MAX_PATHSTRING + 2 + 10)

/* maximum size of a low level slot */
#define MAX_SLOTSIZE (EV_MAX_OUT_LEN + sizeof(struct _slot))

/* ----------------------------------------------------------------------- */

/* structures for expression analyser */

#define DOCNO EV_DOCNO /* NB packed */

typedef struct _CR_SLR /* SLR with just col, row */
{
    COL col;
    ROW row;
}
SLR, * P_SLR; typedef const SLR * PC_SLR;

typedef struct _FULL_SLR /* SLR with col, row and an EV_DOCNO for completeness */
{
    COL col;
    ROW row;
    EV_DOCNO docno;
}
FULL_SLR, * P_FULL_SLR; typedef const FULL_SLR * PC_FULL_SLR;

#define SLRSIZE (1 + sizeof(EV_DOCNO) + sizeof(COL) + sizeof(ROW))

/*
a type to go traversing blocks with
*/

typedef enum
{
    TRAVERSE_ACROSS_ROWS,
    TRAVERSE_DOWN_COLUMNS
}
TRAVERSE_BLOCK_DIRECTION;

typedef struct _TRAVERSE_BLOCK
{
    /* contents are private to traversal routines */
    DOCNO docno;
    SLR cur;
    SLR stt;
    SLR end;
    TRAVERSE_BLOCK_DIRECTION direction;
    BOOL start;
    P_LIST_ITEM it; /* for fast traversal */
    struct _DOCU * p_docu;
}
TRAVERSE_BLOCK, * TRAVERSE_BLOCKP;

#define traverse_block_cur_col(blk) ((blk)->cur.col)
#define traverse_block_cur_row(blk) ((blk)->cur.row)

/* ----------------------------- lists.c --------------------------------- */

#include "lists_x.h"

#include "cmodules/collect.h"

/******************************************************************************
* slot.c
******************************************************************************/

typedef struct _slot *P_SLOT; /* slot pointer type */

/*
column entry for use in sparse matrix
*/

typedef struct colentry
{
    LIST_BLOCK lb;
    S32 wrapwidth;
    S32 colwidth;
    S32 colflags;
}
* P_COLENTRY;

/* object on deleted words list describing a block */

typedef struct _saved_block_descriptor
{
    P_COLENTRY del_colstart;
    COL del_col_size;
    ROW del_row_size;
}
saved_block_descriptor;

/* ------------------------------ commlin.c ------------------------------ */

#define short_menus() (d_menu[0].option)

#define NOMENU 0

#define CHANGED    1
#define LONG_ONLY  2

#define on_short_menus(flag)    !(((flag & LONG_ONLY) == 0) ^ ((flag & CHANGED) == 0))

#define TICKABLE     4
#define TICK_STATUS  8
#define GREYABLE     16
#define GREY_STATUS  32
#define HAS_DIALOG   64
#define DOC_REQ      128  /* document required to do this command */
#define GREY_EXPEDIT 256
#define NO_REPEAT    512  /* REPEAT COMMAND ignores this */
#define NO_MACROFILE 1024 /* don't output this command in the macro recorder */
#define ALWAYS_SHORT 2048 /* some commands can't be removed from short menus */
#define DO_MERGEBUF  4096 /* mergebuf() prior to command */

typedef struct _menu
{
    PC_U8 *title;
    const char *command;
    S32 key;
    S16 flags;
    S16 funcnum;   /* better packing */
    void (*cmdfunc)(void);
}
MENU;

typedef struct _menu_head
{
    PC_U8 *name;
    MENU *tail;
    char installed;
    char items;
    char titlelen;
    char commandlen;
    void * m; /* abuse: RISC OS submenu^ kept here */
}
MENU_HEAD;

/* ----------------------------- scdraw.c -------------------------------- */

/* horizontal and vertical screen tables */

typedef struct _scrrow
{
    ROW  rowno;
    S32   page;
    uchar flags;
}
SCRROW;

typedef struct _scrcol
{
    COL  colno;
    uchar flags;
}
SCRCOL;

/* ------------------------------ dialog.c ------------------------------- */

/* Saves acres of superfluous code masking */
typedef S32 optiontype;

typedef struct _dialog_entry
{
    uchar type;         /* type of field, text, number, special */
    uchar ch1;          /* first character of save option string */
    uchar ch2;          /* second character of save option string */
    optiontype option;  /* single character option eg Y, sometimes int index */
    PC_U8 *optionlist; /* range of possible values for option, first is default */
    char *textfield;    /* user specified name of something */
    U32 offset;         /* of corresponding variable in windvars */
}
DIALOG;

/* dialog box entry checking: n is maximum used in d_D[n].whatever expr. */
#define assert_dialog(n, D) assert((n+1) == dialog_head[D].items)

typedef void (* dialog_proc) (DIALOG *dptr);

typedef struct _dheader
{
    DIALOG * dialog_box;
    S32 items;
    S32 flags;
    dialog_proc dproc;
    const char * dname;
}
DHEADER;

/* --------------------------- cursmov.c --------------------------------- */

#define SAVE_DEPTH 5

typedef struct _savpos
{
    COL ref_col;
    ROW ref_row;
    DOCNO ref_docno;
}
SAVPOS;

/* --------------------------- pdriver.c -------------------------------- */

typedef struct _driver
{
    BOOL (* out) (_InVal_ U8 ch);
    BOOL (* on)  (void);
    BOOL (* off) (BOOL ok);
}
DRIVER;

/* --------------------------- numbers.c -------------------------------- */

/*
structure of reference to a draw file
*/

typedef struct _draw_file_ref
{
    GR_CACHE_HANDLE draw_file_key;
    EV_DOCNO docno;
    COL col;
    ROW row;
    F64 xfactor;
    F64 yfactor;
    S32 xsize_os;
    S32 ysize_os;
}
* drawfrp;

/*
 * structure of an entry in the graphics link list
 * ghandle of entry used as key
*/

typedef struct graphics_link_entry
{
    DOCNO docno;            /* where the block is */
    COL col;
    ROW row;

    S32 task;               /* task id of client */
    BOOL update;            /* does client want updates? */
    BOOL datasent;          /* data sent without end marker */

    S32 ext_dep_han;        /* handle to external dependency */
    ghandle ghan;
    S32 xsize;
    S32 ysize;
    char text[1];           /* leafname & tag, 0-terminated */
}
* graphlinkp;

/* --------------------------- riscos.c ---------------------------------- */

/* Abstract objects for export */
/*typedef struct riscos__window *    riscos_window;*//*use wimp_w*/
typedef struct riscos__eventstr *  riscos_eventstr;
typedef struct riscos__redrawstr * riscos_redrawstr;

typedef struct _riscos_fileinfo
{
    unsigned int exec; /* order important! */
    unsigned int load;
    unsigned int length;
}
riscos_fileinfo;

typedef void (* riscos_redrawproc) (riscos_redrawstr * redrawstr);

typedef void (* riscos_printproc) (void);

/* ----------------------------------------------------------------------- */

enum _driver_types
{
    driver_riscos,
    driver_parallel,
    driver_serial,
    driver_network,
    driver_user
};

/* set of colours used by PipeDream */

enum _d_colour_offsets
{
    FORE,
    BACK,
    PROTECTC,
    NEGATIVEC,
    GRIDC,
    CARETC,
    SOFTPAGEBREAKC,
    HARDPAGEBREAKC,

    BORDERFOREC,     /*BORDERC,*/
    BORDERBACKC,
    CURBORDERFOREC,  /*CURBORDERC,*/
    CURBORDERBACKC,
    FIXBORDERBACKC,
    N_COLOURS
};

/* the following are entries in the print dialog box */

enum _d_print_offsets
{
    P_PSFON,
    P_PSFNAME,
    P_DATABON,
    P_DATABNAME,
    P_OMITBLANK,
    P_BLOCK,
    P_TWO_ON,
    P_TWO_MARGIN,
    P_COPIES,
    P_WAIT,
    P_ORIENT,
    P_SCALE,
    P_THE_LAST_ONE
};

/* entries in the sort block dialog box */

#define SORT_FIELD_DEPTH        5           /* number of col,ascend pairs */
#define SORT_FIELD_COLUMN       0
#define SORT_FIELD_ASCENDING    1

/* rjm removes update refs 14.9.91 */
#if 1
#define SORT_MULTI_ROW        (SORT_FIELD_COLUMN + (SORT_FIELD_DEPTH * 2))
#else
#define SORT_UPDATE_REFS        (SORT_FIELD_COLUMN + (SORT_FIELD_DEPTH * 2))
#define SORT_MULTI_ROW          (SORT_UPDATE_REFS + 1)
#endif

typedef struct _PDCHART_TAGSTRIP_INFO
{
    /*
    out
    */

    P_ANY pdchartdatakey;
}
* P_PDCHART_TAGSTRIP_INFO;

#endif /* __datatype_h */

/* end of datatype.h */
