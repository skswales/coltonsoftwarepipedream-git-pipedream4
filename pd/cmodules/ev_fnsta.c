/* ev_fnsta.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1991-1998 Colton Software Limited
 * Copyright (C) 1998-2014 R W Colton */

/* Statistical function routines for evaluator */

#include "common/gflags.h"

#include "cmodules/ev_eval.h"

#include "cmodules/ev_evali.h"

#include "cmodules/mathxtr2.h" /* for linest() */
#include "cmodules/mathxtr3.h" /* for uniform_distribution() */
#include "cmodules/mathxtr4.h" /* for mx_ln_factorial() */

/******************************************************************************
*
* Statistical functions
*
******************************************************************************/

/*
Statistical functions calculated using array_range processing in ev_exec.c:
AVG()
COUNT()
COUNTA()
MAX()
MIN()
STD()
STDP()
VAR()
VARP()
*/

/*
flatten an x by y array into a 1 by x*y array
*/

static void
ss_data_array_flatten(
    _InoutRef_  P_EV_DATA p_ev_data)
{
    if(RPN_TMP_ARRAY == p_ev_data->did_num)
    {
        /* this is just so easy given the current array representation */
        p_ev_data->arg.ev_array.y_size *= p_ev_data->arg.ev_array.x_size;
        p_ev_data->arg.ev_array.x_size = 1;
    }
}

/******************************************************************************
*
* NUMBER beta(a, b)
*
******************************************************************************/

PROC_EXEC_PROTO(c_beta)
{
    STATUS err = STATUS_OK;
    const F64 a = args[0]->arg.fp;
    const F64 b = args[1]->arg.fp;
    F64 beta_result;

    exec_func_ignore_parms();

    errno = 0;

    beta_result = exp(mx_ln_beta(a, b));

    ev_data_set_real(p_ev_data_res, beta_result);

    if(errno)
        err = status_from_errno();

    if(status_fail(err))
        ev_data_set_error(p_ev_data_res, err);
}

/******************************************************************************
*
* bin(array1, array2)
*
******************************************************************************/

static void
bin_and_frequency_calc(
    _OutRef_    P_EV_DATA p_ev_data_out,
    _InRef_     PC_EV_DATA array_data,
    _InRef_     PC_EV_DATA array_bins,
    _InVal_     BOOL ignore_blanks_and_strings)
{
    S32 xs_0, ys_0;
    S32 xs_1, ys_1;

    array_range_sizes(array_data, &xs_0, &ys_0);
    array_range_sizes(array_bins, &xs_1, &ys_1);

    /* make result array */
    if(status_ok(ss_array_make(p_ev_data_out, 1, ys_1 + 1)))
    {
        { /* clear result array to zero as widest integers */
        S32 iy;

        for(iy = 0; iy < ys_1 + 1; ++iy)
        {
            P_EV_DATA p_ev_data = ss_array_element_index_wr(p_ev_data_out, 0, iy);
            ev_data_set_WORD32(p_ev_data, 0);
        }
        } /*block*/

        { /* put each item into a bin */
        S32 ix, iy;

        for(ix = 0; ix < xs_0; ++ix)
        {
            for(iy = 0; iy < ys_0; ++iy)
            {
                EV_DATA ev_data;
                const EV_IDNO data_id = array_range_index(&ev_data, array_data, ix, iy, EM_REA | EM_INT | EM_DAT | EM_STR);

                switch(data_id)
                {
                case RPN_DAT_STRING:
#if 0 /* just for diff minimization */
                   if(ignore_blanks_and_strings)
                   {   /* ignore this data item */
                       break;
                   }
#endif

                   /*FALLTHRU*/

                case RPN_DAT_REAL:
                case RPN_DAT_WORD8:
                case RPN_DAT_WORD16:
                case RPN_DAT_WORD32:
                case RPN_DAT_DATE:
                    {
                    S32 bin_iy, bin_out_iy = ys_1;

                    for(bin_iy = 0; bin_iy < ys_1; ++bin_iy)
                    {
                        EV_DATA ev_data_bin;
                        const EV_IDNO bin_id = array_range_index(&ev_data_bin, array_bins, 0, bin_iy, EM_REA | EM_INT | EM_DAT | EM_STR);
                        S32 res;

#if 0 /* just for diff minimization */
                        if(ignore_blanks_and_strings && (RPN_DAT_STRING == bin_id))
                        {   /* can't match anything against this bin - skip */
                            continue;
                        }
#else
                        IGNOREPARM_InVal_(ignore_blanks_and_strings);
                        IGNOREPARM_CONST(bin_id);
#endif

                        res = ss_data_compare(&ev_data, &ev_data_bin);

                        ss_data_free_resources(&ev_data_bin);

                        if(res <= 0)
                        {
                            bin_out_iy = bin_iy;
                            break;
                        }
                    }

                    ss_array_element_index_wr(p_ev_data_out, 0, bin_out_iy)->arg.integer += 1;
                    break;
                    }

                default:
                    break;
                }

                ss_data_free_resources(&ev_data);
            }
        }
        } /*block*/
    }
}

