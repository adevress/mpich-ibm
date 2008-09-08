#define pthread_attr_t int
#define pid_t int
#define uid_t int
#define __uint32_t int
#define timer_t int
#define clock_t int

typedef union sigval
{
  int sival_int;                        /* integer signal value */
  void  *sival_ptr;                     /* pointer signal value */
} sigval_t;

typedef struct sigevent
{
  sigval_t sigev_value;                 /* signal value */
  int sigev_signo;                      /* signal number */
  int sigev_notify;                     /* notification type */
  void (*sigev_notify_function) (sigval_t); /* notification function */
  pthread_attr_t *sigev_notify_attributes; /* notification attributes */
} sigevent_t;

#pragma pack(push,4)
typedef struct
{
  int si_signo;                         /* signal number */
  int si_code;                          /* signal code */
  pid_t si_pid;                         /* sender's pid */
  uid_t si_uid;                         /* sender's uid */
  int si_errno;                         /* errno associated with signal */

  union
  {
    __uint32_t __pad[32];               /* plan for future growth */
    union
    {
      /* timers */
      struct
      {
        union
        {
          struct
          {
            timer_t si_tid;             /* timer id */
            unsigned int si_overrun;    /* overrun count */
          };
          sigval_t si_sigval;           /* signal value */
          sigval_t si_value;            /* signal value */
        };
      };
    };

    /* SIGCHLD */
    struct
    {
      int si_status;                    /* exit code */
      clock_t si_utime;                 /* user time */
      clock_t si_stime;                 /* system time */
    };

    /* core dumping signals */
    void *si_addr;                      /* faulting address */
  };
} siginfo_t;

int main() {
return 0;
}
