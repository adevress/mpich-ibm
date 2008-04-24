/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 *
 *   Copyright (C) 2001 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "ad_ufs.h"
#ifdef ROMIO_BGL
#include "../ad_bgl/ad_bgl.h"
#endif

/* adioi.h has the ADIOI_Fns_struct define */
#include "adioi.h"

struct ADIOI_Fns_struct ADIO_UFS_operations = {
#ifdef ROMIO_BGL   /* BlueGene support for pvfs through ufs */
    ADIOI_BGL_Open, /* Open */
#else
    ADIOI_UFS_Open, /* Open */
#endif
    ADIOI_GEN_ReadContig, /* ReadContig */
    ADIOI_GEN_WriteContig, /* WriteContig */
#ifdef ROMIO_BGL   /* BlueGene support for pvfs through ufs */
    ADIOI_BGL_ReadStridedColl, /* ReadStridedColl */
    ADIOI_BGL_WriteStridedColl, /* WriteStridedColl */
#else
    ADIOI_GEN_ReadStridedColl, /* ReadStridedColl */
    ADIOI_GEN_WriteStridedColl, /* WriteStridedColl */
#endif
    ADIOI_GEN_SeekIndividual, /* SeekIndividual */
    ADIOI_GEN_Fcntl, /* Fcntl */
#ifdef ROMIO_BGL   /* BlueGene support for pvfs through ufs */
    ADIOI_BGL_SetInfo, /* SetInfo */
    ADIOI_GEN_ReadStrided, /* ReadStrided */
    ADIOI_NOLOCK_WriteStrided, /* WriteStrided */
    ADIOI_BGL_Close, /* Close */
#else
    ADIOI_GEN_SetInfo, /* SetInfo */
    ADIOI_GEN_ReadStrided, /* ReadStrided */
    ADIOI_GEN_WriteStrided, /* WriteStrided */
    ADIOI_GEN_Close, /* Close */
#endif
#ifdef ROMIO_HAVE_WORKING_AIO
    ADIOI_GEN_IreadContig, /* IreadContig */
    ADIOI_GEN_IwriteContig, /* IwriteContig */
#else
    ADIOI_FAKE_IreadContig, /* IreadContig */
    ADIOI_FAKE_IwriteContig, /* IwriteContig */
#endif
    ADIOI_GEN_IODone, /* ReadDone */
    ADIOI_GEN_IODone, /* WriteDone */
    ADIOI_GEN_IOComplete, /* ReadComplete */
    ADIOI_GEN_IOComplete, /* WriteComplete */
    ADIOI_GEN_IreadStrided, /* IreadStrided */
    ADIOI_GEN_IwriteStrided, /* IwriteStrided */
    ADIOI_GEN_Flush, /* Flush */
    ADIOI_GEN_Resize, /* Resize */
    ADIOI_GEN_Delete, /* Delete */
};
