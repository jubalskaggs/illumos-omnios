/*	$OpenBSD: vioscsireg.h,v 1.1 2013/12/20 21:50:49 matthew Exp $	*/
/*
 * Copyright (c) 2013 Google Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __VIRTIO_SCSI_H__
#define __VIRTIO_SCSI_H__

#include <sys/types.h>


///* Feature bits */
#define VIRTIO_SCSI_F_INOUT                     (0x1 << 0)
#define VIRTIO_SCSI_F_HOTPLUG                   (0x1 << 1)
#define	VIRTIO_SCSI_F_CHANGE                    (0x1 << 2)

// ---------------- OLD ONES -----------------------------------------
///* registers offset in bytes */
#define VIRTIO_SCSI_CONFIG_NUM_QUEUES              0
#define VIRTIO_SCSI_CONFIG_SEG_MAX                 4
#define VIRTIO_SCSI_CONFIG_MAX_SECTORS             8
#define VIRTIO_SCSI_CONFIG_CMD_PER_LUN             12
#define VIRTIO_SCSI_CONFIG_EVI_SIZE                16
#define VIRTIO_SCSI_CONFIG_SENSE_SIZE              20
#define VIRTIO_SCSI_CONFIG_CDB_SIZE                24
#define VIRTIO_SCSI_CONFIG_MAX_CHANNEL             28
#define VIRTIO_SCSI_CONFIG_MAX_TARGET              32
#define VIRTIO_SCSI_CONFIG_MAX_LUN                 36

/* response codes */
#define VIRTIO_SCSI_S_OK                        0
#define VIRTIO_SCSI_S_OVERRUN                   1
#define VIRTIO_SCSI_S_ABORTED                   2
#define VIRTIO_SCSI_S_BAD_TARGET                3
#define VIRTIO_SCSI_S_RESET                     4
#define VIRTIO_SCSI_S_BUSY                      5
#define VIRTIO_SCSI_S_TRANSPORT_FAILURE         6
#define VIRTIO_SCSI_S_TARGET_FAILURE            7
#define VIRTIO_SCSI_S_NEXUS_FAILURE             8
#define VIRTIO_SCSI_S_FAILURE                   9
#define VIRTIO_SCSI_S_FUNCTION_SUCCEEDED        10
#define VIRTIO_SCSI_S_FUNCTION_REJECTED         11
#define VIRTIO_SCSI_S_INCORRECT_LUN             12

/* Controlq type codes */
#define VIRTIO_SCSI_T_TMF                       0
#define VIRTIO_SCSI_T_AN_QUERY                  1
#define VIRTIO_SCSI_T_AN_SUBSCRIBE              2

/* events */
#define VIRTIO_SCSI_T_EVENTS_MISSED             0x80000000
#define VIRTIO_SCSI_T_NO_EVENT                  0
#define VIRTIO_SCSI_T_TRANSPORT_RESET           1
#define VIRTIO_SCSI_T_ASYNC_NOTIFY              2
#define VIRTIO_SCSI_T_PARAM_CHANGE              3

/*reasons of reset event */
#define VIRTIO_SCSI_EVT_RESET_HARD              0
#define VIRTIO_SCSI_EVT_RESET_RESCAN            1
#define VIRTIO_SCSI_EVT_RESET_REMOVED           2

#define VIRTIO_SCSI_S_SIMPLE                   0
#define VIRTIO_SCSI_S_ORDERED                  1
#define VIRTIO_SCSI_S_HEAD                     2
#define VIRTIO_SCSI_S_ACA                      3

///* new from fbsd */
//#define VIRTIO_SCSI_S_SIMPLE                    0
//#define VIRTIO_SCSI_S_ORDERED                   1
//#define VIRTIO_SCSI_S_HEAD                      2
//#define VIRTIO_SCSI_S_ACA                       3

//#ifndef __packed
//#define __packed __attribute__((packed))
//#endif
// ----------------- END OLD ONES ------------------------------------

///* Configuration registers */
//#define VIRTIO_SCSI_CONFIG_NUM_QUEUES       0 /* 32bit */
//#define VIRTIO_SCSI_CONFIG_SEG_MAX          4 /* 32bit */
//#define VIRTIO_SCSI_CONFIG_MAX_SECTORS		8 /* 32bit */
//#define VIRTIO_SCSI_CONFIG_CMD_PER_LUN		12 /* 32bit */
//#define VIRTIO_SCSI_CONFIG_EVENT_INFO_SIZE	16 /* 32bit */
//#define VIRTIO_SCSI_CONFIG_SENSE_SIZE		20 /* 32bit */
//#define VIRTIO_SCSI_CONFIG_CDB_SIZE         24 /* 32bit */
//#define VIRTIO_SCSI_CONFIG_MAX_CHANNEL		28 /* 16bit */
//#define VIRTIO_SCSI_CONFIG_MAX_TARGET		30 /* 16bit */
//#define VIRTIO_SCSI_CONFIG_MAX_LUN          32 /* 32bit */