PROC_EXEC_PROTO(c_bin)
{
    const PC_EV_DATA array_data = args[0];
    const PC_EV_DATA array_bins = args[1];

    exec_func_ignore_parms();

    bin_and_frequency_calc(p_ev_data_res, array_data, array_bins, FALSE);
}

#if 0 /* just for diff minimization */

PROC_EXEC_PROTO(c_frequency)
{
    const PC_EV_DATA array_data = args[0];
    const PC_EV_DATA array_bins = args[1];

    exec_func_ignore_parms();

    bin_and_frequency_calc(p_ev_data_res, array_data, array_bins, TRUE);
}

#endif

/******************************************************************************
*
* NUMBER combin(n, k)
*
******************************************************************************/

/* nCk = (n k) = (n.(n-1).(n-2)...(n-k+1)) / (k.(k-1).(k-2)...1) */

/* Mathematically nCk is zero for k>n but most spreadsheets avoid this */

/* For k<=n, as factorials, nCk = (n k) = n!/((n-k)!*k!) */

static void
binomial_coefficient_calc(
    _OutRef_    P_EV_DATA p_ev_data_out,
    _InVal_     S32 n,
    _InVal_     S32 k)
{
    STATUS err = STATUS_OK;

    if((n < 0) || (k < 0) /*|| ((n - k) < 0)*/)
    {
        err = EVAL_ERR_ARGRANGE;
    }
    else if(k > n)
    {
        ev_data_set_integer(p_ev_data_out, 0);
    }
    else if((k == 0) || (k == n))
    {
        ev_data_set_integer(p_ev_data_out, 1);
    }
    else if(n <= 170) /* SKS maximum factorial that will fit in F64 */
    {
        EV_DATA ev_data_divisor;

        /* assume result will be integer to start with */
        p_ev_data_out->did_num = RPN_DAT_WORD32;

        /* function will go to fp as necessary */
        product_between_calc(p_ev_data_out, (n - k) + 1, n);

        /* calculate divisor in same format as dividend (either still WORD32 or REAL)
         * NB divisor is always smaller than dividend so this is OK
         */
        ev_data_divisor.did_num = p_ev_data_out->did_num;
        product_between_calc(&ev_data_divisor, 1, k);
        assert(ev_data_divisor.did_num == p_ev_data_out->did_num);

        if(RPN_DAT_REAL == p_ev_data_out->did_num)
        {
            /* binomial coefficient always integer result - see if we can get one! */
            ev_data_set_real_ti(p_ev_data_out, floor((p_ev_data_out->arg.fp / ev_data_divisor.arg.fp) + 0.5));
        }
        else
        {
            /* no worries about remainders - combination always integer result! */
            ev_data_set_integer(p_ev_data_out, p_ev_data_out->arg.integer / ev_data_divisor.arg.integer);
        }
    }
    else
    {
        F64 ln_binomial_coefficient;
        F64 binomial_coefficient_result;

        errno = 0;

        ln_binomial_coefficient = mx_ln_binomial_coefficient(n, k);

        /* binomial coefficient always integer result - see if we can get one! */
        binomial_coefficient_result = floor(exp(ln_binomial_coefficient) + 0.5);

        ev_data_set_real_ti(p_ev_data_out, binomial_coefficient_result);

        if(errno)
            err = status_from_errno();
    }

    if(status_fail(err))
        ev_data_set_error(p_ev_data_out, err);
}

