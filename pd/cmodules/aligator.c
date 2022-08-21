/* aligator.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1991-1998 Colton Software Limited
 * Copyright (C) 1998-2014 R W Colton */

/* Module to allocate blocks of memory, both handle based and pointer based */

/* MRJC December 1991 */

#include "common/gflags.h"

#if defined(STUART) && 0 && WINDOWS
#define WINDOWS_AL_PTR_GLOBALALLOC
#endif

/*
internal functions
*/

_Check_return_
static STATUS
realloc_array(
    _InoutRef_  P_ARRAY_BLOCK p_array_block,
    _InVal_     S32 num_elements,
    _OutRef_    P_ARRAY_INDEX p_array_index);

static U32
tell_full_event_clients(
    _InVal_     U32 bytes_needed);

#if TRACE_ALLOWED && defined(AL_PTR_MAP_TO_HANDLE)

_Check_return_
_Ret_writes_bytes_to_maybenull_(n_bytes, 0) /* may be NULL */
static P_BYTE
al_ptr_alloc_us(
    _InVal_     U32 n_bytes,
    _OutRef_    P_STATUS p_status);

static void
al_ptr_free_us(
    _Pre_maybenull_ _Post_invalid_ P_ANY p_any);

_Check_return_
_Ret_writes_bytes_to_maybenull_(n_bytes, 0) /* may be NULL */
static P_BYTE
al_ptr_realloc_us(
    _Pre_maybenull_ _Post_invalid_ P_ANY p_any,
    _InVal_     U32 n_bytes,
    _OutRef_    P_STATUS p_status);

#else

#define al_ptr_alloc_us(n_bytes, p_status) \
    _al_ptr_alloc(n_bytes, p_status)

#define al_ptr_free_us(p_any) \
    al_ptr_free(p_any)

#define al_ptr_realloc_us(p_any, n_bytes, p_status) \
    _al_ptr_realloc(p_any, n_bytes, p_status)

#endif

#if !defined(ALIGATOR_USE_ALLOC)

#define alloc_free(ptr) \
    free(ptr)

#define alloc_malloc(size) \
    malloc(size)

#define alloc_realloc(ptr, size) \
    realloc(ptr, size)

#endif /* ALIGATOR_USE_ALLOC */

#define ALLOC_SIZE_LIMIT 0x01000000U /* only for debugging */

/*
auto managed arrays
*/

ARRAY_ROOT_BLOCK
array_root = { P_ARRAY_BLOCK_NONE, 0, 0 /* then flags, zeroed*/ };

#define array_block_wr_no_checks(pc_array_handle) \
    de_const_cast(P_ARRAY_BLOCK, array_blockc_no_checks(pc_array_handle))

#if CHECKING
#define array_block_wr(pc_array_handle) \
    de_const_cast(P_ARRAY_BLOCK, _array_block(pc_array_handle))
#else
#endif

_Check_return_
static inline STATUS
realloc_array_root(
    _OutRef_    P_ARRAY_HANDLE p_array_handle)
{
    /* NB ARRAY_ROOT_BLOCK and ARRAY_BLOCK MUST be the same layout */
    assert_EQ(sizeof32(ARRAY_ROOT_BLOCK), sizeof32(ARRAY_BLOCK));
    return(realloc_array((P_ARRAY_BLOCK) &array_root, 1, (P_ARRAY_INDEX) p_array_handle));
}

/*
free block chain
*/

static ARRAY_INDEX next_free_block;

_Check_return_
static STATUS
aligator_fail(
    _InVal_     U32 n_bytes)
{
    IGNOREPARM_InVal_(n_bytes);
    myassert1(TEXT("failed to allocate n_bytes=") U32_XTFMT, n_bytes);
    return(status_nomem());
}

#if TRACE_ALLOWED && (PERSONAL_TRACE_FLAGS & TRACE_MODULE_ALIGATOR)

extern void
aligator_trap(
    _InVal_     ARRAY_HANDLE array_handle);

extern void
aligator_trap(
    _InVal_     ARRAY_HANDLE array_handle)
{
    static ARRAY_HANDLE array_handle_trap;
    array_handle_trap = array_handle;
}

#endif

_Check_return_
extern STATUS
aligator_init(void)
{
    ARRAY_HANDLE array_handle;

    /* initialise array root parameters */
    array_root.parms.auto_compact = 1;
    array_root.parms.compact_off = 0;
    array_root.parms.entry_free = 0;
    array_root.parms.clear_new_block = 0;
#if WINDOWS
    array_root.parms.use_GlobalAlloc = 0;

    array_root.element_size = sizeof32(ARRAY_BLOCK);
    array_root.size_increment = 256;
#else
    array_root.parms.packed_element_size = (UBF) sizeof32(ARRAY_BLOCK);
    array_root.parms.packed_size_increment = 256;
#endif
    trace_1(TRACE_OUT | TRACE_ANY, TEXT("sizeof32(ARRAY_BLOCK) == %d"), sizeof32(ARRAY_BLOCK));

#ifdef ALIGATOR_USE_ALLOC
    status_return(alloc_init());
#endif

    /* SKS initialises the allocator to get an initial handle table else handle 0 accesses crash! */
    if(STATUS_NOMEM == realloc_array_root(&array_handle))
        return(STATUS_NOMEM); /* already flagged */

    { /* preset handle zero so that it has a NULL pointer, zero elements and zero element size */
    P_ARRAY_BLOCK p_array_block = de_const_cast(P_ARRAY_BLOCK, array_root.p_array_block);

    zero_struct_ptr(p_array_block);

#if 1
    p_array_block->p_any = P_DATA_NONE;
#endif
    } /*block*/

    return(STATUS_OK);
}

extern void
aligator_exit(void)
{
#if CHECKING
    status_assert(al_array_handle_check(TRUE));
#endif
}

/******************************************************************************
*
* allocate a block of memory, returning a pointer
*
******************************************************************************/

#if TRACE_ALLOWED && defined(AL_PTR_MAP_TO_HANDLE)

_Check_return_
_Ret_writes_bytes_to_maybenull_(n_bytes, 0) /* may be NULL */
extern P_BYTE
_al_ptr_alloc(
    _InVal_     U32 n_bytes,
    _OutRef_    P_STATUS p_status)
{
    ARRAY_HANDLE array_handle;
    P_ARRAY_HANDLE p_array_handle;

    if(0 == n_bytes)
    {
        *p_status = STATUS_OK;
        return(NULL);
    }

    if(NULL == (p_array_handle = (P_ARRAY_HANDLE) al_array_alloc(&array_handle, BYTE, sizeof32(ARRAY_HANDLE) + n_bytes, &array_init_block_u8, p_status)))
        return(NULL);

    *p_array_handle = array_handle;

    *p_status = STATUS_OK;

    return((P_BYTE) (p_array_handle + 1));
}

_Check_return_
_Ret_writes_bytes_to_maybenull_(n_bytes, 0) /* may be NULL */
static P_BYTE
al_ptr_alloc_us(
    _InVal_     U32 n_bytes,
    _OutRef_    P_STATUS p_status)

#else

_Check_return_
_Ret_writes_bytes_to_maybenull_(n_bytes, 0) /* may be NULL */
extern P_BYTE
_al_ptr_alloc(
    _InVal_     U32 n_bytes,
    _OutRef_    P_STATUS p_status)

#endif
{
    if(0 == n_bytes)
    {
        *p_status = STATUS_OK;
        return(NULL);
    }

    for(;;)
    {

#if defined(WINDOWS_AL_PTR_GLOBALALLOC)

        DWORD dw_bytes = n_bytes + sizeof32(HGLOBAL);
        HGLOBAL hglobal;
        P_BYTE p_data = GlobalAllocAndLock(GMEM_MOVEABLE, dw_bytes, &hglobal);

        if(NULL != p_data)
        {
            HGLOBAL * p_hglobal = (HGLOBAL *) p_data;
            if(NULL != p_hglobal)
            {
                *p_hglobal++ = hglobal;
                *p_status = STATUS_OK;
                return((P_BYTE) p_hglobal);
            }
        }

#else

        P_BYTE p_byte_malloc = alloc_malloc(n_bytes);

        if(NULL != p_byte_malloc)
        {
            *p_status = STATUS_OK;
            return(p_byte_malloc);
        }

#endif

        if(0 == tell_full_event_clients(n_bytes))
            break;
    }

    *p_status = aligator_fail(n_bytes);
    return(NULL);
}

/******************************************************************************
*
* allocate a block of memory, returning a pointer
* but also clearing the contents of the block to NULLCH
*
******************************************************************************/

