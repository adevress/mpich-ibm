/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "hydra.h"
#include "hydra_utils.h"
#include "demux.h"
#include "proxy.h"

struct HYD_Proxy_params HYD_Proxy_params;
int HYD_Proxy_listenfd;

int main(int argc, char **argv)
{
    int i, j, arg, count, pid, ret_status;
    int stdin_fd, timeout;
    char *str, *timeout_str;
    char *client_args[HYD_EXEC_ARGS];
    char *tmp[HYDU_NUM_JOIN_STR];
    HYD_Status status = HYD_SUCCESS;

    status = HYD_Proxy_get_params(argc, argv);
    HYDU_ERR_POP(status, "bad parameters passed to the proxy\n");

    status = HYDU_Sock_listen(&HYD_Proxy_listenfd, NULL,
                              (uint16_t *) & HYD_Proxy_params.proxy_port);
    HYDU_ERR_POP(status, "unable to listen on socket\n");

    /* Register the listening socket with the demux engine */
    status = HYD_DMX_Register_fd(1, &HYD_Proxy_listenfd, HYD_STDOUT, HYD_Proxy_listen_cb);
    HYDU_ERR_POP(status, "unable to register fd\n");

    /* FIXME: We do not use the bootstrap server right now, as the
     * current implementation of the bootstrap launch directly reads
     * the executable information from the HYD_Handle structure. Since
     * we are a different process, we don't share this
     * structure. Without the bootstrap server, we can only launch
     * local processes. That is, we can only have a single-level
     * hierarchy of proxies. */

    HYDU_MALLOC(HYD_Proxy_params.out, int *,
                HYD_Proxy_params.proc_count * sizeof(int), status);
    HYDU_MALLOC(HYD_Proxy_params.err, int *,
                HYD_Proxy_params.proc_count * sizeof(int), status);
    HYDU_MALLOC(HYD_Proxy_params.pid, int *,
                HYD_Proxy_params.proc_count * sizeof(int), status);
    HYDU_MALLOC(HYD_Proxy_params.exit_status, int *,
                HYD_Proxy_params.proc_count * sizeof(int), status);

    /* Initialize the exit status */
    for (i = 0; i < HYD_Proxy_params.proc_count; i++)
        HYD_Proxy_params.exit_status[i] = -1;

    /* Spawn the processes */
    for (i = 0; i < HYD_Proxy_params.proc_count; i++) {

        j = 0;
        tmp[j++] = MPIU_Strdup("PMI_ID=");
        status = HYDU_String_int_to_str(HYD_Proxy_params.pmi_id + i, &str);
        HYDU_ERR_POP(status, "unable to convert int to string\n");

        tmp[j++] = MPIU_Strdup(str);
        HYDU_FREE(str);
        tmp[j++] = NULL;
        status = HYDU_String_alloc_and_join(tmp, &str);
        HYDU_ERR_POP(status, "unable to join strings\n");

        HYDU_Env_putenv(str);
        for (j = 0; tmp[j]; j++)
            HYDU_FREE(tmp[j]);

        status = HYDU_Chdir(HYD_Proxy_params.wdir);
        HYDU_ERR_POP(status, "unable to change wdir\n");

        for (j = 0, arg = 0; HYD_Proxy_params.args[j]; j++)
            client_args[arg++] = MPIU_Strdup(HYD_Proxy_params.args[j]);
        client_args[arg++] = NULL;

        if ((i & HYD_Proxy_params.pmi_id) == 0) {
            status = HYDU_Create_process(client_args, &HYD_Proxy_params.in,
                                         &HYD_Proxy_params.out[i], &HYD_Proxy_params.err[i],
                                         &HYD_Proxy_params.pid[i]);
        }
        else {
            status = HYDU_Create_process(client_args, NULL,
                                         &HYD_Proxy_params.out[i],
                                         &HYD_Proxy_params.err[i], &HYD_Proxy_params.pid[i]);
        }
        HYDU_ERR_POP(status, "spawn process returned error\n");

        HYDU_FREE(str);
    }

    /* Everything is spawned, now wait for I/O */
    status = HYD_DMX_Register_fd(HYD_Proxy_params.proc_count, HYD_Proxy_params.out,
                                 HYD_STDOUT, HYD_Proxy_stdout_cb);
    HYDU_ERR_POP(status, "unable to register fd\n");

    status = HYD_DMX_Register_fd(HYD_Proxy_params.proc_count, HYD_Proxy_params.err,
                                 HYD_STDOUT, HYD_Proxy_stderr_cb);
    HYDU_ERR_POP(status, "unable to register fd\n");

    if (HYD_Proxy_params.pmi_id == 0) {
        status = HYDU_Sock_set_nonblock(HYD_Proxy_params.in);
        HYDU_ERR_POP(status, "unable to set socket as non-blocking\n");

        stdin_fd = 0;
        status = HYDU_Sock_set_nonblock(stdin_fd);
        HYDU_ERR_POP(status, "unable to set socket as non-blocking\n");

        HYD_Proxy_params.stdin_buf_offset = 0;
        HYD_Proxy_params.stdin_buf_count = 0;
        status = HYD_DMX_Register_fd(1, &stdin_fd, HYD_STDIN, HYD_Proxy_stdin_cb);
        HYDU_ERR_POP(status, "unable to register fd\n");
    }

    timeout_str = getenv("MPIEXEC_TIMEOUT");
    if (timeout_str)
        timeout = atoi(timeout_str);
    else
        timeout = -1;

    while (1) {
        /* Wait for some event to occur */
        status = HYD_DMX_Wait_for_event(timeout);
        HYDU_ERR_POP(status, "demux engine error waiting for event\n");

        /* Check to see if there's any open read socket left; if there
         * are, we will just wait for more events. */
        count = 0;
        for (i = 0; i < HYD_Proxy_params.proc_count; i++) {
            if (HYD_Proxy_params.out[i] != -1 || HYD_Proxy_params.err[i] != -1) {
                count++;
                break;
            }
        }

        /* We are done */
        if (!count)
            break;
    }

    /* FIXME: If we did not break out yet, add a small usleep to yield
     * CPU here. We can not just sleep for the remaining time, as the
     * timeout value might be large and the application might exit
     * much quicker. Note that the sched_yield() call is broken on
     * newer linux kernel versions and should not be used. */
    /* Once all the sockets are closed, wait for all the processes to
     * finish. We poll here, but hopefully not for too long. */
    do {
        pid = waitpid(-1, &ret_status, WNOHANG);

        /* Find the pid and mark it as complete. */
        if (pid > 0)
            for (i = 0; i < HYD_Proxy_params.proc_count; i++)
                if (HYD_Proxy_params.pid[i] == pid)
                    HYD_Proxy_params.exit_status[i] = WEXITSTATUS(ret_status);

        /* Check how many more processes are pending */
        count = 0;
        for (i = 0; i < HYD_Proxy_params.proc_count; i++) {
            if (HYD_Proxy_params.exit_status[i] == -1) {
                count++;
                break;
            }
        }

        if (count == 0)
            break;

        /* Check if there are any messages from the launcher */
        status = HYD_DMX_Wait_for_event(0);
        HYDU_ERR_POP(status, "demux engine error waiting for event\n");
    } while (1);

    ret_status = 0;
    for (i = 0; i < HYD_Proxy_params.proc_count; i++)
        ret_status |= HYD_Proxy_params.exit_status[i];

  fn_exit:
    if (status != HYD_SUCCESS)
        return -1;
    else
        return ret_status;

  fn_fail:
    goto fn_exit;
}
