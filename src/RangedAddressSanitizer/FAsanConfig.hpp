/*
 * FAsanConfig.hpp
 *
 *  Created on: 16/04/2014
 *      Author: simon
 */

#ifndef FASANCONFIG_HPP_
#define FASANCONFIG_HPP_

/*
 * If defined, failing assertions will not abort the program
 */
#define FASAN_FAIL_SILENT

// inline the call to __fasan_verify
#define FASAN_INLINE_RUNTIME

#ifdef FASAN_FAIL_SILENT
#define FASAN_CHECK(COND,HANDLER) if (!(COND)) { HANDLER; }
#else
#define FASAN_CHECK(COND,HANDLER) { assert(cond); }
#endif


#endif /* FASANCONFIG_HPP_ */
