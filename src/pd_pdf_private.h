//
// pd_pdf_private.h
//
// Copyright (c) 2012 - 2014 Karl-Johan Alm (http://github.com/kallewoof)
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef INCLUDED_pd_pdf_private_h
#define INCLUDED_pd_pdf_private_h

#define PDDeallocateViaStackDealloc(ob) (*pd_stack_dealloc)(ob)

#define currch  (scv->allocBuf)[scv->offs]
#define currchi scv->left--; (scv->allocBuf)[(scv->offs)++]
#define currstr &currch
#define iterate(v) scv->offs += v; scv->left -= v

#define putfmt(fmt...)  \
    sz = sprintf(currstr, fmt); \
    iterate(sz)

#define putstr(str, len) \
    memcpy(currstr, str, len); \
    iterate(len)

#define PDStringGrow(req) \
    if (scv->left < req) { \
        scv->allocBuf = realloc(scv->allocBuf, scv->offs + req); \
        scv->left = req; \
    }

#define PDStringFromObRef(ref, reflen) \
    PDInteger obid = pd_stack_pop_int(s); \
    PDInteger genid = pd_stack_pop_int(s); \
    PDInteger sz;\
    \
    char req = 5 + reflen + 2; \
    if (obid > 999) req += 3; \
    if (genid > 99) req += 5; \
    PDStringGrow(req); \
    putfmt("%ld %ld " ref, obid, genid)

// get primitive if primtiive, otherwise delegate to arbitrary func
#define PDStringFromAnything() \
    if ((*s)->type == PD_STACK_STRING) {\
        char *str = pd_stack_pop_key(s);\
        PDInteger len = strlen(str);\
        PDStringGrow(len);\
        putstr(str, len);\
        PDDeallocateViaStackDealloc(str); \
    } else {\
        pd_stack co = pd_stack_pop_stack(s); \
        PDStringFromArbitrary(&co, scv);\
    }

#endif
