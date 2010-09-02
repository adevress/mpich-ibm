/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_hw.c
 * \brief Fill in a hardware info structure
 */

#include "mpidimpl.h"

#include "pamix.h"

void MPIDI_HW_Init(MPID_Hardware_t *hw)
{
   typedef struct pami_extension_torus_information
   {
      size_t dims;
      size_t *coord;
      size_t *size;
      size_t *torus;
   } pami_extension_torus_information_t;

   int i=0;
   typedef const pami_extension_torus_information_t * 
      (*pami_extension_torus_information_fn) ();
   pami_result_t rc = PAMI_SUCCESS;

   pami_extension_t extension;

   hw->clockMHz = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_CLOCK_MHZ).value.intval;
   hw->memSize = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_MEM_SIZE).value.intval;

/*   fprintf(stderr,"clockMHz: %d, memSize: %d\n", hw->clockMHz, hw->memSize);*/

   rc = PAMI_Extension_open(MPIDI_Client, "EXT_torus_network", &extension);

   if(rc != PAMI_SUCCESS)
   {
      /* Should we assert here or something? */
      fprintf(stderr,"No PAMI torus network extension available.\n");
   }

   pami_extension_torus_information_fn pamix_torus_info = 
      (pami_extension_torus_information_fn) PAMI_Extension_function(extension,
         "information");

   assert(pamix_torus_info != NULL);

   const pami_extension_torus_information_t *info = pamix_torus_info();
   /* The extension returns a "T" dimension */
   hw->torus_dimension = (int)info->dims-1;

   for(i=0;i<info->dims-1;i++)
   {
      hw->Size[i] = (int)info->size[i];
      hw->Coords[i] = (int)info->coord[i];
      hw->isTorus[i] = (int)info->torus[i];
   }
   hw->coreID = (int)info->coord[info->dims-1];
}