///* Feature bits */
//#define VIRTIO_SCSI_F_INOUT                 (0x1 << 0)
//#define VIRTIO_SCSI_F_HOTPLUG               (0x1 << 1)

///* Response status values */
//#define VIRTIO_SCSI_S_OK                    0
//#define VIRTIO_SCSI_S_OVERRUN               1
//#define VIRTIO_SCSI_S_ABORTED               2
//#define VIRTIO_SCSI_S_BAD_TARGET            3
//#define VIRTIO_SCSI_S_RESET                 4
//#define VIRTIO_SCSI_S_BUSY                  5
//#define VIRTIO_SCSI_S_TRANSPORT_FAILURE		6
//#define VIRTIO_SCSI_S_TARGET_FAILURE		7
//#define VIRTIO_SCSI_S_NEXUS_FAILURE         8
//#define VIRTIO_SCSI_S_FAILURE               9

///* Task attributes */
//#define VIRTIO_SCSI_S_SIMPLE                0
//#define VIRTIO_SCSI_S_ORDERED               1
//#define VIRTIO_SCSI_S_HEAD                  2
//#define VIRTIO_SCSI_S_ACA                   3

///* from solaris one */
#define VIRTIO_SCSI_CDB_SIZE                32
#define	VIRTIO_SCSI_SENSE_SIZE              96
#define VIRTIO_SCSI_BUFFER_ALLOCATED        0x1
#define VIRTIO_SCSI_BUFFER_FREE             0x2

#ifndef __packed
#define __packed __attribute__((packed))
#endif

// TODO: [jubal] kill these, they're now vioscsi_cmd_req and vioscsi_cmd_resp
///* Request header structure */
//struct virtio_scsi_req_hdr {
//    uint8_t		lun[8];
//    uint64_t	id;
//    uint8_t		task_attr;
//    uint8_t		prio;
//    uint8_t		crn;
//    uint8_t		cdb[32];
//} __packed;
///* Followed by data-out. */

///* Response header structure */
//struct virtio_scsi_res_hdr {
//    uint32_t	sense_len;
//    uint32_t	residual;
//    uint16_t	status_qualifier;
//    uint8_t		status;
//    uint8_t		response;
//    uint8_t		sense[96];
//} __packed;
///* Followed by data-in. */

/* virtio SCSI command request */
struct vioscsi_cmd_req {
    uint8_t lun[8];                                             /* logical unit number */
    uint64_t tag;                                               /* command identifier */
    uint8_t	task_attr;                                          /* task attribute */
    uint8_t	prio;
    uint8_t crn;
    uint8_t cdb[VIRTIO_SCSI_CDB_SIZE];
} __packed;
/* followed by data-out */

/* virtio SCSI response header structure, followed by sense data and data-in */
struct vioscsi_cmd_resp {
    uint32_t sense_len;                                         /* Sense data length */
    uint32_t res_id;                                            /* Residual bytes in data buffer */
    uint16_t status_qualifier;                                  /* status qualifier */
    uint8_t	status;                                             /* command completion status */
    uint8_t response;                                           /* response values */
    uint8_t sense[VIRTIO_SCSI_SENSE_SIZE];
} __packed;
/* Followed by data-in. */

/*Task managment request */
struct vioscsi_ctrl_tmf_req {
    uint32_t type;
    uint32_t subtype;
    uint8_t  lun[8];
    uint64_t tag;
} __packed;

struct vioscsi_ctrl_tmf_resp {
    uint8_t response;
} __packed;

/* asynchronous notification query/subscription */
struct vioscsi_ctrl_an_req {
    uint32_t type;
    uint8_t lun[8];
    uint32_t event_requested;
} __packed;

struct vioscsi_ctrl_an_resp {
    uint32_t event_actual;
    uint8_t	response;
} __packed;

struct vioscsi_event {
    uint32_t event;
    uint8_t lun[8];
    uint32_t reason;
} __packed;



#endif /* __VIRTIO_SCSI_H__ */
