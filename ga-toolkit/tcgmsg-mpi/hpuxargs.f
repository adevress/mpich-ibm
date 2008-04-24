c
c     $Header$
c
      integer function hpargc()
      hpargc = iargc() + 1
      end
      integer function hpargv(index, arg, maxlen)
      character*256 arg
      hpargv = igetarg(index,arg,maxlen)
      end
