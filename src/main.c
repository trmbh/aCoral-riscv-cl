/* Copyright 2018 Canaan Inc.	
 *	
 * Licensed under the Apache License, Version 2.0 (the "License");	
 * you may not use this file except in compliance with the License.	
 * You may obtain a copy of the License at	
 *	
 *     http://www.apache.org/licenses/LICENSE-2.0	
 *	
 * Unless required by applicable law or agreed to in writing, software	
 * distributed under the License is distributed on an "AS IS" BASIS,	
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.	
 * See the License for the specific language governing permissions and	
 * limitations under the License.	
 */	
#include <stdio.h>	
#include "bsp.h"	
#include "core.h"
#include "log.h"

 int core1_function(void *ctx)	
{	
    uint64_t core = current_coreid();	
    ACORAL_LOG_TRACE("Core %ld Hello world\n", core);	
    while(1);
}	


 int main()	
{	
    uint64_t core = current_coreid();	
    ACORAL_LOG_TRACE("Core %ld Hello world\n", core);	
    // register_core1(core1_function, NULL);	
    system_start();	
}