/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VPX_PORTS_MEM_H_
#define VPX_PORTS_MEM_H_

#include "vpx_config.h"
#include "vpx/vpx_integer.h"

#if (defined(__GNUC__) && __GNUC__) || defined(__SUNPRO_C)
#define DECLARE_ALIGNED(n,typ,val)  typ val __attribute__ ((aligned (n)))
#elif defined(_MSC_VER)
#define DECLARE_ALIGNED(n,typ,val)  __declspec(align(n)) typ val
#else
#warning No alignment directives known for this compiler.
#define DECLARE_ALIGNED(n,typ,val)  typ val
#endif

/* Indicates that the usage of the specified variable has been audited to assure
 * that it's safe to use uninitialized. Silences 'may be used uninitialized'
 * warnings on gcc.
 */
#if defined(__GNUC__) && __GNUC__
#define UNINITIALIZED_IS_SAFE(x) x=x
#else
#define UNINITIALIZED_IS_SAFE(x) x
#endif

#if HAVE_NEON && defined(_MSC_VER)
#define __builtin_prefetch(x)
#endif

#endif  // VPX_PORTS_MEM_H_
