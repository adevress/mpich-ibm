      block data
C$Id: blkdat120lin.f,v 1.2 1995/02/02 23:23:46 d3g681 Exp $
      implicit double precision (a-h, o-z)
      include 'cscf.h'
c
c     initalize data in common ... clumsy but avoids code to read in data
c
c     line of eight be atoms 4.0 a.u. apart with 120 orbitals
c
c     have 9s functions on each center and simulate p's by having
c     s function at +- 1 in each of x, y, z
c
      data ax /   0.0d0, 4.0d0, 8.0d0, 12.0d0, 16.0d0,
     $            20.0d0, 24.0d0, 28.0d0/
      data ay /   8*0.0d0/
      data az /   8*0.0d0/
      data  q /8*4.0d0/, enrep/54.9714285664/
c
      data x /9*0.0d0, 1.6d0, -1.6d0, 0.0d0, 0.0d0, 0.0d0, 0.0d0,
     $        9*4.0d0, 5.6d0,  2.4d0, 4.0d0, 4.0d0, 4.0d0, 4.0d0,
     $        9*8.0d0, 9.6d0,  6.4d0, 8.0d0, 8.0d0, 8.0d0, 8.0d0,
     $       9*12.0d0,13.6d0, 10.4d0,12.0d0,12.0d0,12.0d0,12.0d0,
     $       9*16.0d0,17.6d0, 14.4d0,16.0d0,16.0d0,16.0d0,16.0d0,
     $       9*20.0d0,21.6d0, 18.4d0,20.0d0,20.0d0,20.0d0,20.0d0,
     $       9*24.0d0,25.6d0, 22.4d0,24.0d0,24.0d0,24.0d0,24.0d0,
     $       9*28.0d0,29.6d0, 26.4d0,28.0d0,28.0d0,28.0d0,28.0d0/
      data y /9*0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0, 0.0d0, 0.0d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0, 0.0d0, 0.0d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0, 0.0d0, 0.0d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0, 0.0d0, 0.0d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0, 0.0d0, 0.0d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0, 0.0d0, 0.0d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0, 0.0d0, 0.0d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0, 0.0d0, 0.0d0/
      data z /9*0.0d0, 0.0d0, 0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0,
     $        9*0.0d0, 0.0d0, 0.0d0, 0.0d0, 0.0d0, 1.6d0, -1.6d0/
      data expnt /1741.0d0, 262.1d0, 60.33d0, 17.62d0, 5.933d0, 2.185d0,
     $     0.859, 0.1806d0, 0.05835d0, 6*0.3d0,
     $            1741.0d0, 262.1d0, 60.33d0, 17.62d0, 5.933d0, 2.185d0,
     $     0.859, 0.1806d0, 0.05835d0, 6*0.3d0,
     $            1741.0d0, 262.1d0, 60.33d0, 17.62d0, 5.933d0, 2.185d0,
     $     0.859, 0.1806d0, 0.05835d0, 6*0.3d0,
     $            1741.0d0, 262.1d0, 60.33d0, 17.62d0, 5.933d0, 2.185d0,
     $     0.859, 0.1806d0, 0.05835d0, 6*0.3d0,
     $            1741.0d0, 262.1d0, 60.33d0, 17.62d0, 5.933d0, 2.185d0,
     $     0.859, 0.1806d0, 0.05835d0, 6*0.3d0,
     $            1741.0d0, 262.1d0, 60.33d0, 17.62d0, 5.933d0, 2.185d0,
     $     0.859, 0.1806d0, 0.05835d0, 6*0.3d0,
     $            1741.0d0, 262.1d0, 60.33d0, 17.62d0, 5.933d0, 2.185d0,
     $     0.859, 0.1806d0, 0.05835d0, 6*0.3d0,
     $            1741.0d0, 262.1d0, 60.33d0, 17.62d0, 5.933d0, 2.185d0,
     $     0.859, 0.1806d0, 0.05835d0, 6*0.3d0/
      end