PROC_EXEC_PROTO(c_combin)
{
    const S32 n = args[0]->arg.integer;
    const S32 k = args[1]->arg.integer;

    exec_func_ignore_parms();

    binomial_coefficient_calc(p_ev_data_res, n, k);
}

/******************************************************************************
*
* REAL gammaln(number)
*
******************************************************************************/

PROC_EXEC_PROTO(c_gammaln)
{
    STATUS err = STATUS_OK;
    const F64 number = args[0]->arg.fp;
    F64 gammaln_result;

    exec_func_ignore_parms();

    errno = 0;

    gammaln_result = lgamma(number);

    ev_data_set_real(p_ev_data_res, gammaln_result);

    if(errno)
        err = status_from_errno();

    if(status_fail(err))
        ev_data_set_error(p_ev_data_res, err);
}

/******************************************************************************
*
* REAL grand({mean {, sd}})
*
******************************************************************************/

PROC_EXEC_PROTO(c_grand)
{
    F64 s = 1.0;
    F64 m = 0.0;
    F64 r;

    exec_func_ignore_parms();

    switch(n_args)
    {
    case 2:
        if((s = args[1]->arg.fp) < 0.0)
            s = -s;

        /*FALLTHRU*/

    case 1:
        m = args[0]->arg.fp;
        break;

    default:
        break;
    }

    consume_bool(uniform_distribution_test_seeded(TRUE /*ensure*/));

    r  = normal_distribution();

    r *= s;
    r += m;

    ev_data_set_real(p_ev_data_res, r);
}

/******************************************************************************
*
* ARRAY listcount(array)
*
******************************************************************************/

PROC_EXEC_PROTO(c_listcount)
{
    STATUS status = STATUS_OK;
    EV_DATA ev_data_temp_array;

    exec_func_ignore_parms();

    ev_data_set_blank(&ev_data_temp_array);

    for(;;)
    {
        S32 xs, ys, n_unique = 0, iy = 0;

        status_assert(ss_data_resource_copy(&ev_data_temp_array, args[0]));
        data_ensure_constant(&ev_data_temp_array);

        if(RPN_DAT_ERROR == ev_data_temp_array.did_num)
        {
            ss_data_free_resources(p_ev_data_res);
            *p_ev_data_res = ev_data_temp_array;
            return;
        }

        status_break(status = array_sort(&ev_data_temp_array, 0));

        status_break(status = ss_array_make(p_ev_data_res, 0, 0));

        array_range_sizes(&ev_data_temp_array, &xs, &ys);

        while((iy < ys) && status_ok(status))
        {
            EV_DATA ev_data;
            F64 count = 0;

            (void) array_range_index(&ev_data, &ev_data_temp_array, 0, iy, EM_CONST);

            while(status_ok(status))
            {
                EV_DATA ev_data_t;

                (void) array_range_index(&ev_data_t, &ev_data_temp_array, 0, iy, EM_CONST);

                if(ss_data_compare(&ev_data, &ev_data_t))
                {
                    if(status_ok(status = ss_array_element_make(p_ev_data_res, 1, n_unique)))
                    {
                        P_EV_DATA p_ev_data = ss_array_element_index_wr(p_ev_data_res, 0, n_unique);
                        status_assert(ss_data_resource_copy(p_ev_data, &ev_data));
                        ev_data_set_real_ti(&p_ev_data[1], count);
                        n_unique += 1;
                    }
                    break;
                }
                else if(xs > 1)
                {
                    EV_DATA ev_data_count;
                    /* ignore content of all columns between the first (data) and last (count) */
                    if(RPN_DAT_REAL == array_range_index(&ev_data_count, &ev_data_temp_array, xs-1, iy, EM_REA))
                        count += ev_data_count.arg.fp; /* SKS 19may97 was just using [1, iy] before */
                    ss_data_free_resources(&ev_data_count);
                }
                else
                    count += 1.0;

                iy += 1;
                ss_data_free_resources(&ev_data_t);
            }

            ss_data_free_resources(&ev_data);
        }

        break;
        /*NOTREACHED*/
    }

    ss_data_free_resources(&ev_data_temp_array);

    if(status_fail(status))
    {
        ss_data_free_resources(p_ev_data_res);
        ev_data_set_error(p_ev_data_res, status);
    }
}

/******************************************************************************
*
* NUMBER median(array)
*
******************************************************************************/

