//
// pd_env.c
//
// Copyright (c) 2013 Karl-Johan Alm (http://github.com/kallewoof)
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

