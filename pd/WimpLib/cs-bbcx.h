/* cs-bbcx.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1989-1998 Colton Software Limited
 * Copyright (C) 1998-2014 R W Colton */

#ifndef __cs_bbcx_h
#define __cs_bbcx_h

#ifndef __bbc_h
#include "bbc.h"
#endif

/*
cs-riscasm.s
*/

_Check_return_
_Ret_maybenull_
extern _kernel_oserror *
os_writeN(
    _In_reads_(count) const char * s,
    _InVal_     U32 count);

_Check_return_
_Ret_maybenull_
extern _kernel_oserror *
os_plot(
    _InVal_     int code,
    _InVal_     int x,
    _InVal_     int y);

#if !defined(COMPILING_WIMPLIB)

#define bbc_draw(x, y) \
    os_plot(bbc_SolidBoth + bbc_DrawAbsFore, x, y)

/* Draw a line to coordinates specified relative to current graphic cursor. */
#define bbc_drawby(x, y) \
    os_plot(bbc_SolidBoth + bbc_DrawRelFore, x, y)

#define bbc_move(x, y) \
    os_plot(bbc_SolidBoth + bbc_MoveCursorAbs, x, y)

#endif /* COMPILING_WIMPLIB */

#endif /* __cs_bbcx_h */

/* end of cs_bbcx.h */
