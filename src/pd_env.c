//
//  pd_env.c
//
//  Copyright (c) 2013 Karl-Johan Alm (http://github.com/kallewoof)
// 
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
// 
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
// 
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#include "Pajdeg.h"
#include "pd_env.h"
#include "pd_stack.h"

#include "pd_internal.h"

void pd_env_destroy(pd_env env)
{
    if (env->buildStack) pd_stack_destroy(env->buildStack);
    if (env->varStack) pd_stack_destroy(env->varStack);
    free(env);
}

pd_env pd_env_create(PDStateRef state)
{
    pd_env env = calloc(1, sizeof(struct pd_env));
    env->state = state;
    return env;
}