_Check_return_
_Ret_writes_bytes_maybenull_(n_bytes) /* may be NULL */
extern P_BYTE
_al_ptr_calloc(
    _InVal_     U32 n_bytes,
    _OutRef_    P_STATUS p_status)
{
    P_BYTE p_byte = _al_ptr_alloc(n_bytes, p_status);

    if(NULL != p_byte)
        void_memset32(p_byte, 0, n_bytes);

    return(p_byte);
}

/******************************************************************************
*
* free a block of memory, given a pointer
*
******************************************************************************/

#if TRACE_ALLOWED && defined(AL_PTR_MAP_TO_HANDLE)

extern void
al_ptr_free(
    _Pre_maybenull_ _Post_invalid_ P_ANY p_any)
{
    if(NULL == p_any)
        return;

    {
    P_ARRAY_HANDLE p_array_handle = (P_ARRAY_HANDLE) p_any;
    ARRAY_HANDLE array_handle = p_array_handle[-1];
    al_array_dispose(&array_handle);
    } /*block*/
}

static void
al_ptr_free_us(
    _Pre_maybenull_ _Post_invalid_ P_ANY p_any)

#else

extern void
al_ptr_free(
    _Pre_maybenull_ _Post_invalid_ P_ANY p_any)

#endif
{
    if(NULL == p_any)
        return;

#if defined(WINDOWS_AL_PTR_GLOBALALLOC)

    {
    HGLOBAL * p_hglobal = p_any;
    HGLOBAL hglobal = *--p_hglobal;
    GlobalUnlock(hglobal);
    GlobalFree(hglobal);
    } /*block*/

#else

    alloc_free(p_any);

#endif
}

/******************************************************************************
*
* change the size of an existing block of memory,
* given a pointer to the memory
*
******************************************************************************/

#if TRACE_ALLOWED && defined(AL_PTR_MAP_TO_HANDLE)

_Check_return_
_Ret_writes_bytes_to_maybenull_(n_bytes, 0) /* may be NULL */
extern P_BYTE
_al_ptr_realloc(
    _Pre_maybenull_ _Post_invalid_ P_ANY p_any,
    _InVal_     U32 n_bytes,
    _OutRef_    P_STATUS p_status)
{
    if(NULL == p_any)
        return(_al_ptr_alloc(n_bytes, p_status));

    *p_status = STATUS_OK;

    if(0 == n_bytes)
    {
        al_ptr_free(p_any);
        return(NULL);
    }

    {
    P_ARRAY_HANDLE p_array_handle = (P_ARRAY_HANDLE) p_any;
    ARRAY_HANDLE array_handle = p_array_handle[-1];
    U32 cur_n_bytes;

    cur_n_bytes = (array_elements32(&array_handle) - sizeof32(ARRAY_HANDLE));

    if(cur_n_bytes < n_bytes)
    {   /* grow */
        U32 extend_bytes = n_bytes - cur_n_bytes;

        if(NULL == al_array_extend_by(&array_handle, BYTE, extend_bytes, PC_ARRAY_INIT_BLOCK_NONE, p_status))
            return(NULL);
    }
    else if(cur_n_bytes > n_bytes)
    {   /* shrink */
        S32 shrink_bytes = (S32) (n_bytes - cur_n_bytes);

        al_array_shrink_by(&array_handle, shrink_bytes);
    }

    p_array_handle = array_base(&array_handle, ARRAY_HANDLE);

    return((P_BYTE) (p_array_handle + 1));
    } /*block*/
}

_Check_return_
_Ret_writes_bytes_to_maybenull_(n_bytes, 0) /* may be NULL */
static P_BYTE
al_ptr_realloc_us(
    _Pre_maybenull_ _Post_invalid_ P_ANY p_any,
    _InVal_     U32 n_bytes,
    _OutRef_    P_STATUS p_status)

#else

_Check_return_
_Ret_writes_bytes_to_maybenull_(n_bytes, 0) /* may be NULL */
extern P_BYTE
_al_ptr_realloc(
    _Pre_maybenull_ _Post_invalid_ P_ANY p_any,
    _InVal_     U32 n_bytes,
    _OutRef_    P_STATUS p_status)

#endif
{
    if(NULL == p_any)
        return(al_ptr_alloc_us(n_bytes, p_status));

    if(0 == n_bytes)
    {
        al_ptr_free_us(p_any);
        *p_status = STATUS_OK;
        return(NULL);
    }

#if defined(WINDOWS_AL_PTR_GLOBALALLOC)

    {
    HGLOBAL * p_hglobal = p_any;
    HGLOBAL oldhglobal = *--p_hglobal;

    GlobalUnlock(oldhglobal);

    for(;;)
    {
        DWORD dw_bytes = n_bytes + sizeof32(HGLOBAL);
        HGLOBAL hglobal = GlobalReAlloc(oldhglobal, (DWORD) n_bytes, 0);

        if(NULL != hglobal)
        {
            p_hglobal = (HGLOBAL *) GlobalLock(hglobal);
            *p_hglobal++ = hglobal;
            *p_status = STATUS_OK;
            return((P_BYTE) p_hglobal);
        }

        if(0 == tell_full_event_clients(n_bytes))
            break;
    }

    /* failed to reallocate so better nail the old core back in place (still has old handle) */
    p_hglobal = (HGLOBAL *) GlobalLock(oldhglobal);
    } /*block*/

#else

    for(;;)
    {
        P_BYTE p_byte_realloc = alloc_realloc(p_any, n_bytes);

        if(NULL != p_byte_realloc)
        {
            *p_status = STATUS_OK;
            return(p_byte_realloc);
        }

        if(0 == tell_full_event_clients(n_bytes))
            break;
    }

#endif

    *p_status = aligator_fail(n_bytes);
    return(NULL);
}

/******************************************************************************
*
* reallocate an array and copy a block of data into it
*
******************************************************************************/

_Check_return_
extern STATUS
_al_array_add(
    _InoutRef_  P_ARRAY_HANDLE p_array_handle,
    _InVal_     U32 n_elements,
    _InRef_opt_ PC_ARRAY_INIT_BLOCK p_array_init_block,
    _In_reads_bytes_(bytesof_elem_x_num_elem) PC_ANY p_data_in /*copied*/
    PREFAST_ONLY_ARG(_InVal_ U32 bytesof_elem_x_num_elem))
{
    STATUS status;
    P_ANY p_any = _al_array_extend_by(p_array_handle, n_elements, p_array_init_block, &status PREFAST_ONLY_ARG(bytesof_elem_x_num_elem));

    if(NULL != p_any)
    {
        PC_ARRAY_BLOCK p_array_block = array_blockc_no_checks(p_array_handle);
        const U32 n_bytesof_elem_x_num_elem = n_elements * array_block_element_size(p_array_block);
        PREFAST_ONLY_ASSERT((n_bytesof_elem_x_num_elem == n_bytesof_elem_x_num_elem) || (0 == bytesof_elem_x_num_elem));
        void_memcpy32(p_any, p_data_in, n_bytesof_elem_x_num_elem);
    }

    return(status);
}

/******************************************************************************
*
* allocate an auto-managed array
*
******************************************************************************/

