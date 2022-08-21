/* cs-msgs.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1990-1998 Colton Software Limited
 * Copyright (C) 1998-2015 R W Colton */

/* Stuart K. Swales, 06-Jun-90 */

/* This code written to replace the supplied RISC_OSLib code
 * as that had (i) too much overhead, was (ii) too slow in loading
 * and (iii) used file i/o without bothering about buffering
 *
 * See msgs.h for explanation of functions / file format
 */

#include "include.h"

#include "msgs.h"

#include "cmodules/file.h"

static char * msgs__block;

static void
__msgs_readfile(
    _In_z_      const char *filename);

/*
compare two strings backwards
*/

static int inline
__strrncmp(const char * a, const char * b, size_t n)
{
    if(n)
    {
        a += n;
        b += n;

        do
            if(*--a != *--b)
                break; /* returns number of leading characters different */
        while(--n);
    }

    return(n);
}

#define INVALID_MSGS_BLOCK ((void *) 1)

#define COMMENT_CH    '#'
#define TAG_DELIMITER ':'

extern void
msgs_init(void)
{
    char filename[256];
    int tmp_length = res_findname("Messages", filename);
    if(tmp_length > 0)
        __msgs_readfile(filename);
}

static const char * lookup_ptr = NULL;

extern /*const*/ char *
msgs_lookup(/*const*/ char *tag_and_default)
{
    const char * tag = tag_and_default;
    char tag_buffer[msgs_TAG_MAX + 1];
    const char * default_ptr;
    U32 tag_length;
    const char * init_lookup_ptr;

    if(NULL == (default_ptr = strchr(tag_and_default, TAG_DELIMITER)))
        tag_length = strlen(tag);
    else
    {   /* copy tag to buffer, stopping at delimiter */
        tag_length = default_ptr - tag_and_default;

        if(tag_length > msgs_TAG_MAX)
        {
            messagef("Tag too long: %s", tag_buffer);
            return(NULL);
        }

        *tag_buffer = CH_NULL;
        strncat(tag_buffer, tag_and_default, tag_length);

        default_ptr++;
        tag = tag_buffer;
    }

    /* if there ain't a message block or it's bad then we can't do lookup */
    if((NULL != msgs__block)  &&  (msgs__block != INVALID_MSGS_BLOCK))
    {
        init_lookup_ptr = lookup_ptr;

        for(;;)
        {
            /* try resuming search at last position or at start */
            if(NULL == lookup_ptr)
                lookup_ptr = msgs__block;

            while(CH_NULL != *lookup_ptr)
            {
                const U32 lookup_length = strlen(lookup_ptr);

                if(lookup_length > tag_length)
                    if(*(lookup_ptr + tag_length) == TAG_DELIMITER)
                        if(0 == __strrncmp(tag, lookup_ptr, tag_length))
                            return((char *) (lookup_ptr + tag_length + 1));

                /* step to next message */
                lookup_ptr += lookup_length + 1;

                if(lookup_ptr == init_lookup_ptr)
                {
                    init_lookup_ptr = NULL;
                    break; /* out of both loops */
                }
            }

            lookup_ptr = NULL; /* ensure restart */

            if(NULL == init_lookup_ptr)
                break;
        }
    }

    /* return the default, or if that fails, the tag */
    return((char *) ((NULL != default_ptr) ? default_ptr : tag_and_default));
}

static STATUS
__msgs_load_messages_file(
    _OutRef_    P_P_U8 p_msgs,
    FILE_HANDLE file_handle)
{
    STATUS status;
    P_ANY msgs;
    S32 length = file_length(file_handle);
    const U32 messages_bodge = 1; /* one for final CH_NULL */

    *p_msgs = NULL;

    if(length < 0)
        return(length);

    if(NULL == (msgs = _al_ptr_alloc(length + messages_bodge, &status)))
        return(status);

    if((status = file_read(msgs, 1, length, file_handle)) != length)
    {
        if(status_ok(status))
            status = STATUS_FAIL;
    }
    else
        status = STATUS_OK;

    if(status_fail(status))
    {
        al_ptr_dispose(&msgs);

        return(status);
    }

    *p_msgs = msgs;
    return(length);
}

/* squish all the comments out of the loaded messages */

static void
__msgs_process_loaded_data(
                P_U8 msgs,
    _InVal_     U32 real_length)
{
    U32 in = 0;
    U32 out = 0;
#if TRACE
    U32 start = 0;
#endif
    const U32 end = real_length;
    int ch;

    do  {
        ch = (in != end) ? msgs[in++] : LF;

        if(ch == COMMENT_CH)
        {   /* SKS 2016sep28 only look for comments at start of lines */
            if((out == 0) || (CH_NULL == msgs[out - 1]))
            {
                tracef1("[msgs_readfile: found comment at &%p]", msgs + (in - 1));
                do
                    ch = (in != end) ? msgs[in++] : LF;
                while(ch != LF);

                if(in == end)
                    break;
            }
        }

        if(ch == LF)
        {
            tracef1("[msgs_readfile: got a line terminator at &%p - place a CH_NULL]", msgs + (in - 1));
            ch = CH_NULL;
        }

        /* don't place two CH_NULLs together or one at the start (this also means you can have blank lines) */
        if(CH_NULL == ch)
        {
            if(out == 0)
                continue;

            if(CH_NULL == msgs[out - 1])
                continue;

#if TRACE
            tracef1("[msgs_readfile: placing CH_NULL at &%p]", msgs + out);
            msgs[out] = CH_NULL;
            tracef1("[msgs_readfile: ended line '%s']", msgs + start);
            start = out + 1;
#endif
        }

        msgs[out++] = (char) ch;
    }
    while(in != end);

    tracef1("[msgs_readfile: placing last CH_NULL at &%p]", msgs + out);
    msgs[out++] = CH_NULL; /* need this last byte as end marker */
}

static void
__msgs_readfile(
    _In_z_      const char *filename)
{
    size_t real_length;

    {
    FILE_HANDLE file_handle;

    if(status_fail(file_open(filename, file_open_read, &file_handle)))
        return;

    real_length = __msgs_load_messages_file(&msgs__block, file_handle);

    file_close(&file_handle);
    } /*block*/

    if(NULL == msgs__block)
    {
        /* stop msgs trying to translate during application death */
        msgs__block = INVALID_MSGS_BLOCK;
        message_output("Memory full: unable to load messages");
        return;
    }

    __msgs_process_loaded_data(msgs__block, real_length);
}

/* end of cs-msgs.c */
