#!/bin/sh

echo " =======================    UPC Testing Suite   ==============================="
echo "                                (12/07/2000 Version 0.95)"   
echo " "

:> /tmp/toprn
for fn in I_*.c II_*.c III_*.c IV_*.c V_*.c VI_*.c  \
          VII_*.c VIII_*.c IX_*.c X_*.c XI_*.c XII_*.c ; do
  cat $fn 
done 

echo " "
echo "====================== Test end =============================================="