_Check_return_
_Ret_writes_bytes_maybenull_(bytesof_elem_x_num_elem)
extern P_BYTE
_al_array_alloc(
    _OutRef_    P_ARRAY_HANDLE p_array_handle,
    _InVal_     U32 num_elements,
    _InRef_     PC_ARRAY_INIT_BLOCK p_array_init_block,
    _OutRef_    P_STATUS p_status
    PREFAST_ONLY_ARG(_InVal_ U32 bytesof_elem_x_num_elem))
{
    *p_array_handle = 0;

    *p_status = STATUS_OK;

    PREFAST_ONLY_IGNOREPARM_InVal_(bytesof_elem_x_num_elem);

    /* first of all, acquire a handle */
    if(0 != next_free_block)
    {
        P_ARRAY_BLOCK p_array_block;
        *p_array_handle = next_free_block;
        p_array_block = array_block_wr_no_checks(p_array_handle);
        next_free_block = p_array_block->free;
        p_array_block->free = 0;
    }

    if(0 == *p_array_handle)
        if(status_fail(*p_status = realloc_array_root(p_array_handle)))
            return(NULL);

#if TRACE_ALLOWED && (PERSONAL_TRACE_FLAGS & TRACE_MODULE_ALIGATOR)
    switch(*p_array_handle)
    {
    default:
        break;

    case S32_MAX:
        aligator_trap(*p_array_handle);
        break;
    }
#endif

    { /* clear out slot we found */
    P_ARRAY_BLOCK p_array_block = array_block_wr_no_checks(p_array_handle);
    ARRAY_INDEX array_index;
    P_BYTE p_byte;

    p_array_block->p_any = P_DATA_NONE;
    p_array_block->free  = 0;
    p_array_block->size  = 0;

    assert(p_array_init_block != NULL);

    p_array_block->parms.auto_compact     = 0;
    p_array_block->parms.compact_off      = 0;
    p_array_block->parms.entry_free       = 0;
    p_array_block->parms.clear_new_block  = (UBF) p_array_init_block->clear_new_block;
#if WINDOWS
    p_array_block->parms.use_GlobalAlloc  = (UBF) p_array_init_block->use_GlobalAlloc;
#endif
#if WINDOWS
    p_array_block->element_size                 = (0 != p_array_init_block->element_size)   ? p_array_init_block->element_size   : 1;
    p_array_block->size_increment               = (0 != p_array_init_block->size_increment) ? p_array_init_block->size_increment : 1;
#elif RISCOS
    p_array_block->parms.packed_element_size    = (0 != p_array_init_block->element_size)   ? (UBF) p_array_init_block->element_size   : 1;
    p_array_block->parms.packed_size_increment  = (0 != p_array_init_block->size_increment) ? (UBF) p_array_init_block->size_increment : 1;
#endif

    /* now acquire some memory to go with this handle iff non-zero amount requested */
    if(0 == num_elements)
        return((P_BYTE) (uintptr_t) 1); /* handle allocated, but pointer unusable */

    assert(num_elements < 0xF0000000U); /* check possible -ve client */

    if(status_fail(*p_status = realloc_array(p_array_block, num_elements, &array_index /*filled*/)))
    {
        al_array_dispose(p_array_handle);
        return(NULL);
    }

    p_byte = PtrAddBytes(P_BYTE, p_array_block->p_any, (array_index * array_block_element_size(p_array_block)));

    trace_4(TRACE_MODULE_ALLOC, TEXT("al_array_alloc(n:") U32_TFMT TEXT(" es:") U32_TFMT TEXT(") yields *h:") S32_TFMT TEXT(" -> ") PTR_XTFMT,
            num_elements, p_array_init_block->element_size, *p_array_handle, p_byte);

    return(p_byte);
    } /*block*/
}

/******************************************************************************
*
* allocate an auto-managed array with zero initial elements
* useful to allocate the handle and associate with an array_init_block
*
******************************************************************************/

_Check_return_
extern STATUS
al_array_alloc_zero(
    _OutRef_    P_ARRAY_HANDLE p_array_handle,
    _InRef_     PC_ARRAY_INIT_BLOCK p_array_init_block)
{
    STATUS status = STATUS_OK;
    consume_ptr(_al_array_alloc(p_array_handle, 0, p_array_init_block, &status PREFAST_ONLY_ARG(0)));
    return(status);
}

/* similar to above but ensure that we've got some associated core */

_Check_return_
extern STATUS
al_array_preallocate_zero(
    _OutRef_    P_ARRAY_HANDLE p_array_handle,
    _InRef_     PC_ARRAY_INIT_BLOCK p_array_init_block)
{
    STATUS status = STATUS_OK;
    if(NULL != _al_array_alloc(p_array_handle, 1, p_array_init_block, &status PREFAST_ONLY_ARG(1 * p_array_init_block->element_size)))
        al_array_empty(p_array_handle);
    return(status);
}

/******************************************************************************
*
* set auto compact attribute on array
*
******************************************************************************/

