/* gr_coord.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1991-1998 Colton Software Limited
 * Copyright (C) 1998-2014 R W Colton */

/* Coordinate manipulation */

/* SKS July 1991 */

#ifndef __gr_coord_h
#define __gr_coord_h

/*
structure
*/

/* coordinates are large signed things */
typedef S32 GR_COORD; typedef GR_COORD * P_GR_COORD;

#define GR_COORD_MAX S32_MAX

/*
points, or simply pairs of coordinates
*/

typedef struct _GR_POINT
{
    GR_COORD x, y;
}
GR_POINT, * P_GR_POINT; typedef const GR_POINT * PC_GR_POINT;

/*
boxes, or simply pairs of points
*/

typedef struct _GR_BOX
{
    GR_COORD x0, y0, x1, y1;
}
GR_BOX, * P_GR_BOX; typedef const GR_BOX * PC_GR_BOX;

/*
ordered rectangles
*/

#if defined(UNUSED)

typedef struct _GR_RECT
{
    GR_POINT tl, br;
}
GR_RECT, * P_GR_RECT; typedef const GR_RECT * PC_GR_RECT;

#endif /* UNUSED */

typedef S32 GR_SCALE; /* signed 16.16 fixed point number */

#define GR_SCALE_ZERO    0          /* 0.0 */
#define GR_SCALE_ONE     0x00010000 /* 1.0 */
#define GR_SCALE_MAX     0x7FFFFFFF /* maximum number in this representation */

#define GR_MAX_FOR_SCALE 0x7FFF     /* maximum number that can be converted to this representation */

typedef struct _gr_bin16_pair
{
    GR_SCALE x, y;
}
GR_SCALE_PAIR, * P_GR_SCALE_PAIR; typedef const GR_SCALE_PAIR * PC_GR_SCALE_PAIR;

/*
matrix transforms
*/

typedef struct _GR_XFORMMATRIX
{
    GR_SCALE a, b;
    GR_SCALE c, d;
    GR_COORD e, f;
}
GR_XFORMMATRIX, * P_GR_XFORMMATRIX; typedef const GR_XFORMMATRIX * PC_GR_XFORMMATRIX;

/*
exported functions
*/

extern void
eliminate_common_factors(
    _InoutRef_  P_S32 p_numer,
    _InoutRef_  P_S32 p_denom);

/*
box operations
*/

_Check_return_
extern S32
gr_box_hit(
    _OutRef_    P_GR_POINT offset,
    _InRef_     PC_GR_BOX abox,
    _InRef_     PC_GR_POINT apoint,
    _InRef_opt_ PC_GR_POINT semimajor /*NULL->exact hit required*/);

_Check_return_
extern S32
gr_box_intersection(
    _OutRef_    P_GR_BOX ibox,
    _In_opt_    PC_GR_BOX abox /*NULL->src ibox*/,
    _InRef_     PC_GR_BOX bbox);

extern void
gr_box_make_bad(
    _OutRef_    P_GR_BOX abox);

extern void
gr_box_make_huge(
    _OutRef_    P_GR_BOX abox);

extern void
gr_box_make_null(
    _OutRef_    P_GR_BOX abox);

extern P_GR_BOX
gr_box_rotate(
    _OutRef_    P_GR_BOX xbox,
    _In_opt_    PC_GR_BOX abox /*NULL->src xbox*/,
    _InRef_     PC_GR_POINT spoint,
    _InRef_     PC_F64 angle);

extern P_GR_BOX
gr_box_scale(
    _OutRef_    P_GR_BOX xbox,
    _InRef_     PC_GR_BOX abox,
    _InRef_     PC_GR_POINT spoint,
    _InRef_     PC_GR_SCALE_PAIR scale);

extern void
gr_box_sort(
    _OutRef_    P_GR_BOX sbox,
    _InRef_     PC_GR_BOX abox /*may==sbox*/);

extern P_GR_BOX
gr_box_translate(
    _OutRef_    P_GR_BOX xbox,
    _In_opt_    PC_GR_BOX abox /*NULL->src xbox*/,
    _InRef_     PC_GR_POINT spoint);

extern P_GR_BOX
gr_box_union(
    _OutRef_    P_GR_BOX ubox,
    _In_opt_    PC_GR_BOX abox /*NULL->src ubox*/,
    _InRef_     PC_GR_BOX bbox);

extern P_GR_BOX
gr_box_xform(
    _OutRef_    P_GR_BOX xbox,
    _In_opt_    PC_GR_BOX abox /*NULL->src xbox*/,
    _InRef_     PC_GR_XFORMMATRIX xform);

/*
coordinate operations
*/

_Check_return_
static inline GR_COORD
gr_coord_from_f64(
    _InVal_     F64 d)
{
    if(d >= +GR_COORD_MAX)
        return(+GR_COORD_MAX);
    if(d <= -GR_COORD_MAX)
        return(-GR_COORD_MAX);
    return((GR_COORD) d);
}

