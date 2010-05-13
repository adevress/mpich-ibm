/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/mpid_unimpl.c
 * \brief These are functions that are not supported
 */
#include "mpidimpl.h"

int MPID_Close_port(const char *port_name)
{
  MPID_abort();
}
int MPID_Open_port(MPID_Info *info_ptr,
                   char *port_name)
{
  MPID_abort();
}

int MPID_Comm_accept(char *port_name,
                     MPID_Info *info_ptr,
                     int root,
                     MPID_Comm *comm_ptr,
                     MPID_Comm **newcomm)
{
  MPID_abort();
}
int MPID_Comm_connect(const char *port_name,
                      MPID_Info *info_ptr,
                      int root,
                      MPID_Comm *comm_ptr,
                      MPID_Comm **newcomm)
{
  MPID_abort();
}
int MPID_Comm_disconnect(MPID_Comm *comm_ptr)
{
  MPID_abort();
}
int MPID_Comm_spawn_multiple(int count,
                             char *array_of_commands[],
                             char* *array_of_argv[],
                             int array_of_maxprocs[],
                             MPID_Info *array_of_info[],
                             int root,
                             MPID_Comm *comm_ptr,
                             MPID_Comm **intercomm,
                             int array_of_errcodes[])
{
  MPID_abort();
}
