#ifndef _PTUTIL_H_
#define _PTUTIL_H_

/*
	Assumes "defs.h" is already included to get definition of bool.
*/
/*[PTINFO]*/
#define PT_MAX_NAME 20

typedef struct {
	int pt_fd_m;					/* master file descriptor */
	int pt_fd_s;					/* slave file descriptor */
	char pt_name_m[PT_MAX_NAME];	/* master file name */
	char pt_name_s[PT_MAX_NAME];	/* slave file name */
} PTINFO;

#define PT_GET_MASTER_FD(p)		((p)->pt_fd_m)
#define PT_GET_SLAVE_FD(p)		((p)->pt_fd_s)
/*[]*/
//#define PT_GET_MASTER_NAME(p)	((p)->pt_name_m)
//#define PT_GET_SLAVE_NAME(p)	((p)->pt_name_s)
PTINFO *pt_open_master(void);
bool pt_wait_master(PTINFO *p);
bool pt_open_slave(PTINFO *p);
bool pt_close_master(PTINFO *p);
bool pt_close_slave(PTINFO *p);

#endif /* _PTUTIL_H_ */
