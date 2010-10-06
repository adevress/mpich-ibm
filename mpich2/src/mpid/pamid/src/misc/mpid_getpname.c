/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/misc/mpid_getpname.c
 * \brief Device interface to MPI_Get_processor_name()
 */
#include <mpidimpl.h>

/**
 * \brief Device interface to MPI_Get_processor_name()
 * \param[out] name      Storage for the name as a string
 * \param[in]  namelen   The maximum allowed length
 * \param[out] resultlen The actual length written
 * \returns MPI_SUCCESS
 *
 * All this does is convert the rank to a string and return the data
 */
int MPID_Get_processor_name(char * name, int namelen, int * resultlen)
{
  /* Get the name from PAMI */
  const char* pami_name = PAMIX_Client_query(MPIDI_Client, PAMI_CLIENT_PROCESSOR_NAME).value.chararray;
  /* Copy to the destination */
  strncpy(name, pami_name, namelen);
  /* Ensure that there is a trailing NULL */
  if (namelen > 0)
    name[namelen - 1]= '\0';
  /* Get the size of the name */
  *resultlen = strlen(name);

  return MPI_SUCCESS;
}
