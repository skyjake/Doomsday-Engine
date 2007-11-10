/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/*
 * Smart Headers. Leverage the compiler to optimise, debug, and generally improve
 * our code.
 */


/* give hints to gcc as to what the expected outcome of a comparison is:
 * eg if (likely(a > 1))
 * use sparingly in places where profiling indicates they are needed
 * ie a LOT of mis-predictions - or you'll just slow it down.
 * use -freorder-blocks and/or -freorder-blocks-and-partition with
 * GCC to make best use of this.
 */

#ifdef _GNUC_
    #if __GNUC__ >= 3
        #define unlikely(expr) __builtin_expect(!!(expr), 0)
        #define likely(expr) __builtin_expect(!!(expr), 1)
    #endif
    #if __GNUC__ < 3
        #error Sorry, your GCC is too old. Please use 3.x.x or newer.
    #endif
#endif
#ifndef  _GNUC_
        #define unlikely(expr) (expr)
        #define likely(expr) (expr)
#endif

/* leverage OpenMP if available for multi-threading goodness */
#ifdef _OPENMP
#ifdef _GNUC_
#include <omp.h>
#endif
#ifndef  _GNUC_
extern void omp_set_num_threads (int);
extern int omp_get_num_threads (void);
extern int omp_get_max_threads (void);
extern int omp_get_thread_num (void);
extern int omp_get_num_procs (void);

extern int omp_in_parallel (void);

extern void omp_set_dynamic (int);
extern int omp_get_dynamic (void);

extern void omp_set_nested (int);
extern int omp_get_nested (void);

extern void omp_init_lock (omp_lock_t *);
extern void omp_destroy_lock (omp_lock_t *);
extern void omp_set_lock (omp_lock_t *);
extern void omp_unset_lock (omp_lock_t *);
extern int omp_test_lock (omp_lock_t *);

extern void omp_init_nest_lock (omp_nest_lock_t *);
extern void omp_destroy_nest_lock (omp_nest_lock_t *);
extern void omp_set_nest_lock (omp_nest_lock_t *);
extern void omp_unset_nest_lock (omp_nest_lock_t *);
extern int omp_test_nest_lock (omp_nest_lock_t *);

extern double omp_get_wtime (void);
extern double omp_get_wtick (void);
#endif
#endif
