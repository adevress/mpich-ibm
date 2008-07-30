/*
UPC Testing Suite

Copyright (C) 2000 Chen Jianxun, Sebastien Chauvin, Tarek El-Ghazawi

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
/**
   Test: VII_case1_ii   - upc_notify test with an expression which does not 
                          contain shared references (same expression). 
   Purpose: to check that no thread may proceed from the upc_wait until
            after all other threads have issued their upc_notify
            statements.
   Type: Positive.
   How : - Declare a shared array, a, of integers of size THREADS. 
           This array will be automatically initialized to 0.
         - Just before reaching the notify statement, each thread will set 
           a[MYTHREAD] to 1.
         - Right after the wait statement, every thread should check that 
           a is filled with ones. Otherwise, the barrier statement is not
           working properly.
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <upc.h>

shared int a[THREADS];

int
main()
{
  int i,pe=MYTHREAD;
  int sum;
  int errflag=0;


  sleep(MYTHREAD*2/THREADS);  // Make the barrier necessary

  a[pe]=1;
  upc_notify 1;

  sum=0;

  upc_wait 1;
  
  for(i=0; i<THREADS; i++)
    sum+=a[i];

  if (sum!=THREADS){
    errflag=1;
  } 

  if (errflag) {
      printf("Failure: on Thread %d with errflag %d\n",MYTHREAD,errflag);
  } else if (MYTHREAD == 0) {
      printf("Success: on Thread %d \n",MYTHREAD);
  }

  return(errflag);
}
 
  