_Check_return_
extern GR_COORD
gr_coord_scale(
    _InVal_     GR_COORD coord,
    _InVal_     GR_SCALE scale);

_Check_return_
extern GR_COORD
gr_coord_scale_inverse(
    _InVal_     GR_COORD coord,
    _InVal_     GR_SCALE scale);

extern void
gr_coord_sort(
    _InoutRef_  P_GR_COORD acoord,
    _InoutRef_  P_GR_COORD bcoord);

/*
point operations
*/

extern P_GR_POINT
gr_point_rotate(
    _OutRef_    P_GR_POINT xpoint,
    _InRef_     PC_GR_POINT apoint,
    _InRef_     PC_GR_POINT spoint,
    _InRef_     PC_F64 angle);

extern P_GR_POINT
gr_point_scale(
    _OutRef_    P_GR_POINT xpoint,
    _InRef_     PC_GR_POINT apoint,
    _InRef_opt_ PC_GR_POINT spoint,
    _InRef_     PC_GR_SCALE_PAIR scale);

extern P_GR_POINT
gr_point_translate(
    _OutRef_    P_GR_POINT xpoint,
    _InRef_     PC_GR_POINT apoint,
    _InRef_     PC_GR_POINT spoint);

extern P_GR_POINT
gr_point_xform(
    _OutRef_    P_GR_POINT xpoint,
    _InRef_     PC_GR_POINT apoint,
    _InRef_     PC_GR_XFORMMATRIX xform);

/*
scale building
*/

#define gr_f64_from_scale(b) ( \
    ((F64) (b)) / GR_SCALE_ONE )

_Check_return_
static inline GR_SCALE
gr_scale_from_f64(
    _InVal_     F64 d)
{
    if(d >= +GR_MAX_FOR_SCALE)
        return(+GR_SCALE_MAX);
    if(d <= -GR_MAX_FOR_SCALE)
        return(-GR_SCALE_MAX);
    return((GR_SCALE) ((d) * GR_SCALE_ONE));
}

_Check_return_
extern GR_SCALE
gr_scale_from_s32_pair(
    _InVal_     S32 numerator,
    _InVal_     S32 denominator);

/*
transformation building
*/

/*ncr*/
extern P_GR_XFORMMATRIX
gr_xform_make_combination(
    _OutRef_    P_GR_XFORMMATRIX xform,
    _InRef_     PC_GR_XFORMMATRIX axform,
    _InRef_     PC_GR_XFORMMATRIX bxform);

/*ncr*/
extern P_GR_XFORMMATRIX
gr_xform_make_rotation(
    _OutRef_    P_GR_XFORMMATRIX xform,
    _InRef_     PC_GR_POINT spoint,
    _InRef_     PC_F64 angle);

/*ncr*/
extern P_GR_XFORMMATRIX
gr_xform_make_scale(
    _OutRef_    P_GR_XFORMMATRIX xform,
    _InRef_opt_ PC_GR_POINT spoint /*NULL->origin*/,
    _InVal_     GR_SCALE xscale,
    _InVal_     GR_SCALE yscale);

extern void
gr_xform_make_translation(
    _OutRef_    P_GR_XFORMMATRIX xform,
    _InRef_     PC_GR_POINT spoint);

/*
Draw analogues
*/

/*
box
*/

static inline void
draw_box_make_bad(
    _OutRef_    P_DRAW_BOX p_draw_box)
{
    gr_box_make_bad((const P_GR_BOX) p_draw_box);
}

static inline void
draw_box_translate(
    _OutRef_    P_DRAW_BOX xbox,
    _In_opt_    PC_DRAW_BOX abox /*NULL->src xbox*/,
    _InRef_     PC_DRAW_POINT spoint)
{
    gr_box_translate((const P_GR_BOX) xbox, (PC_GR_BOX) abox, (const PC_GR_POINT) spoint);
}

static inline void
draw_box_union(
    _OutRef_    P_DRAW_BOX ubox,
    _In_opt_    PC_DRAW_BOX abox /*NULL->src ubox*/,
    _InRef_     PC_DRAW_BOX bbox)
{
    gr_box_union((const P_GR_BOX) ubox, (PC_GR_BOX) abox, (const PC_GR_BOX) bbox);
}

static inline void
draw_box_xform(
    _OutRef_    P_DRAW_BOX xbox,
    _In_opt_    PC_DRAW_BOX abox /*NULL->src xbox*/,
    _InRef_     PC_GR_XFORMMATRIX xform)
{
    gr_box_xform((const P_GR_BOX) xbox, (PC_GR_BOX) abox, xform);
}

/*
point
*/

static inline void
draw_point_xform(
    _OutRef_    P_DRAW_POINT xpoint,
    _InRef_     PC_DRAW_POINT apoint,
    _InRef_     PC_GR_XFORMMATRIX xform)
{
    gr_point_xform((const P_GR_POINT) xpoint, (const PC_GR_POINT) apoint, xform);
}

#endif /* __gr_coord_h */

/* end of gr_coord.h */
