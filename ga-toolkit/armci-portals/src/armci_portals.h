#ifndef PORTALS_H
#define PORTALS_H

/* portals header file */

#include <portals/portals3.h>
#include <portals/nal.h>

#define NUM_COMP_DSCRO 16
#define NUM_COMP_DSCR 8

#define ARMCI_PORTALS_PTL_NUMBER 37

#define HAS_RDMA_GET
#define NUM_SERV_BUFSO 4
#define NUM_SERV_BUFS 1

/*corresponds to num of different armci mem regions*/
#define MAX_MEM_REGIONS 64

#define VBUF_DLEN_ORG 4*64*1024
#define VBUF_DLENO 64*1024
#define VBUF_DLEN 16*1024
#define VBUF_DLEN16 16*1024
#define MSG_BUFLEN_DBL ((VBUF_DLEN)>>3)


#define ARMCI_NET_ERRTOSTR(__ARMCI_ERC_) ptl_err_str[__ARMCI_ERC_]

typedef enum op {
        ARMCI_PORTALS_PUT,
        ARMCI_PORTALS_NBPUT,
        ARMCI_PORTALS_GET, 
        ARMCI_PORTALS_NBGET, 
        ARMCI_PORTALS_ACC,
        ARMCI_PORTALS_NBACC,
        ARMCI_PORTALS_GETPUT,
        ARMCI_PORTALS_NBGETPUT
} armci_portals_optype;

typedef struct {
    void *data_ptr;         /* pointer where the data should go */
    long ack;               /* header ack */
    void *ack_ptr;          /* pointer where the data should go */
#if defined(SERV_QUEUE)
    int imm_msg;
    size_t data_len;
#endif
} msg_tag_t;

typedef struct armci_portals_desc{
       int active;
       int tag;
       int dest_id;
       armci_portals_optype type;
       ptl_md_t mem_dsc;
       ptl_handle_md_t mem_dsc_hndl;
       char *bufptr;
}comp_desc;

/*for buffers*/
extern char *armci_portals_client_buf_allocate(int);
#define BUF_ALLOCATE armci_portals_client_buf_allocate
#define BUF_EXTRA_FIELD_T comp_desc*

#define INIT_SEND_BUF(_field,_snd,_rcv) _snd=1;_rcv=1;_field=NULL

#define GET_SEND_BUFFER _armci_buf_get
#define FREE_SEND_BUFFER _armci_buf_release

#define CLEAR_SEND_BUF_FIELD(_field,_snd,_rcv,_to,_op) if((_op==UNLOCK || _op==PUT || ACC(_op)) && _field!=NULL)x_buf_wait_ack((request_header_t *)((void **)&(_field)+1),((char *)&(_field)-sizeof(BUF_INFO_T)));_field=NULL;
#define TEST_SEND_BUF_FIELD(_field,_snd,_rcv,_to,_op,_ret)
#define COMPLETE_HANDLE _armci_buf_complete_nb_request

#define NB_CMPL_T comp_desc*
#define ARMCI_NB_WAIT(_cntr) if(_cntr){\
        int rc;\
        if(nb_handle->tag)\
          if(nb_handle->tag==_cntr->tag)\
          rc = armci_client_complete(0,nb_handle->proc,nb_handle->tag,_cntr);\
} else{\
printf("\n%d:wait null ctr\n",armci_me);}


/* structure of computing process */
typedef struct {
  ptl_pt_index_t ptl;
  ptl_process_id_t  rank;
  ptl_handle_ni_t   ni_h; 
  ptl_handle_eq_t   eq_h;
  ptl_process_id_t  Srank;
  ptl_handle_ni_t   Sni_h; 
  ptl_handle_eq_t   Seq_h;
  int               outstanding_puts;
  int               outstanding_gets;
  ptl_process_id_t *procid_map;  
  ptl_process_id_t *servid_map;  
  int               free_comp_desc_index;
}armci_portals_proc_t;

typedef struct {
  ptl_match_bits_t  mb;
  ptl_md_t          md;
  ptl_handle_me_t   me_h;
  ptl_handle_md_t   md_h;
}armci_portals_serv_mem_t;

typedef struct {
  int               reg_count;
  int               outstanding_puts;
  int               outstanding_gets;
  armci_portals_serv_mem_t meminfo[MAX_MEM_REGIONS];
}armci_portals_serv_t;


extern void print_mem_desc_table(void);
extern int armci_init_portals(void);
extern void armci_fini_portals(void);
extern int armci_post_descriptor(ptl_md_t *md); 
extern int armci_prepost_descriptor(void* start, long bytes);
extern ptl_size_t armci_get_offset(ptl_md_t md, void *ptr,int proc);
extern int armci_get_md(void * start, int bytes , ptl_md_t * md, ptl_match_bits_t * mb);
extern void armci_portals_put(int,void *,void *,int,void **,int );
extern void armci_portals_get(int,void *,void *,int,void **,int );
extern void comp_desc_init();
extern int armci_client_complete(ptl_event_kind_t evt,int proc_id, int nb_tag ,comp_desc * cdesc);

#endif /* PORTALS_H */