extern void
al_array_auto_compact_set(
    _InRef_     PC_ARRAY_HANDLE p_array_handle)
{
    if(!array_handle_valid(p_array_handle))
    {
        myassert3(TEXT("al_array_auto_compact_set(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
        return;
    }

    if(*p_array_handle)
    {
        P_ARRAY_BLOCK p_array_block = array_block_wr_no_checks(p_array_handle);

        p_array_block->parms.auto_compact = 1;
    }
}

#if defined(UNUSED_IN_PD)

_Check_return_
extern ARRAY_INDEX
_al_array_bfind(
    _In_reads_bytes_(bytesof_elem) PC_ANY key,
    _InRef_     PC_ARRAY_HANDLE p_array_handle,
    _In_        P_PROC_BSEARCH p_proc_bsearch,
    _OutRef_    P_BOOL p_hit
    PREFAST_ONLY_ARG(_InVal_ U32 bytesof_elem))
{
    U32 array_index = 0; /* for simple insert_before at start if !*p_hit */

    *p_hit = FALSE;

    if(!array_handle_valid(p_array_handle))
    {
        assert0();
        return(0);
    }

    if(*p_array_handle)
    {
        const U32 array_element_size = array_element_size32_no_checks(p_array_handle);
        P_ANY p_any;

        PREFAST_ONLY_ASSERT(bytesof_elem == array_element_size);

        p_any =
            _bfind(key,
                   array_basec_no_checks(p_array_handle, void),
                   array_elements32_no_checks(p_array_handle),
                   array_element_size,
                   p_proc_bsearch,
                   p_hit);

        if(NULL != p_any)
        {
            array_index = PtrDiffBytesU32(p_any, array_basec_no_checks(p_array_handle, void));
            if((0 != array_index) && (array_element_size > 1))
                array_index /= array_element_size;
        }
    }

    return((ARRAY_INDEX) array_index);
}

#endif /* UNUSED_IN_PD */

_Check_return_
_Ret_writes_bytes_maybenull_(bytesof_elem)
extern P_ANY
_al_array_bsearch(
    _In_        PC_ANY key,
    _InRef_     PC_ARRAY_HANDLE p_array_handle,
    _In_        P_PROC_BSEARCH p_proc_bsearch
    PREFAST_ONLY_ARG(_InVal_ U32 bytesof_elem))
{
    P_ANY p_any = P_DATA_NONE;

    if(!array_handle_valid(p_array_handle))
    {
        assert0();
        return(P_DATA_NONE);
    }

    if(*p_array_handle)
    {
        PREFAST_ONLY_ASSERT(bytesof_elem == array_element_size32_no_checks(p_array_handle));

        p_any =
            bsearch(key,
                    array_basec_no_checks(p_array_handle, void),
                    array_elements32_no_checks(p_array_handle),
                    array_element_size32_no_checks(p_array_handle),
                    p_proc_bsearch);

        if(NULL == p_any)
            p_any = P_DATA_NONE;
    }

    return(p_any);
}

#if defined(UNUSED_IN_PD)

_Check_return_
_Ret_writes_bytes_maybenull_(bytesof_elem)
extern P_ANY
_al_array_lsearch(
    _In_        PC_ANY key,
    _InRef_     P_ARRAY_HANDLE p_array_handle,
    _In_        P_PROC_BSEARCH p_proc_bsearch
    PREFAST_ONLY_ARG(_InVal_ U32 bytesof_elem))
{
    P_ANY p_any = P_DATA_NONE;

    if(!array_handle_valid(p_array_handle))
    {
        assert0();
        return(P_DATA_NONE);
    }

    if(*p_array_handle)
    {
        p_any =
            _lsearch(key,
                     array_basec_no_checks(p_array_handle, void),
                     array_elements32_no_checks(p_array_handle),
                     array_element_size32_no_checks(p_array_handle),
                     p_proc_bsearch);

        if(NULL == p_any)
            p_any = P_DATA_NONE;
    }

    return(p_any);
}

#endif /* UNUSED_IN_PD */

/******************************************************************************
*
* duplicate an array
*
******************************************************************************/

_Check_return_
extern STATUS
al_array_duplicate(
    _OutRef_    P_ARRAY_HANDLE p_dup_array_handle,
    _InRef_     PC_ARRAY_HANDLE pc_src_array_handle)
{
    STATUS status;
    ARRAY_INIT_BLOCK array_init_block;
    S32 n_elements;
    P_ANY p_any;
    PC_ARRAY_BLOCK pc_src_array_block;
    BOOL src_auto_compact;

    *p_dup_array_handle = 0;

    if(0 == *pc_src_array_handle)
        return(STATUS_OK);

    if(!array_handle_valid(pc_src_array_handle))
    {
        myassert4(TEXT("al_array_duplicate(") PTR_XTFMT TEXT(", ") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_dup_array_handle, pc_src_array_handle, *pc_src_array_handle, array_root.free);
        return(status_check());
    }

    n_elements = array_elements_no_checks(pc_src_array_handle);
    pc_src_array_block = array_blockc_no_checks(pc_src_array_handle);

    src_auto_compact = pc_src_array_block->parms.auto_compact;

    array_init_block.size_increment   = array_block_size_increment(pc_src_array_block);
    array_init_block.element_size     = array_block_element_size(pc_src_array_block);
    array_init_block.clear_new_block  = UBF_UNPACK(U8, pc_src_array_block->parms.clear_new_block);
#if WINDOWS
    array_init_block.use_GlobalAlloc  = UBF_UNPACK(U8, pc_src_array_block->parms.use_GlobalAlloc);
#endif

    p_any = _al_array_alloc(p_dup_array_handle, n_elements, &array_init_block, &status PREFAST_ONLY_ARG(n_elements * array_init_block.element_size));

    /* NB can have handles with no memory */
    if(0 == *p_dup_array_handle)
        return(status);

    if(p_any && n_elements)
    {
        const U32 n_bytes = n_elements * array_init_block.element_size;
        PC_BYTE p_src = array_basec_no_checks(pc_src_array_handle, BYTE);
        void_memcpy32(p_any, p_src, n_bytes);
    }

    if(src_auto_compact)
        al_array_auto_compact_set(p_dup_array_handle);

    return(status);
}

/******************************************************************************
*
* clear down (to zero elements) but don't free the array
*
******************************************************************************/

extern void
al_array_empty(
    _InRef_     PC_ARRAY_HANDLE p_array_handle)
{
    ARRAY_INDEX n_elements = array_elements(p_array_handle);

    if(n_elements)
        al_array_shrink_by(p_array_handle, -n_elements);
}

/******************************************************************************
*
* release an array handle and all associated with it
*
******************************************************************************/

extern void
__al_array_free(
    _InVal_     ARRAY_HANDLE array_handle)
{
    P_ARRAY_BLOCK p_array_block;

#if TRACE_ALLOWED && (PERSONAL_TRACE_FLAGS & TRACE_MODULE_ALIGATOR)
    switch(array_handle)
    {
    case S32_MAX:
        aligator_trap(array_handle);
        break;

    default:
        break;
    }
#endif

    if((U32) array_handle >= (U32) array_root.free)
    {
        myassert2(TEXT("al_array_free(h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, array_handle, array_root.free);
        return;
    }

    p_array_block = array_block_wr_no_checks(&array_handle);

#if CHECKING
    if(p_array_block->parms.entry_free)
    {
        myassert1(TEXT("al_array_free(h:") S32_TFMT TEXT(" --- handle has already been freed"), array_handle);
        return;
    }
#endif

    /* free the array itself */
    if(P_DATA_NONE != p_array_block->p_any)
    {
#if WINDOWS
        if(p_array_block->parms.use_GlobalAlloc)
        {
            HGLOBAL hglobal = GlobalPtrHandle(p_array_block->p_any);
            assert(hglobal);
            if(NULL != hglobal)
            {
                GlobalUnlock(hglobal);
                GlobalFree(hglobal);
            }
        }
        else
#endif
            al_ptr_free_us(p_array_block->p_any);

        p_array_block->p_any = P_DATA_NONE;
    }

#if CHECKING
    zero_struct_ptr(p_array_block);

    /* ensure subsequent references go sprong (and tells us which handle ref'd too!) */
    p_array_block->p_any = BAD_POINTER_X(P_ANY, array_handle);
#endif

    /* mark entry re-usable */
    p_array_block->parms.entry_free = 1;

#if TRACE_ALLOWED && (PERSONAL_TRACE_FLAGS & TRACE_MODULE_ALIGATOR) && defined(ALIGATOR_HANDLES_MONOTONIC)
    /* handle debugging uses monotonically increasing handle numbers */
#else
    /* add to free list */
    p_array_block->free = next_free_block;
    next_free_block = array_handle;
#endif

    trace_1(TRACE_MODULE_ALLOC, TEXT("al_array_free(h:") S32_TFMT TEXT(") freed"), array_handle);
}

/******************************************************************************
*
* remove deleted entries from an array
*
* calls the client back to find out
* whether a given element is deleted
*
******************************************************************************/

/*ncr*/
extern S32 /* number of elements remaining */
al_array_garbage_collect(
    _InoutRef_  P_ARRAY_HANDLE p_array_handle,
    _InVal_     ARRAY_INDEX element_start,
    _In_opt_    P_PROC_ELEMENT_DELETED p_proc_element_deleted,
    _InVal_     AL_GARBAGE_FLAGS al_garbage_flags)
{
    if(!array_handle_valid(p_array_handle))
    {
        myassert3(TEXT("al_array_garbage_collect(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
        return(0);
    }

    if(*p_array_handle)
    {
        P_ARRAY_BLOCK p_array_block = array_block_wr_no_checks(p_array_handle);

        if(p_proc_element_deleted && al_garbage_flags.remove_deleted)
        {
            U32 element_size = array_block_element_size(p_array_block);
            ARRAY_INDEX new_free = element_start; /* size we'd be if all after element_start got trimmed off */
            P_U8 p_out = (P_U8) p_array_block->p_any + (element_start * element_size);
            P_U8 p_in = p_out;
            ARRAY_INDEX i;

            assert(element_start <= p_array_block->free);

            /* move all non-deleted elements towards the start of the array */
            for(i = new_free; i < p_array_block->free; ++i)
            {
                if(0 == (* p_proc_element_deleted) (p_in))
                {
                    if(p_out != p_in)
                        void_memcpy32(p_out, p_in, element_size);

                    p_out += element_size;
                    new_free += 1;
                }

                p_in += element_size;
            }

            p_array_block->free = new_free; /* SKS 18sep85 makes more robust (old code did ptr sub and division) */
        }

        /* client wants us to try to dispose of the array? */
        if(al_garbage_flags.may_dispose && (0 == p_array_block->free))
            al_array_dispose(p_array_handle);
        /* client wants us to try to shrink the array? */
        else if(al_garbage_flags.shrink && (p_array_block->free < p_array_block->size))
        {
#if WINDOWS
            if(p_array_block->parms.use_GlobalAlloc)
            {
                /* currently a NO OP */
            }
            else /* SKS 18.01.2012 was missing! */
#endif /* WINDOWS */
            {
                STATUS status;
                p_array_block->p_any = al_ptr_realloc_us(p_array_block->p_any, p_array_block->free * array_block_element_size(p_array_block), &status);
                p_array_block->size = p_array_block->free;
                IGNOREPARM(status);
            }
        }
    }

    return(array_elements(p_array_handle));
}

#if CHECKING

/******************************************************************************
*
* check for storage leaks
*
******************************************************************************/

_Check_return_
extern S32 /* number of allocated handles */
al_array_handle_check(
    _InVal_     S32 winge /* winge about allocated handles */)
{
    ARRAY_INDEX count = 0;
    UINT pass;

    IGNOREPARM_InVal_(winge);

    for(pass = 1; pass <= 2; ++pass)
    {
        U32 i;
        PC_ARRAY_BLOCK p_array_block;

        for(i = 1, p_array_block = array_root.p_array_block + i; i < array_root.free; ++i, ++p_array_block)
        {
            if(p_array_block->parms.entry_free)
                continue;

#if CHECKING && defined(AL_PTR_MAP_TO_HANDLE) && 1
            switch(i)
            {
            case 14: /* stop Neil and date_init hiding errors */
                if(PRODUCT_ID_FPROWORKZ == g_product_id)
                    break;

            case S32_MAX:
                trace_1(TRACE_OUT | TRACE_ANY, TEXT("handle ") S32_TFMT TEXT(" not freed (but known about)"), i);
                continue;

            default:
                break;
            }
#endif

            if(pass == 1)
                count += 1;
            else
            {
                trace_1(TRACE_OUT | TRACE_ANY, TEXT("handle ") S32_TFMT TEXT(" not freed"), i);
                /*myassert1x(!winge, TEXT("al_array_handle_check handle ") S32_TFMT TEXT(" not freed"), i);*/
            }
        }

        if(count == 0)
            break;

        if(pass == 1)
        {
            trace_1(TRACE_OUT | TRACE_ANY, S32_TFMT TEXT(" handles not freed"), count);

#if 0 /* <<< enable this to debug which handles have gone walkabout */
            if(!__myasserted_msg(TEXT("al_array_handle_check"), TEXT(__FILE__), __LINE__, TEXT("OK to list, Cancel to continue."), S32_TFMT TEXT(" handles not freed"), count))
                break;
#endif
        }
    }

    return(count);
}

#endif /* CHECKING */

extern void
al_array_delete_at(
    _InRef_     P_ARRAY_HANDLE p_array_handle,
    _InVal_     S32 num_elements, /* NB num_elements still -ve */
    _InVal_     ARRAY_INDEX delete_at)
{
    P_ARRAY_BLOCK p_array_block;
    S32 cur_elements;
    P_ANY p_any;

#if CHECKING
    if(!array_handle_valid(p_array_handle) && (0 != *p_array_handle))
    {
        myassert3(TEXT("al_array_delete_at(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
        return;
    }
#endif

    p_array_block = array_block_wr_no_checks(p_array_handle);

    assert(num_elements <= 0);

    /* NB. don't use delete with 0 elements to allocate a handle without memory - use alloc. */
    if(0 == num_elements)
        return;

#if CHECKING
    if(p_array_block->parms.entry_free)
    {
        myassert2(TEXT("al_array_delete_at(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle has been freed"), p_array_handle, *p_array_handle);
        return;
    }
#endif

    cur_elements = array_elements(p_array_handle);

    if(0 == cur_elements)
    {
        assert(cur_elements != 0);
        return;
    }

    assert(delete_at >= 0);
    assert(delete_at <= cur_elements);

    if(num_elements < 0)
    {
        /* move core down before realloc trims it off (optimise out trimming off end elements) */
        S32 delete_elements = - num_elements;
        S32 move_elements = cur_elements - (delete_at + delete_elements);

        if(move_elements) /* copy current contents down */
        {
            const U32 element_size = array_block_element_size(p_array_block);
            P_ANY p_dst;

            p_dst = (P_U8) p_array_block->p_any + (delete_at * element_size);
            p_any = (P_U8) p_dst + (delete_elements * element_size);

            void_memmove32(p_dst, p_any, move_elements * element_size);
        }

        assert(p_array_block->free + num_elements >= 0); /* another case of shrinking realloc */
        p_array_block->free += num_elements;
    }
}

_Check_return_
_Ret_writes_bytes_maybenull_(bytesof_elem_x_num_elem)
extern P_BYTE
_al_array_insert_before(
    _InoutRef_  P_ARRAY_HANDLE p_array_handle,
    _InVal_     S32 num_elements,
    _InRef_opt_ PC_ARRAY_INIT_BLOCK p_array_init_block,
    _OutRef_    P_STATUS p_status,
    _InVal_     ARRAY_INDEX insert_before
    PREFAST_ONLY_ARG(_InVal_ U32 bytesof_elem_x_num_elem))
{
    P_ARRAY_BLOCK p_array_block;
    S32 cur_elements;

    *p_status = STATUS_OK;

#if CHECKING
    if(!array_handle_valid(p_array_handle) && (0 != *p_array_handle))
    {
        myassert3(TEXT("al_array_insert_before(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
        *p_status = status_check();
        return(NULL);
    }
#endif

    p_array_block = array_block_wr_no_checks(p_array_handle);

    /* NB. don't use insert with 0 elements to allocate a handle without memory - use alloc. this realloc simply returns the current pointer, if any */
    if(0 == num_elements)
        return(p_array_block->p_any);

#if CHECKING
    if(p_array_block->parms.entry_free)
    {
        myassert2(TEXT("al_array_insert_before(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle has been freed"), p_array_handle, *p_array_handle);
        *p_status = status_check();
        return(NULL);
    }
#endif

    if(num_elements < 0)
    {
        assert(num_elements > 0); /* but warn about vvv */
        al_array_delete_at(p_array_handle, num_elements, insert_before /*delete_at*/); /* old compatibility */
        return(NULL);
    }

    cur_elements = array_elements(p_array_handle);

    if(0 == cur_elements)
    {
        assert(insert_before == 0);
        /* use realloc: there may be a handle but no elements */
        return(_al_array_extend_by(p_array_handle, num_elements, p_array_init_block, p_status PREFAST_ONLY_ARG(bytesof_elem_x_num_elem)));
    }

    assert(insert_before >= 0);
    assert(insert_before <= cur_elements);

    { /* actually insert at the specified place and return a pointer to that (optimise out if adding to end) */
    const U32 element_size = array_block_element_size(p_array_block);
    S32 move_elements = cur_elements - insert_before;
    ARRAY_INDEX dummy_array_index;
    P_BYTE p_byte;

    if(status_fail(*p_status = realloc_array(p_array_block, num_elements, &dummy_array_index))) /* don't want the result of the realloc here */
        return(NULL);

    p_byte = PtrAddBytes(P_BYTE, p_array_block->p_any, (insert_before * element_size));

    if(move_elements) /* copy current contents up */
    {
        P_BYTE p_byte_dst = p_byte + (num_elements * element_size);

        void_memmove32(p_byte_dst, p_byte, move_elements * element_size);

        if(p_array_block->parms.clear_new_block)
        {
            U32 n_bytes = num_elements * element_size;
            void_memset32(p_byte, 0, n_bytes);
        }
    }

    return(p_byte);
    } /*block*/
}

extern void
al_array_qsort(
    _InRef_     PC_ARRAY_HANDLE p_array_handle,
    _In_        P_PROC_QSORT p_proc_compare_qsort)
{
    U32 n_elements = array_elements32(p_array_handle);

    if(0 != n_elements)
        qsort(
            array_base_no_checks(p_array_handle, void),
            n_elements,
            array_element_size32_no_checks(p_array_handle),
            p_proc_compare_qsort);
}

#if defined(UNUSED_IN_PD)

extern void /* declared as qsort replacement */
al_array_check_sorted(
    _InRef_     PC_ARRAY_HANDLE p_array_handle,
    _In_        P_PROC_QSORT p_proc_compare_qsort)
{
    const U32 n_elements = array_elements32(p_array_handle);

    if(n_elements > 1)
        check_sorted(
            array_base_no_checks(p_array_handle, void),
            n_elements,
            array_element_size32_no_checks(p_array_handle),
            p_proc_compare_qsort);
}

#endif /* UNUSED_IN_PD */

/******************************************************************************
*
* reallocate the size of an array - now split into distinct extend / shrink procs
*
******************************************************************************/

_Check_return_
_Ret_writes_bytes_maybenull_(bytesof_elem_x_num_elem)
extern P_BYTE
_al_array_extend_by(
    _InoutRef_  P_ARRAY_HANDLE p_array_handle,
    _InVal_     U32 add_elements,
    _InRef_opt_ PC_ARRAY_INIT_BLOCK p_array_init_block,
    _OutRef_    P_STATUS p_status
    PREFAST_ONLY_ARG(_InVal_ U32 bytesof_elem_x_num_elem))
{
    P_ARRAY_BLOCK p_array_block;

    *p_status = STATUS_OK;

    assert(add_elements < 0xF0000000U); /* check possible -ve client */

    /* NB. don't use realloc with 0 elements to allocate a handle without memory - use alloc. this realloc simply returns the current pointer, if any */
    if(0 == add_elements)
    {
        if(!array_handle_valid(p_array_handle))
        {
            myassert3(TEXT("al_array_extend_by(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
            *p_status = status_check();
            return(NULL);
        }

        return(array_blockc_no_checks(p_array_handle)->p_any);
    }

    if(0 == *p_array_handle)
    {
        assert(!_IS_P_DATA_NONE(PC_ARRAY_INIT_BLOCK, p_array_init_block));
        return(_al_array_alloc(p_array_handle, add_elements, p_array_init_block, p_status PREFAST_ONLY_ARG(bytesof_elem_x_num_elem)));
    }

#if CHECKING
    if(!array_handle_valid(p_array_handle) && (0 != *p_array_handle))
    {
        myassert3(TEXT("al_array_extend_by(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
        *p_status = status_check();
        return(NULL);
    }
#endif

    p_array_block = array_block_wr_no_checks(p_array_handle);

#if CHECKING
    if(p_array_block->parms.entry_free)
    {
        myassert2(TEXT("al_array_extend_by(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle has been freed"), p_array_handle, *p_array_handle);
        *p_status = status_check();
        return(NULL);
    }
#endif

    {
    ARRAY_INDEX array_index;
    P_BYTE p_byte;

    if(status_fail(*p_status = realloc_array(p_array_block, add_elements, &array_index)))
        return(NULL);

    p_byte = PtrAddBytes(P_BYTE, p_array_block->p_any, (array_index * array_block_element_size(p_array_block)));

    return(p_byte);
    } /*block*/
}

extern void
al_array_shrink_by(
    _InRef_     PC_ARRAY_HANDLE p_array_handle,
    _InVal_     S32 num_elements)
{
    P_ARRAY_BLOCK p_array_block;

    if(0 == *p_array_handle)
        return;

#if CHECKING
    if(!array_handle_valid(p_array_handle))
    {
        myassert3(TEXT("al_array_shrink_by(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
        return;
    }
#endif

    p_array_block = array_block_wr_no_checks(p_array_handle);

#if CHECKING
    if(p_array_block->parms.entry_free)
    {
        myassert2(TEXT("al_array_shrink_by(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle has been freed"), p_array_handle, *p_array_handle);
        return;
    }
#endif

    /* note that realloc_shrink_by never explicitly frees a handle as it is valid
     * to have a handle to 0 elements
     * >>the handle won't be freed even if you set the auto-compact bit
     */
    assert(num_elements <= 0);

    if(num_elements < 0)
    {
        /* SKS moved only case of shrinking realloc here 23apr93 and assert() 07jul93 */
        assert(p_array_block->free + num_elements >= 0);
        p_array_block->free += num_elements;
    }
}

#if WINDOWS

extern void
al_array_resized(
    _InRef_     P_ARRAY_HANDLE p_array_handle,
    _In_        HGLOBAL hglobal,
    _InVal_     U32 size)
{
    P_ARRAY_BLOCK p_array_block;

    if(!array_handle_valid(p_array_handle))
    {
        myassert3(TEXT("al_array_resized(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
        return;
    }

    p_array_block = array_block_wr_no_checks(p_array_handle);

    p_array_block->p_any = GlobalLock(hglobal);
    p_array_block->free = size;
    p_array_block->size = size;
}

#endif /* WINDOWS */

#if WINDOWS

_Check_return_
_Ret_maybenull_
extern HGLOBAL
al_array_steal_hglobal(
    _InoutRef_      P_ARRAY_HANDLE p_array_handle)
{
    HGLOBAL hglobal = 0;

    if(*p_array_handle)
    {
        P_ARRAY_BLOCK p_array_block;

        if(!array_handle_valid(p_array_handle))
        {
            myassert3(TEXT("al_array_steal_hglobal(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
            return(0);
        }

        p_array_block = array_block_wr_no_checks(p_array_handle);

#if CHECKING
        if(p_array_block->parms.entry_free)
        {
            myassert2(TEXT("al_array_steal_hglobal(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle has already been freed"), p_array_handle, *p_array_handle);
            return(0);
        }
#endif

        /* reduce size of allocated object to final size (hopefully never zero) */
        al_array_trim(p_array_handle);

        if(p_array_block->parms.use_GlobalAlloc)
        {
            hglobal = GlobalPtrHandle(p_array_block->p_any);
            assert(hglobal);
            if(NULL != hglobal)
                GlobalUnlock(hglobal);

            /* mark the handle as stolen */
            p_array_block->p_any = P_DATA_NONE;
            p_array_block->parms.use_GlobalAlloc = 0;
        }

        /* free the array itself */
        __al_array_free(*p_array_handle);

        trace_2(TRACE_MODULE_ALLOC, TEXT("al_array_steal_hglobal hglobal:") PTR_XTFMT TEXT(" stolen, handle:") S32_TFMT TEXT(" freed"), hglobal, *p_array_handle);

        *p_array_handle = 0;
    }

    return(hglobal);
}

#endif /* WINDOWS */

extern void
al_array_trim(
    _InRef_     PC_ARRAY_HANDLE p_array_handle)
{
    P_ARRAY_BLOCK p_array_block;

    if(!array_handle_valid(p_array_handle))
    {
        myassert3(TEXT("al_array_trim(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, p_array_handle, *p_array_handle, array_root.free);
        return;
    }

    p_array_block = array_block_wr_no_checks(p_array_handle);

    if(p_array_block->size != p_array_block->free)
    {
        /* shrink allocation to fit desired size */
#if WINDOWS
        if(p_array_block->parms.use_GlobalAlloc)
        {
            HGLOBAL hglobal = GlobalPtrHandle(p_array_block->p_any);
            assert(hglobal);
            if(NULL != hglobal)
            {
                GlobalUnlock(hglobal);

                hglobal = GlobalReAlloc(hglobal, p_array_block->free * array_block_element_size(p_array_block), GMEM_MOVEABLE);

                assert(hglobal);
                if(NULL != hglobal)
                    p_array_block->p_any = GlobalLock(hglobal);
            }
        }
        else
#endif /* WINDOWS */
        {
            STATUS status;
            p_array_block->p_any = al_ptr_realloc_us(p_array_block->p_any, p_array_block->free * array_block_element_size(p_array_block), &status);
            IGNOREPARM(status);
        }

        p_array_block->size = p_array_block->free;
    }
}

#if WINDOWS

_Check_return_
_Ret_writes_bytes_to_maybenull_(dwBytes, 0) /* may be NULL */
extern P_BYTE
GlobalAllocAndLock(
    _InVal_     UINT uFlags,
    _InVal_     U32 dwBytes,
    _Out_       HGLOBAL * const p_hGlobal)
{
    HGLOBAL hGlobal = GlobalAlloc(uFlags, dwBytes);

    *p_hGlobal = hGlobal;

    if(NULL == hGlobal)
    {
        void_WrapOsBoolChecking(NULL != hGlobal);
        return(NULL);
    }

    return((P_BYTE) GlobalLock(hGlobal));
}

#endif /* WINDOWS */

/*
full event clients
*/

static ARRAY_HANDLE h_full_clients;

typedef struct _full_event_client
{
    P_PROC_AL_FULL_EVENT proc;
}
FULL_EVENT_CLIENT, * P_FULL_EVENT_CLIENT;

/******************************************************************************
*
* register a full event client
*
******************************************************************************/

_Check_return_
extern STATUS
al_full_event_client_register(
    _In_        P_PROC_AL_FULL_EVENT proc)
{
    FULL_EVENT_CLIENT full_event_client;
    SC_ARRAY_INIT_BLOCK array_init_block = aib_init(1, sizeof32(FULL_EVENT_CLIENT), TRUE);

    /* save routine address */
    full_event_client.proc = proc;

    return(al_array_add(&h_full_clients, FULL_EVENT_CLIENT, 1, &array_init_block, &full_event_client));
}

#if TRACE_ALLOWED

extern void
al_list_big_handles(
    _InVal_     U32 over_n_bytes)
{
    U32 i;
    PC_ARRAY_BLOCK p_array_block;

    for(i = 1, p_array_block = array_root.p_array_block + i; i < array_root.free; ++i, ++p_array_block)
    {
        if(p_array_block->parms.entry_free)
            continue;

        if(p_array_block->size * array_block_element_size(p_array_block) <= over_n_bytes)
            continue;

        trace_4(TRACE_APP_MEMORY_USE | TRACE_MODULE_ALLOC,
                TEXT("handle:") S32_TFMT TEXT(", ") S32_TFMT TEXT(" entries, used bytes: ") S32_TFMT TEXT(", total_bytes: ") S32_TFMT,
                i,
                p_array_block->free,
                p_array_block->free * array_block_element_size(p_array_block),
                p_array_block->size * array_block_element_size(p_array_block));
    }
}

#endif

/******************************************************************************
*
* reallocate an array block
* SKS 23apr93 made it only grow
* SKS 16sep95 made it return index not pointer to save divides
*
******************************************************************************/

_Check_return_
static STATUS
realloc_array(
    _InoutRef_  P_ARRAY_BLOCK p_array_block,
    _InVal_     S32 num_elements,
    _OutRef_    P_ARRAY_INDEX p_array_index)
{
    S32 cur_free = p_array_block->free; /* saves reload */
    STATUS status = STATUS_OK;

    *p_array_index = 0;

    assert(num_elements > 0);

    trace_2(TRACE_MODULE_ALLOC,
            TEXT("array_realloc p_array_block:") PTR_XTFMT TEXT(", num_elements:") S32_TFMT,
            report_ptr_cast(p_array_block), num_elements);

    if(num_elements > p_array_block->size - cur_free)
    {
        S32 extra = MAX(num_elements, (S32) array_block_size_increment(p_array_block));
        P_ANY p_new_array;

        /* prevent auto compaction compacting us */
        p_array_block->parms.compact_off = 1;

#if WINDOWS
        if(p_array_block->parms.use_GlobalAlloc)
        {
            DWORD n_bytes = ((DWORD) p_array_block->size + extra) * array_block_element_size(p_array_block);
            HGLOBAL hglobal = NULL;

#if CHECKING
            if(IS_BAD_POINTER(p_array_block->p_any)) /* might be reusing debug mutilated one */
                p_array_block->p_any = P_DATA_NONE; /* restore sanity for clarity */
#endif

            if(P_DATA_NONE == p_array_block->p_any)
            {
                /* first time allocation */
                for(;;)
                {
                    if(NULL != (p_new_array = GlobalAllocAndLock(GMEM_MOVEABLE, n_bytes, &hglobal)))
                        break;

                    if(0 == tell_full_event_clients(n_bytes))
                        break;
                }
            }
            else
            {
                /* resize some already-allocated core */
                HGLOBAL oldhglobal = GlobalPtrHandle(p_array_block->p_any);
                assert(oldhglobal);
                if(NULL != oldhglobal)
                {
                    GlobalUnlock(oldhglobal);

                    for(;;)
                    {
                        if(NULL != (hglobal = GlobalReAlloc(oldhglobal, n_bytes, 0)))
                            break;

                        if(0 == tell_full_event_clients(n_bytes))
                            break;
                    }

                    if(NULL == hglobal)
                        /* failed to reallocate so better nail the old core back in place (still has old handle) */
                        p_array_block->p_any = GlobalLock(oldhglobal);
                }

                if(NULL != hglobal)
                    p_new_array = GlobalLock(hglobal);
                else
                    p_new_array = NULL;
            }
        }
        else
#endif /* WINDOWS */
        {
            U32 n_bytes =  (p_array_block->size + extra) * array_block_element_size(p_array_block);

#if CHECKING
            if(IS_BAD_POINTER(p_array_block->p_any)) /* might be reusing debug mutilated one */
                p_array_block->p_any = P_DATA_NONE; /* restore sanity for clarity */
#endif

            if(P_DATA_NONE == p_array_block->p_any)
                p_new_array = al_ptr_alloc_us(n_bytes, &status);
            else
                p_new_array = al_ptr_realloc_us(p_array_block->p_any, n_bytes, &status);
        }

        p_array_block->parms.compact_off = 0;

        if(NULL == p_new_array)
            return(status);

        p_array_block->size += extra;
        p_array_block->p_any = p_new_array;
    }

    p_array_block->free = cur_free + num_elements;

    /* zero contents of newly allocated part of block if wanted */
    if(p_array_block->parms.clear_new_block)
    {
        P_ANY p_any = (P_U8) p_array_block->p_any + (cur_free * array_block_element_size(p_array_block));
        const U32 n_bytes = num_elements * array_block_element_size(p_array_block);
        void_memset32(p_any, 0, n_bytes);
    }

    *p_array_index = cur_free;
    return(STATUS_OK);
}

/******************************************************************************
*
* tell all our customers that memory is full; give some back
*
******************************************************************************/

static U32
tell_full_event_clients(
    _InVal_     U32 bytes_needed)
{
    static U32 next_array_client = 0; /* next array to be asked */
    static U32 next_client = 0;       /* next client to be asked */

    U32 freed = 0;

    trace_1(TRACE_MODULE_ALLOC, TEXT("******* tell_full_event_clients told: ") U32_TFMT TEXT(" bytes needed"), bytes_needed);

    { /* try auto compact phase to reduce allocations */
    U32 count;

    for(count = 0; count < array_root.free; ++count, ++next_array_client)
    {
        P_ARRAY_BLOCK p_array_block;
        STATUS status;

        if(next_array_client >= array_root.free)
            /* wrap around to start */
            next_array_client = 0;

        p_array_block = de_const_cast(P_ARRAY_BLOCK, array_root.p_array_block) + next_array_client;

#if WINDOWS
        if(p_array_block->parms.use_GlobalAlloc)
            /* currently ignore HGLOBAL blocks 'cos their memory ain't as useful to us */
            continue;
#endif

        if(p_array_block->free == p_array_block->size)
            /* allocation fully used */
            continue;

        if(!p_array_block->parms.auto_compact)
            continue;

        if(p_array_block->parms.compact_off)
            /* temporarily locked against compaction */
            continue;

        /* actually do the shrink */
        freed += (p_array_block->size - p_array_block->free) * array_block_element_size(p_array_block);
        p_array_block->p_any = al_ptr_realloc_us(p_array_block->p_any, p_array_block->free * array_block_element_size(p_array_block), &status);
        p_array_block->size = p_array_block->free;

        if(freed >= bytes_needed)
        {
            trace_2(TRACE_MODULE_ALLOC, TEXT("******* tell_full_event_clients (auto compact) bytes_needed: ") U32_TFMT TEXT(", freed: ") U32_TFMT TEXT(" bytes"), bytes_needed, freed);
            return(freed);
        }
    }
    } /*block*/

    { /* try calling clients to reduce allocations */
    U32 n_clients = array_elements32(&h_full_clients);
    U32 count;

    for(count = 0; count < n_clients; ++count, ++next_client)
    {
        P_FULL_EVENT_CLIENT p_full_event_client;
        S32 it_freed;

        if(next_client >= n_clients)
            /* wrap around to start */
            next_client = 0;

        assert(array_handle_valid(&h_full_clients));
        p_full_event_client = array_ptr_no_checks(&h_full_clients, FULL_EVENT_CLIENT, next_client);
        assert(p_full_event_client);
        it_freed = (* p_full_event_client->proc) (bytes_needed - freed);

        trace_3(TRACE_MODULE_ALLOC, TEXT("******* called full event client ") S32_TFMT TEXT(" to release: ") U32_TFMT TEXT(" bytes, ") U32_TFMT TEXT(" bytes freed"), next_client, bytes_needed, it_freed);

        freed += it_freed;

        if(freed >= bytes_needed)
        {
            trace_2(TRACE_MODULE_ALLOC, TEXT("******* tell_full_event_clients (client compact) bytes_needed: ") U32_TFMT TEXT(", freed: ") U32_TFMT TEXT(" bytes"), bytes_needed, freed);
            return(freed);
        }
    }
    } /*block*/

    /* didn't get all we requested but it's probably worth the caller trying his alloc again */
    trace_1(TRACE_MODULE_ALLOC, TEXT("******* tell_full_event_clients freed: ") U32_TFMT TEXT(" bytes"), freed);
    return(freed);
}

#if CHECKING

_Check_return_
_Ret_ /*opt_*/
extern P_ANY
_array_base_check(
    _InRef_     PC_ARRAY_HANDLE pc_array_handle)
{
    PC_ARRAY_BLOCK p_array_block;

    if(!array_handle_valid(pc_array_handle))
    {
        myassert3(TEXT("array_base(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle >= free ") S32_TFMT, pc_array_handle, *pc_array_handle, array_root.free);
        return(P_DATA_NONE);
    }

    p_array_block = array_blockc_no_checks(pc_array_handle);

    if(p_array_block->parms.entry_free)
    {
        myassert2(TEXT("array_base(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle has been freed"), pc_array_handle, *pc_array_handle);
        return(P_DATA_NONE);
    }

    if((U32) p_array_block->free >= ALLOC_SIZE_LIMIT)
    {
        myassert3(TEXT("array_base(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle info block has corrupt free ") S32_TFMT TEXT(" field"), pc_array_handle, *pc_array_handle, p_array_block->free);
        return(P_DATA_NONE);
    }

    if((U32) p_array_block->size >= ALLOC_SIZE_LIMIT)
    {
        myassert3(TEXT("array_base(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle info block has corrupt size ") S32_TFMT TEXT(" field"), pc_array_handle, *pc_array_handle, p_array_block->size);
        return(P_DATA_NONE);
    }

    if((U32) p_array_block->free > (U32) p_array_block->size)
    {
        myassert4(TEXT("array_base(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle info block has corrupt free ") S32_TFMT TEXT(" and/or size ") S32_TFMT TEXT(" fields"),
                  pc_array_handle, *pc_array_handle, p_array_block->free, p_array_block->size);
        return(P_DATA_NONE);
    }

    return(p_array_block->p_any);
}

_Check_return_
_Ret_ /*opt_*/
extern PC_ARRAY_BLOCK
_array_block(
    _InRef_     PC_ARRAY_HANDLE pc_array_handle)
{
    PC_ARRAY_BLOCK p_array_block;

    if(!array_handle_valid(pc_array_handle))
    {
        myassert3(TEXT("array_block(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(" --- handle >= free ") S32_TFMT, pc_array_handle, *pc_array_handle, array_root.free);
        return(P_ARRAY_BLOCK_NONE);
    }

    p_array_block = array_blockc_no_checks(pc_array_handle);

    if(p_array_block->parms.entry_free)
    {
        myassert2(TEXT("array_block(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle has been freed"), pc_array_handle, *pc_array_handle);
        return(P_ARRAY_BLOCK_NONE);
    }

    return(p_array_block);
}

_Check_return_
extern ARRAY_INDEX
_array_elements_check(
    _InRef_     PC_ARRAY_HANDLE pc_array_handle)
{
    PC_ARRAY_BLOCK p_array_block;

    if(!array_handle_valid(pc_array_handle))
    {
        myassert3(TEXT("array_elements(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle >= free ") S32_TFMT, pc_array_handle, *pc_array_handle, array_root.free);
        return(0);
    }

    p_array_block = array_blockc_no_checks(pc_array_handle);

    if(p_array_block->parms.entry_free)
    {
        myassert2(TEXT("array_elements(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle has been freed"), pc_array_handle, *pc_array_handle);
        return(0);
    }

    if((U32) p_array_block->free >= ALLOC_SIZE_LIMIT)
    {
        myassert3(TEXT("array_elements(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle info block has corrupt free ") S32_TFMT TEXT(" field"), pc_array_handle, *pc_array_handle, p_array_block->free);
        return(0);
    }

    if((U32) p_array_block->size >= ALLOC_SIZE_LIMIT)
    {
        myassert3(TEXT("array_elements(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle info block has corrupt size ") S32_TFMT TEXT(" field"), pc_array_handle, *pc_array_handle, p_array_block->size);
        return(0);
    }

    if((U32) p_array_block->free > (U32) p_array_block->size)
    {
        myassert4(TEXT("array_elements(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle info block has corrupt free ") S32_TFMT TEXT(" and/or size ") S32_TFMT TEXT(" fields"),
                  pc_array_handle, *pc_array_handle, p_array_block->free, p_array_block->size);
        return(0);
    }

    return(p_array_block->free);
}

_Check_return_
_Ret_ /*opt_*/
extern PC_ARRAY_BLOCK
_array_ptr_check(
    _InRef_     PC_ARRAY_HANDLE pc_array_handle,
    _InVal_     ARRAY_INDEX i)
{
    PC_ARRAY_BLOCK p_array_block;

    if(!array_handle_valid(pc_array_handle))
    {
        myassert3(TEXT("array_ptr(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle >= free ") S32_TFMT, pc_array_handle, *pc_array_handle, array_root.free);
        return(P_ARRAY_BLOCK_NONE);
    }

    p_array_block = array_blockc_no_checks(pc_array_handle);

    if(p_array_block->parms.entry_free)
    {
        myassert2(TEXT("array_ptr(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle has been freed"), pc_array_handle, *pc_array_handle);
        return(P_ARRAY_BLOCK_NONE);
    }

    if((U32) p_array_block->free >= ALLOC_SIZE_LIMIT)
    {
        myassert3(TEXT("array_ptr(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle info block has corrupt free ") S32_TFMT TEXT(" field"), pc_array_handle, *pc_array_handle, p_array_block->free);
        return(P_ARRAY_BLOCK_NONE);
    }

    if((U32) p_array_block->size >= ALLOC_SIZE_LIMIT)
    {
        myassert3(TEXT("array_ptr(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle info block has corrupt size ") S32_TFMT TEXT(" field"), pc_array_handle, *pc_array_handle, p_array_block->size);
        return(P_ARRAY_BLOCK_NONE);
    }

    if((U32) p_array_block->free > (U32) p_array_block->size)
    {
        myassert4(TEXT("array_ptr(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- handle info block has corrupt size ") S32_TFMT TEXT(" and/or free ") S32_TFMT TEXT(" fields"),
                  P_ARRAY_BLOCK_NONE, *pc_array_handle, p_array_block->free, p_array_block->size);
        return(P_ARRAY_BLOCK_NONE);
    }

    if((U32) i > (U32) p_array_block->free)
        /*if(((U32) i > 1U) || (p_array_block->free != 0))*/
            myassert4(TEXT("array_ptr(") PTR_XTFMT TEXT("->h:") S32_TFMT TEXT(") --- index ") S32_TFMT TEXT(" >= free ") S32_TFMT, pc_array_handle, *pc_array_handle, i, p_array_block->free);

    return(p_array_block);
}

#endif /* CHECKING */

/*
---------------------------------------------------------------------------------------------------------------------------
AllocBlock stuff now part of aligator too
---------------------------------------------------------------------------------------------------------------------------
*/

/*extern*/ P_ALLOCBLOCK global_string_alloc_block;

/*
macro used to align allocations as desired
*/

#define ALLOCBLOCK_ROUNDUP(x) ( \
    ((x) + (sizeof32(U32) - 1U)) & ~(sizeof32(U32) - 1U) )

/******************************************************************************
*
* start off the chain of core with an initial allocation
*
******************************************************************************/

_Check_return_
extern STATUS
alloc_block_create(
    _OutRef_    P_P_ALLOCBLOCK lplpAllocBlock,
    _InVal_     U32 n_bytes_wanted)
{
    STATUS status;
    P_ALLOCBLOCK lpAllocBlock;
    const U32 n_bytes_alloc = ALLOCBLOCK_ROUNDUP(n_bytes_wanted);

    if(NULL != (*lplpAllocBlock = lpAllocBlock = al_ptr_alloc_bytes(ALLOCBLOCK, n_bytes_alloc, &status)))
    {
        lpAllocBlock->next = NULL;
        lpAllocBlock->hwm = ALLOCBLOCK_ROUNDUP(sizeof32(*lpAllocBlock));
        lpAllocBlock->size = n_bytes_alloc;
    }

    return(status);
}

/******************************************************************************
*
* blow away the chain of core
*
******************************************************************************/

__pragma(warning(push)) __pragma(warning(disable:6001)) /* Using uninitialized memory '*lpAllocBlock' */

extern void
alloc_block_dispose(
    _InoutRef_  P_P_ALLOCBLOCK lplpAllocBlock)
{
    P_ALLOCBLOCK lpAllocBlock = *lplpAllocBlock;

    *lplpAllocBlock = NULL;

    while(NULL != lpAllocBlock)
    {
        P_ALLOCBLOCK next_lpAllocBlock = lpAllocBlock->next;
        al_ptr_free(lpAllocBlock);
        lpAllocBlock = next_lpAllocBlock;
    }
}

__pragma(warning(pop))

/******************************************************************************
*
* attempt to allocate some memory within the current block of core
*
* if this fails, try adding some more core to the chain
* this does in fact waste a little at the end of each block
* but that's the price you pay for simplicity
*
******************************************************************************/

_Check_return_
_Ret_writes_bytes_to_maybenull_(n_bytes_requested, 0) /* may be NULL */
extern P_BYTE
alloc_block_malloc(
    _InoutRef_  P_P_ALLOCBLOCK lplpAllocBlock,
    _InVal_     U32 n_bytes_requested,
    _OutRef_    P_STATUS p_status)
{
    P_ALLOCBLOCK lpAllocBlock = *lplpAllocBlock;
    const U32 n_bytes_alloc = ALLOCBLOCK_ROUNDUP(n_bytes_requested);
    U32 bytes_left = lpAllocBlock->size - lpAllocBlock->hwm;

    if(bytes_left < n_bytes_alloc)
    {
        /* try to get another chunk of core from the system and add it to the HEAD of the list (which is why we need the inout parameter) */
        U32 new_block_size = lpAllocBlock->size;
        P_ALLOCBLOCK new_lpAllocBlock;

        if(NULL == (new_lpAllocBlock = al_ptr_alloc_bytes(ALLOCBLOCK, new_block_size, p_status)))
            return(NULL);

        new_lpAllocBlock->next = lpAllocBlock;
        new_lpAllocBlock->hwm = ALLOCBLOCK_ROUNDUP(sizeof32(*new_lpAllocBlock));
        new_lpAllocBlock->size = new_block_size;

        *lplpAllocBlock = new_lpAllocBlock;
        lpAllocBlock = new_lpAllocBlock;
    }

    { /* trivial allocation within current block */
    P_BYTE p = (P_BYTE) lpAllocBlock;
    p += lpAllocBlock->hwm;
    lpAllocBlock->hwm += n_bytes_alloc;
    *p_status = STATUS_OK;
    return(p);
    } /*block*/
}

/******************************************************************************
*
* do a string assignment without having to think about cleanup
*
******************************************************************************/

_Check_return_
extern STATUS
alloc_block_ustr_set(
    _OutRef_    P_P_USTR aa,
    _In_z_      PC_USTR b,
    _InoutRef_  P_P_ALLOCBLOCK lplpAllocBlock)
{
    STATUS status;
    P_USTR a;
    U32 l;

    *aa = NULL;

    if(NULL == b)
        return(STATUS_OK);

    l = strlen32p1(b);

#if CHECKING && defined(UNUSED_IN_PD)
    if(contains_inline(b))
    {
        assert0();
        /* "<<al_ustr_realloc - CONTAINS INLINES>>" */
        l = 1 /*NULLCH*/ + ustr_inline_strlen32(b);
    }
#endif

    if(!*lplpAllocBlock)
        status_return(alloc_block_create(lplpAllocBlock, 0x0800 - sizeof32(ALLOCBLOCK)));

    if(NULL == (*aa = a = (P_USTR) alloc_block_malloc(lplpAllocBlock, l, &status)))
        return(status);

    void_memcpy32(a, b, l);
    return(STATUS_DONE);
}

#if defined(UNUSED_IN_PD)

_Check_return_
extern STATUS
alloc_block_tstr_set(
    _OutRef_    P_PTSTR aa,
    _In_z_      PCTSTR b,
    _InoutRef_  P_P_ALLOCBLOCK lplpAllocBlock)
{
    STATUS status;
    PTSTR a;
    U32 l;

    *aa = NULL;

    if(NULL == b)
        return(STATUS_OK);

    l = tstrlen32p1(b);

    if(!*lplpAllocBlock)
        status_return(alloc_block_create(lplpAllocBlock, 0x0800 - sizeof32(ALLOCBLOCK)));

    if(NULL == (*aa = a = (PTCH) alloc_block_malloc(lplpAllocBlock, l * sizeof32(*a), &status)))
        return(status);

    void_memcpy32(a, b, l * sizeof32(*a));
    return(STATUS_DONE);
}

#endif /* UNUSED_IN_PD */

/* end of aligator.c */