PROC_EXEC_PROTO(c_median)
{
    STATUS status = STATUS_OK;
    EV_DATA ev_data_temp_array;

    exec_func_ignore_parms();

    ev_data_set_blank(&ev_data_temp_array);

    for(;;)
    {
        S32 xs, ys, ys_half;
        F64 median;

        /* find median by sorting a copy of the source */
        status_assert(ss_data_resource_copy(&ev_data_temp_array, args[0]));
        data_ensure_constant(&ev_data_temp_array);

        if(ev_data_temp_array.did_num == RPN_DAT_ERROR)
        {
            ss_data_free_resources(p_ev_data_res);
            *p_ev_data_res = ev_data_temp_array;
            return;
        }

        ss_data_array_flatten(&ev_data_temp_array); /* SKS 04jun96 - flatten so we can median() on horizontal and rectangular ranges */

        status_break(status = array_sort(&ev_data_temp_array, 0));

        array_range_sizes(&ev_data_temp_array, &xs, &ys);
        ys_half = ys / 2;

        {
        EV_DATA ev_data;
        (void) array_range_index(&ev_data, &ev_data_temp_array, 0, ys_half, EM_REA);
        median = ev_data.arg.fp;
        } /*block*/

        if(0 == (ys & 1))
        {
            /* if there are an even number of elements, the median is the mean of the two middle ones */
            EV_DATA ev_data;
            (void) array_range_index(&ev_data, &ev_data_temp_array, 0, ys_half - 1, EM_REA);
            median = (median + ev_data.arg.fp) / 2.0;
        }

        ev_data_set_real_ti(p_ev_data_res, median);

        break;
        /*NOTREACHED*/
    }

    ss_data_free_resources(&ev_data_temp_array);

    if(status_fail(status))
    {
        ss_data_free_resources(p_ev_data_res);
        ev_data_set_error(p_ev_data_res, status);
    }
}

/******************************************************************************
*
* NUMBER permut(n, k)
*
******************************************************************************/

/* nPk = n.(n-1).(n-2)...(n-k+1) */

/* Mathematically nPk is zero for k>n but most spreadsheets avoid this */

/* For k<=n, as factorials, nPk = n!/(n-k)! */

static void
permut_calc(
    _OutRef_    P_EV_DATA p_ev_data_out,
    _InVal_     S32 n,
    _InVal_     S32 k)
{
    STATUS err = STATUS_OK;

    if((n < 0) || (k < 0) /*|| ((n - k) < 0)*/)
    {
        err = EVAL_ERR_ARGRANGE;
    }
    else if(k > n)
    {
        ev_data_set_integer(p_ev_data_out, 0);
    }
    else if(k == 0)
    {
        ev_data_set_integer(p_ev_data_out, 1);
    }
    else if(n <= 170) /* SKS maximum factorial that will fit in F64 */
    {
        /* assume result will be integer to start with */
        p_ev_data_out->did_num = RPN_DAT_WORD32;

        /* function will go to fp as necessary */
        product_between_calc(p_ev_data_out, (n - k) + 1, n);

        if(RPN_DAT_WORD32 == p_ev_data_out->did_num)
            p_ev_data_out->did_num = ev_integer_size(p_ev_data_out->arg.integer);
    }
    else
    {
        F64 ln_numerator, ln_denominator;
        F64 permut_result;

        errno = 0;

        ln_numerator = mx_ln_factorial(n);
        ln_denominator = mx_ln_factorial(n - k);

        /* nPk always integer result - see if we can get one! */
        permut_result = floor(exp(ln_numerator - ln_denominator) + 0.5);

        ev_data_set_real_ti(p_ev_data_out, permut_result);

        if(errno)
            err = status_from_errno();
    }

    if(status_fail(err))
        ev_data_set_error(p_ev_data_out, err);
}

PROC_EXEC_PROTO(c_permut)
{
    const S32 n = args[0]->arg.integer;
    const S32 k = args[1]->arg.integer;

    exec_func_ignore_parms();

    permut_calc(p_ev_data_res, n, k);
}

/******************************************************************************
*
* REAL rand({n})
*
******************************************************************************/

PROC_EXEC_PROTO(c_rand)
{
    exec_func_ignore_parms();

    if(!uniform_distribution_test_seeded(FALSE /*test*/))
    {
        if((0 != n_args) && (args[0]->arg.fp != 0.0))
            uniform_distribution_seed((unsigned int) args[0]->arg.fp);
        else
            uniform_distribution_test_seeded(TRUE /*ensure*/);
    }

    ev_data_set_real(p_ev_data_res, uniform_distribution());
}

/******************************************************************************
*
* ARRAY rank(array {, spearflag})
*
******************************************************************************/

PROC_EXEC_PROTO(c_rank)
{
    const PC_EV_DATA array_data = args[0];
    BOOL spearman_correct = FALSE;
    S32 xs, ys;

    exec_func_ignore_parms();

    if(n_args > 1)
        spearman_correct = (0 != args[1]->arg.integer);

    array_range_sizes(array_data, &xs, &ys);

    /* make result array */
    if(status_ok(ss_array_make(p_ev_data_res, 2, ys)))
    {
        S32 iy;

        for(iy = 0; iy < ys; ++iy)
        {
            S32 iy_t;
            S32 position = 1;
            S32 equal = 1;
            EV_DATA ev_data;

            (void) array_range_index(&ev_data, array_data, 0, iy, EM_CONST);

            for(iy_t = 0; iy_t < ys; ++iy_t)
            {
                if(iy_t != iy)
                {
                    EV_DATA ev_data_t;
                    S32 res;

                    (void) array_range_index(&ev_data_t, array_data, 0, iy_t, EM_CONST);

                    res = ss_data_compare(&ev_data, &ev_data_t);

                    ss_data_free_resources(&ev_data_t);

                    if(0 == res)
                        equal += 1;
                    else if(res < 0)
                        position += 1;
                     /* else */
                         /* found a larger value - we can't stop as array_data must be assumed to be unsorted */
                }
            }

            {
            P_EV_DATA p_ev_data = ss_array_element_index_wr(p_ev_data_res, 0, iy);

            if(spearman_correct) /* SKS 12apr95 make suitable for passing to spearman with equal values */
                ev_data_set_real(p_ev_data, position + (equal - 1.0) * 0.5);
            else
                ev_data_set_integer(p_ev_data, position);

            p_ev_data = ss_array_element_index_wr(p_ev_data_res, 1, iy);
            ev_data_set_integer(p_ev_data, equal);
            } /*block*/

            ss_data_free_resources(&ev_data);
        }
    }
}

/******************************************************************************
*
* REAL spearman(array1, array2)
*
******************************************************************************/

PROC_EXEC_PROTO(c_spearman)
{
    F64 spearman_result;
    S32 xs_0, ys_0;
    S32 xs_1, ys_1;
    S32 iy, ys;
    F64 sum_d_squared = 0.0;
    S32 n_counted = 0;

    exec_func_ignore_parms();

    array_range_sizes(args[0], &xs_0, &ys_0);
    array_range_sizes(args[1], &xs_1, &ys_1);

    ys = MAX(ys_0, ys_1);

    for(iy = 0; iy < ys; ++iy)
    {
        EV_DATA ev_data_0;
        EV_DATA ev_data_1;
        const EV_IDNO ev_idno_0 = array_range_index(&ev_data_0, args[0], 0, iy, EM_REA);
        const EV_IDNO ev_idno_1 = array_range_index(&ev_data_1, args[1], 0, iy, EM_REA);

        if( (RPN_DAT_REAL == ev_idno_0) &&
            (RPN_DAT_REAL == ev_idno_1) &&
            (ev_data_0.arg.fp != 0.0) &&
            (ev_data_1.arg.fp != 0.0) )
        {
            F64 d = ev_data_1.arg.fp - ev_data_0.arg.fp;
            n_counted += 1;
            sum_d_squared += d * d;
        }

        ss_data_free_resources(&ev_data_0);
        ss_data_free_resources(&ev_data_1);
    }

    if(0 == n_counted)
    {
        ev_data_set_error(p_ev_data_res, EVAL_ERR_NO_VALID_DATA);
        return;
    }

    spearman_result = (1.0 - (6.0 * sum_d_squared) / ((F64) n_counted * ((F64) n_counted * (F64) n_counted - 1.0)));

    ev_data_set_real(p_ev_data_res, spearman_result);
}

/* end of ev_fnsta.c */