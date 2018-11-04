#include <sys/modctl.h>
#include <sys/blkdev.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/ksynch.h>
#include <sys/stat.h>
#include <sys/debug.h>
#include <sys/pci.h>
#include <sys/sysmacros.h>
#include <sys/scsi/scsi.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <io/virtio/virtiovar.h>
#include <io/virtio/virtioreg.h>
#include <io/vioscsi/virtio_scsi.h>
//#include <io/vioscsi/virtio_scsivar.h>



/**
 *
 * kvm args:
 *  -device virtio-scsi-pci,id=scsi0,bus=pci.0,addr=0x8
 *  -device scsi-hd,bus=scsi0.0,channel=0,scsi-id=0,lun=0,drive=drive-scsi0-0-0-0,id=scsi0-0-0-0
 *  -drive file=/var/lib/libvirt/images/omnios-1.qcow2,format=qcow2,if=none,id=drive-scsi0-0-0-0
 *
 * mdb -k ::prtconf
 *
 * -- snip --
 * fffffe01e577fd50 pci1af4,8, instance #0 (driver name: vioscsi)
 *          fffffe01e5d78808 scsiclass,00, instance #1 (driver not attached)
 * -- snip --
 *
 * Same configuration, but picked up out of dmesg from freebsd:
 * Nov  3 09:49:26 fbsd kernel: da0 at vtscsi0 bus 0 scbus2 target 0 lun 0
 * Nov  3 09:49:26 fbsd kernel: da0: <QEMU QEMU HARDDISK 2.5+> Fixed Direct Access SPC-3 SCSI device
 * Nov  3 09:49:26 fbsd kernel: da0: 4294966.784MB/s transfers
 * Nov  3 09:49:26 fbsd kernel: da0: Command Queueing enabled
 * Nov  3 09:49:26 fbsd kernel: da0: 20480MB (41943040 512 byte sectors)
 *
 *
 * Trisk branch from Dmitry at nexenta from 2014 (incomplete, but it's what this is based on
 * https://github.com/trisk/illumos-gate/blob/vioscsi-fk1/usr/src/uts/common/io/vioscsi/vioscsi.c
 *
 * Virtio Doc:
 * http://docs.oasis-open.org/virtio/virtio/v1.0/virtio-v1.0.pdf
 *
 * NetBSD source:
 * https://github.com/NetBSD/src/blob/trunk/sys/dev/pci/vioscsi.c
 *
 * Check here for google compute specs:
 * https://cloud.google.com/compute/docs/images/building-custom-os
 *
 * Useful dtrace regarding scsi_hba
 * Dissecting emulex hba driver:
 *  https://webcache.googleusercontent.com/search?q=cache:r3XGFnnbdooJ:https://nenadnoveljic.com/blog/dissecting-emulex-hba-driver/+&cd=18&hl=en&ct=clnk&gl=us
 *
 *
 * Thread about vioscsi: https://echelog.com/logs/browse/illumos/1517871600
 * [18:47:18] <elegast> Have there been any efforts or plans in the community to port NetBSD's vioscsi driver to illumos?
 * [18:51:33] <yurip> that's for kvm?
 * [18:53:04] <Agnar> yurip: yes, it's a virtio driver that supports various scsi features - for example the possibility two use a shared disk image between two kvms
 * [18:54:04] <Agnar> (supports disk uuids for example and stuff)
 * [18:54:31] <yurip> google tells me Albert might know something:)
 * [18:54:36] * yurip eyes trisk
 * [18:55:31] <yurip> https://github.com/trisk/illumos-gate/blob/vioscsi-fk1/usr/src/uts/common/io/vioscsi/vioscsi.c
 * [18:55:51] <Agnar> woohoo
 * [19:02:09] <elegast> yurip, agar: ooh, that looks promising
 * [19:02:28] <trisk> yep, that has basic functionality but there were some remaining bugs which is why it was split from the original virtio work. I was only a reviewer, Dmitry wrote it
 * [19:02:28] <elegast> *agnar
 * [19:03:06] <elegast> Yeah, I'm scared to put that in production
 * [19:04:54] <elegast> but using it to identify remaining bugs might not be a step in the right direction, thanks guys
 * [19:05:25] <elegast> *awk, might actually be
 * [19:05:57] <elegast> my keyboard has typhoids, must be lack of coffee
 * [19:20:45] <ptribble> virtio-scsi would also be really useful in allowing us to run on Google's cloud
 * [19:21:53] <xorhash> I mindlessly default to virtio for storage and network in kvm guests these days, so I'm somewhat excited to see this
 * [19:35:43] <rmustacc> ptribble: Do you know what else we're missing?
 * [19:38:50] <ptribble> When I looked, virtio-scsi was an obvious blocker so I didn't look too closely, but it's regular kvm so we should otherwise be in pretty good shape
 * [19:39:01] <rmustacc> OK.
 * [19:39:10] <rmustacc> Well, if someone wants to run with the above, that'd be neat.
 *
 *
 * Thread where modernpacifist tries to get smartos booting in gcp: https://echelog.com/logs/browse/smartos/1508104800
 *
 * [01:31:07] <modernpacifist> So this may be an odd thing to be attempting, but I'm messing around with getting SmartOS to boot inside google compute engine (to also mess around with nested virt) but not making it overly far. From what I've read they use KVM, but have their own alternative to QEMU. SmartOS is panicking on boot with "panic[cpu0]/thread=fffffffffbc386c0: microfind: could not calibrate delay loop"
 * [05:43:41] <LeftWing> modernpacifist: A cursory glance suggests that the PIT is not working correctly =\
 * [05:43:54] <LeftWing> (That is, the Intel 8254 Programmable Interval Timer.)
 * [05:46:25] <LeftWing> modernpacifist: Are you able to use kmdb on the console of the instance?
 * [05:46:30] <modernpacifist> Certainly am
 * [05:46:32] <modernpacifist> One moment
 * [05:46:43] <LeftWing> It would be interesting to know the value of "microdata" after that panic
 * [05:47:34] <modernpacifist> Just launching a new instance
 * [05:51:09] <modernpacifist> Alright, got a serial console in kmdb post panic
 * [05:51:37] <LeftWing> microdata/D
 * [05:51:50] <modernpacifist> 65536
 * [05:51:59] <LeftWing> whaaaat
 * [05:55:36] <LeftWing> modernpacifist: Can you see the value of %r12 in ::status ?
 * [05:55:55] <LeftWing> or... ::regs I guess
 * [05:56:09] <modernpacifist> 0xfffffffffbc7edb8 panicbuf
 * [05:57:15] <LeftWing> modernpacifist: Does it suggest what might have been in %r12 in ... ::msgbuf ?
 * [05:58:35] <modernpacifist> LeftWing: The contents of msgbuf: https://gist.github.com/davefinster/51497dda7cf641a43b6438d148b5bc09
[0]> ::msgbuf
MESSAGE
SunOS Release 5.11 Version joyent_20171012T151912Z 64-bit
Copyright (c) 2010-2017, Joyent Inc. All rights reserved.
x86_feature: lgpg
x86_feature: tsc
x86_feature: msr
x86_feature: mtrr
x86_feature: pge
x86_feature: de
x86_feature: cmov
x86_feature: mmx
x86_feature: mca
x86_feature: pae
x86_feature: cv8
x86_feature: pat
x86_feature: sep
x86_feature: sse
x86_feature: sse2
x86_feature: htt
x86_feature: asysc
x86_feature: nx
x86_feature: sse3
x86_feature: cx16
x86_feature: cmp
x86_feature: tscp
x86_feature: cpuid
x86_feature: ssse3
x86_feature: sse4_1
x86_feature: sse4_2
x86_feature: 1gpg
x86_feature: clfsh
x86_feature: 64
x86_feature: aes
x86_feature: pclmulqdq
x86_feature: xsave
x86_feature: avx
x86_feature: vmx
x86_feature: f16c
x86_feature: rdrand
x86_feature: x2apic
x86_feature: avx2
x86_feature: bmi1
x86_feature: bmi2
x86_feature: fma
x86_feature: smep
x86_feature: adx
x86_feature: rdseed
x86_feature: xsaveopt
mem = 3930736K (0xefe9c000)
panic[cpu0]/thread=fffffffffbc386c0:
microfind: could not calibrate delay loop
Warning - stack not written to the dump buffer
fffffffffbc7ab10 unix:kdi_slave_entry_patch+ae41 ()
fffffffffbc7aba0 unix:startup_modules+25 ()
fffffffffbc7abb0 unix:startup+55 ()
fffffffffbc7abf0 genunix:main+3b ()
fffffffffbc7ac00 unix:_locore_start+90 ()
 * [05:58:57] <modernpacifist> Probably going to have to breakpoint this aren't I
 * [05:59:36] <LeftWing> modernpacifist: Unfortunately breakpoints are going to affect the functioning of this mechanism, but ... that might be OK
 * [06:00:23] <LeftWing> modernpacifist: https://github.com/joyent/illumos-joyent/blob/master/usr/src/uts/i86pc/io/microfind.c
 * [06:00:37] <LeftWing> You're going to want to see what's happening with microfind_pit_delta_avg() as it gets called in microfind()
 * [06:00:59] <LeftWing> You may also want to play with the values "microdata_trial_count" and "microdata_allowed_failures" to see if they help.
 * [06:01:52] <LeftWing> In fact, if you increase "microdata_allowed_failures" to, say, 10000
 * [06:01:58] <LeftWing> It might just boot
 * [06:02:45] <LeftWing> i.e., on the kmdb prompt before you :c, perhaps "microdata_allowed_failures/W 0t10000"
 * [06:13:40] <modernpacifist> LeftWing: So that hasn't been entirely successful - its not panicking, but the CPU is pegged at 100% - cant seem to send a break over the serial console either to drop back into the debugger
 * [06:14:09] <LeftWing> modernpacifist: Give it a while?
 * [06:16:48] <modernpacifist> 100 panic'd - 1000 seems to be making progress. WARNING: Time of Day clock error: reason [Stalled]. --  Stopped tracking Time Of Day clock. WARNING: vioif0: Host does not support RING_INDIRECT_DESC. Cannot attach.
 * [06:17:12] <LeftWing> modernpacifist: Interesting.
 * [06:18:40] <modernpacifist> The output: https://gist.github.com/davefinster/915a4dd50c49d47c8b26c20eb400805c - Dumps me to single user mode. Just have to get the platform password
 * [06:19:22] <LeftWing> modernpacifist: Progress!
 * [06:23:18] <LeftWing> modernpacifist: Ugh, the RING_INDIRECT_DESC thing.  It would seem we need to enhance our Virtio NIC driver to support operating without indirect descriptors.
 * [06:27:15] <modernpacifist> LeftWing: Probably worth tossing this against smartos-live on Github? Google also provide a hardware manifest here: https://cloud.google.com/compute/docs/images/building-custom-os
 * [06:29:04] <LeftWing> modernpacifist: Sure, if you want to open a tracking issue for getting this to work that seems a reasonable place.
 * [06:29:44] <LeftWing> modernpacifist: I'm not sure that we have a vioscsi
 * [06:29:54] <LeftWing> It looks like one might be required here too
 * [06:36:37] <modernpacifist> LeftWing: Perhaps not something worth opening. An enhancement to virtio networking is one thing but introducing scsi is probably a bit much, on top of the other environmental curisoities
 * [06:36:47] <modernpacifist> *curiosities
 * [06:36:59] <LeftWing> modernpacifist: It depends on why you're trying to run in GCE, I guess ;)
 * [06:37:21] <LeftWing> Our SCSI framework is relatively complete, and we have a common vio layer
 * [06:37:27] <LeftWing> Virtio layer, that is
 * [06:37:33] <LeftWing> (vioblk and vioif are based on it)
 * [06:37:39] <LeftWing> It might not be that much work to do Virtio SCSI
 * [06:38:05] <modernpacifist> LeftWing: A friend has a physical DC located SmartOS box that will soon become unavailable and thought a quick fix for him would be to spin up a GCE instance and just migrate
 * [06:38:18] <LeftWing> modernpacifist: Ahh.
 * [06:39:30] <modernpacifist> LeftWing: From my reading, they now support KVM nested virtualisation and for IP ranges they do https://cloud.google.com/compute/docs/alias-ip/
 * [06:39:57] <LeftWing> modernpacifist: Interesting.
 * [06:44:34] <modernpacifist> LeftWing: Oh well, I'll tell him to get an OVH box - they are in Sydney now
 * [06:44:48] <LeftWing> modernpacifist: That seems like it will require less engineering effort.
 * [06:45:18] <modernpacifist> LeftWing: Less engineering effort, but more panic wondering whether their single telstra transit link doesn't fall over :P
 * [07:03:43] <modernpacifist> LeftWing: Night!
 *
 *
 *
 * Another thread: https://echelog.com/logs/browse/illumos/1386630000
 * Another thread: https://echelog.com/logs/browse/openindiana/1432591200
 * A bug where virtio-net was added: https://illumos.org/issues/3644
 * Related to ^ http://cr.illumos.org/~webrev/int0dh/virtio-net/
 * One where virtio-blk was added: https://www.illumos.org/issues/1147
 *
 * http://blog.sina.com.cn/s/blog_553c6d4e0101jb4w.html
 * and https://github.com/xl0/solaris-virtio/tree/solaris11
 * and https://github.com/xl0/illumos-virtio (THIS IS THE ORIGINAL SOURCE!!!  This guy is from nexenta)
 * and https://github.com/xl0/illumos-gate/branches (here are his merges with dmitry yusupov)
 *
 * Alexey Zaytsev also worked with ^
 * Here is his repo: https://bitbucket.org/xl0/illumos-gate/commits/all
 *
 * Cyril Plisko from 2011
 * https://github.com/imp/solaris-virtio/blob/master/usr/src/uts/common/sys/virtio_ring.h
 *
 */









/**
 * ----------------------------------------------------------------------------------------
 * BEGIN DATA STRUCTURES
 */



struct vioscsi_buffer {
    uint8_t	state;                      /* state of the buffer - allocated/free */
    caddr_t	buffer_virt;                /* virtual address of the buffer */
    ddi_dma_handle_t buffer_dmah;       /*  DMA handle */
    ddi_dma_cookie_t buffer_dmac;       /* first cookie in the chain */
    ddi_acc_handle_t buffer_acch;       /* access handle for DMA buffer memory */
    unsigned int	buffer_ncookies;    /* number of cookies */
    u_int		buffer_nwins;           /* number of DMA windows */
    size_t		buffer_size;            /* total buffer size */
};

struct vioscsi_request {

    struct scsi_pkt *req_pkt;           /* SCSA packet we are servicing */
    struct vq_entry  *req_ve;           /* VQ entry we are using */
    /* first buffer is for virtio scsi headers/stuff */
    /* second one - for data payload */
    struct vioscsi_buffer virtio_headers_buf;

    boolean_t dir;                      /* request direction (to/from HBA) */
    int polling_done;                   /* true if the request is completed */

    unsigned char scbp[DEFAULT_SCBLEN];
    unsigned char cdbp[DEFAULT_CDBLEN];
};

struct vioscsi_ld {
    dev_info_t	*dip;
    uint8_t		lun_type;
    uint8_t		reserved[3];
};

struct vioscsi_softc {
    dev_info_t                  *sc_dev;        /* mirrors virtio_softc->sc_dev */
    struct virtio_softc         sc_virtio;
    uint64_t                    sc_features;
    struct virtqueue            *sc_control_vq;
    struct virtqueue            *sc_event_vq;
    struct virtqueue            *sc_request_vq;
    scsi_hba_tran_t             *sc_hba_tran;
    uint32_t                    sc_max_channel;
    uint32_t                    sc_max_target;
    uint32_t                    sc_max_lun;
    uint32_t                    sc_cdb_size;
    uint32_t                    sc_max_seg;
    /* maximal number of requests */
    uint32_t                    sc_max_req;
    struct vioscsi_ld           sc_ld[VIRTIO_SCSI_CONFIG_MAX_TARGET];
    struct vioscsi_buffer       event_buffers[4];
};

static ddi_device_acc_attr_t vioscsi_acc_attr = {
    DDI_DEVICE_ATTR_V0,
    DDI_NEVERSWAP_ACC,	/* virtio is always native byte order */
    DDI_STORECACHING_OK_ACC,
    DDI_DEFAULT_ACC
};

/* DMA attr for the data blocks. */
static ddi_dma_attr_t vioscsi_data_dma_attr = {
    DMA_ATTR_V0,			/* dma_attr version	*/
    0,                      /* dma_attr_addr_lo	*/
    0xFFFFFFFFFFFFFFFFull,	/* dma_attr_addr_hi	*/
    0x00000000FFFFFFFFull,	/* dma_attr_count_max	*/
    1,                      /* dma_attr_align	*/
    1,                      /* dma_attr_burstsizes	*/
    1,                      /* dma_attr_minxfer	*/
    4096,                   /* dma_attr_maxxfer, set in attach */
    0xFFFFFFFFFFFFFFFFull,	/* dma_attr_seg		*/
    64,                     /* dma_attr_sgllen, set in attach */
    1,                      /* dma_attr_granular	*/
    0,                      /* dma_attr_flags	*/
};



// *** KILL THIS, WHAT THE HECK WITH ddi_set_driver_private anyway?
static struct vioscsi_softc *global_virtio_scsi_softc;

/**
 * END DATA STRUCTURES
 * ----------------------------------------------------------------------------------------
 */







/**
 * ----------------------------------------------------------------------------------------
 * BEGIN SCSA CALLBACKS
 */


/*
    See Here: http://illumos.org/books/wdd/scsihba-32898.html#scsihba-32898
    Function Name               Called as a Result of
    tran_abort(9E)              Target driver calling scsi_abort(9F)
    tran_bus_reset(9E)          System resetting bus
    tran_destroy_pkt(9E)        Target driver calling scsi_destroy_pkt(9F)
    tran_dmafree(9E)            Target driver calling scsi_dmafree(9F)
    tran_getcap(9E)             Target driver calling scsi_ifgetcap(9F)
    tran_init_pkt(9E)           Target driver calling scsi_init_pkt(9F)
    tran_quiesce(9E)            System quiescing bus
    tran_reset(9E)              Target driver calling scsi_reset(9F)
    tran_reset_notify(9E)       Target driver calling scsi_reset_notify(9F)
    tran_setcap(9E)             Target driver calling scsi_ifsetcap(9F)
    tran_start(9E)              Target driver calling scsi_transport(9F)
    tran_sync_pkt(9E)           Target driver calling scsi_sync_pkt(9F)
    tran_tgt_free(9E)           System detaching target device instance
    tran_tgt_init(9E)           System attaching target device instance
    tran_tgt_probe(9E)          Target driver calling scsi_probe(9F)
    tran_unquiesce(9E)          System resuming activity on bus
*/

/*
 * heres what we register:
    // setup the hba transport callbacks!
    hba_tran                        = scsi_hba_tran_alloc(devinfo, SCSI_HBA_CANSLEEP);
    sc->sc_hba_tran                 = hba_tran;
    hba_tran->tran_hba_len          = sizeof(struct vioscsi_request);
    hba_tran->tran_hba_private      = sc;
    hba_tran->tran_tgt_private      = NULL;
    hba_tran->tran_tgt_init         = vioscsi_tran_tgt_init;
    hba_tran->tran_tgt_probe        = vioscsi_tran_tgt_probe;
    hba_tran->tran_tgt_free         = vioscsi_tran_tgt_free;

    hba_tran->tran_start            = vioscsi_tran_start;
    hba_tran->tran_abort            = vioscsi_tran_abort;
    hba_tran->tran_reset            = vioscsi_tran_reset;
    hba_tran->tran_getcap           = vioscsi_tran_getcap;
    hba_tran->tran_setcap           = vioscsi_tran_setcap;

    hba_tran->tran_setup_pkt        = vioscsi_tran_setup_pkt;
    hba_tran->tran_teardown_pkt     = vioscsi_tran_teardown_pkt;
    hba_tran->tran_pkt_constructor  = vioscsi_tran_pkt_constructor;
    hba_tran->tran_pkt_destructor   = vioscsi_tran_pkt_destructor;

    hba_tran->tran_dmafree          = vioscsi_tran_dma_free;
    hba_tran->tran_sync_pkt         = vioscsi_tran_sync_pkt;
    hba_tran->tran_reset_notify     = vioscsi_tran_reset_notify;
    hba_tran->tran_quiesce          = vioscsi_tran_bus_quiesce;
    hba_tran->tran_unquiesce        = vioscsi_tran_bus_unquiesce;
    hba_tran->tran_bus_reset        = vioscsi_tran_bus_reset;
    hba_tran->tran_bus_config       = vioscsi_tran_bus_config;
 */


// ----------------- THESE NEED WORK
// done
static int vioscsi_tran_abort(struct scsi_address *ap, struct scsi_pkt *pkt) {
    /* IMO WE DON'T need tran_abort for VIRTIO_SCSI case */
    printf("%s: called\n", __func__);
    return DDI_FAILURE;
}

// done
static int vioscsi_tran_bus_reset(dev_info_t *hba_dip, int level) {
    /* TODO: implement tran_bus_reset? */
    printf("%s: called\n", __func__);
    return DDI_FAILURE;
}

// done
static int vioscsi_tran_bus_quiesce(dev_info_t *hba_dip) {
    printf("%s: called\n", __func__);
    return DDI_SUCCESS;
}

// done
static int vioscsi_tran_reset(struct scsi_address *ap, int level) {
    printf("%s: called\n", __func__);
    return DDI_FAILURE;
}

// done
static int vioscsi_tran_reset_notify(struct scsi_address *ap, int flags, void (*callback)(caddr_t), caddr_t arg) {
    printf("%s: called\n", __func__);
    return DDI_FAILURE;
}

// done
static void vioscsi_tran_sync_pkt(struct scsi_address *ap, struct scsi_pkt *pkt) {
    printf("%s: called\n", __func__);
    return;
}

// done
static void vioscsi_tran_tgt_free(dev_info_t *hba_dip, dev_info_t *tgt_dip, scsi_hba_tran_t *hba_tran, struct scsi_device *sd) {
    printf("%s: called\n", __func__);
    return;
}

// done
static int vioscsi_tran_bus_unquiesce(dev_info_t *hba_dip) {
    printf("%s: called\n", __func__);
    return DDI_SUCCESS;
}


/* not implemented */
/* static void vioscsi_tran_destroy_pkt(struct scsi_address *ap, struct scsi_pkt *pkt); */

// ------------------- END THESE NEED WORK







// done
static int vioscsi_tran_getcap(struct scsi_address *ap, char *cap, int whom) {
    printf("%s: called\n", __func__);
    int rval = 0;
    struct vioscsi_softc *sc = ap->a_hba_tran->tran_hba_private;

    if (cap == NULL) {
        printf("%s: cap was NULL, returning -1\n", __func__);
        return (-1);
    }


    /*SCSI_CAP_ARQ;
    SCSI_CAP_ASCII;
    SCSI_CAP_CDB_LEN;
    SCSI_CAP_DISCONNECT;
    SCSI_CAP_DMA_MAX;
    SCSI_CAP_DMA_MAX_ARCH;
    SCSI_CAP_GEOMETRY;
    SCSI_CAP_INITIATOR_ID;
    SCSI_CAP_INTERCONNECT_TYPE;
    SCSI_CAP_LINKED_CMDS;
    SCSI_CAP_LUN_RESET;
    SCSI_CAP_MSG_OUT;
    SCSI_CAP_PARITY;
    SCSI_CAP_QFULL_RETRIES;
    SCSI_CAP_QFULL_RETRY_INTERVAL;
    SCSI_CAP_RESET_NOTIFICATION;
    SCSI_CAP_SCSI_VERSION;
    SCSI_CAP_SECTOR_SIZE;
    SCSI_CAP_SYNCHRONOUS;
    SCSI_CAP_TAGGED_QING;
    SCSI_CAP_TOTAL_SECTORS;
    SCSI_CAP_TRAN_LAYER_RETRIES;
    SCSI_CAP_UNTAGGED_QING;
    SCSI_CAP_WIDE_XFER;*/

    switch (scsi_hba_lookup_capstr(cap)) {
    case SCSI_CAP_DMA_MAX:
        rval = 4096;
        printf("%s: SCSI_CAP_DMA_MAX\n", __func__);
        break;

    case SCSI_CAP_MSG_OUT:
        rval = 1;
        printf("%s: SCSI_CAP_MSG_OUT\n", __func__);
        break;

    case SCSI_CAP_DISCONNECT:
        rval = 0;
        printf("%s: SCSI_CAP_DISCONNECT\n", __func__);
        break;

    case SCSI_CAP_SYNCHRONOUS:
        rval = 0;
        printf("%s: SCSI_CAP_SYNCHRONOUS\n", __func__);
        break;

    case SCSI_CAP_WIDE_XFER:
        rval = 1;
        printf("%s: SCSI_CAP_WIDE_XFER\n", __func__);
        break;

    case SCSI_CAP_TAGGED_QING:
        rval = 1;
        printf("%s: SCSI_CAP_TAGGED_QING\n", __func__);
        break;

    case SCSI_CAP_UNTAGGED_QING:
        rval = 1;
        printf("%s: SCSI_CAP_UNTAGGED_QING\n", __func__);
        break;

    case SCSI_CAP_PARITY:
        rval = 1;
        printf("%s: SCSI_CAP_PARITY\n", __func__);
        break;

    case SCSI_CAP_LINKED_CMDS:
        rval = 0;
        printf("%s: SCSI_CAP_LINKED_CMDS\n", __func__);
        break;

    case SCSI_CAP_GEOMETRY:
        rval = -1;
        printf("%s: SCSI_CAP_GEOMETRY\n", __func__);
        break;

    case SCSI_CAP_CDB_LEN:
        rval = sc->sc_cdb_size;
        printf("%s: SCSI_CAP_CDB_LEN\n", __func__);
        break;

    default:
        rval = -1;
    }

    printf("%s: returning %d\n", __func__, rval);
    return (rval);
}

// done
static int vioscsi_tran_setcap(struct scsi_address *ap, char *cap, int value, int whom) {
    printf("%s: called\n", __func__);
    int rval = 1;

    if (cap == NULL || whom == 0) {
        return (-1);
    }
    switch (scsi_hba_lookup_capstr(cap)) {
        default: {
            rval = 1;
        }
    }
    printf("%s: returning %d\n", __func__, rval);
    return (rval);
}



// ****************************** NOT YET -- DONE?  Tran_start sends us into a loop never returning.
// helpers for vioscsi_tran_start
uint_t vioscsi_intr_handler(caddr_t arg1, caddr_t arg2) {
    printf("%s: called\n", __func__);

    struct virtio_softc *vsc = (void *) arg1;
    struct vioscsi_softc *sc = container_of(vsc, struct vioscsi_softc, sc_virtio);
    struct vq_entry *ve;
    struct vioscsi_request *req;
    struct vioscsi_cmd_resp *resp;
    struct scsi_arq_status *arqstat;
    struct scsi_pkt *pkt;
    uint32_t len;
    struct vioscsi_buffer *req_buf = NULL;

    /* TODO: push request into the ready queue and schedule taskq */
    printf("%s: entering while loop\n", __func__);

    while ((ve = virtio_pull_chain(sc->sc_request_vq, &len))) {
         //req = virtio_get_private(ve);
        req = ve->qe_private;
        ve->qe_private = NULL;

        pkt = req->req_pkt;

        req_buf = &req->virtio_headers_buf;

        resp = (struct vioscsi_cmd_resp *)(req_buf->buffer_virt + sizeof(struct vioscsi_cmd_req));

        /* TODO: translate virtio SCSI responses into the SCSA status codes */
        switch (resp->response) {

            /* virtio scsi processes request sucessfully, check the request SCSI status */
            case VIRTIO_SCSI_S_OK:

                switch (resp->status) {
                    case 0:
                    /* ok, request processed by host SCSI */
                        pkt->pkt_scbp[0] = STATUS_GOOD;
                        break;
                    default:
                        ((struct scsi_status *)pkt->pkt_scbp)->sts_chk = 1;
                        if (pkt->pkt_cdbp[0] != SCMD_TEST_UNIT_READY) {
                            pkt->pkt_state |= STATE_ARQ_DONE;
                            arqstat = (void *)(pkt->pkt_scbp);
                            arqstat->sts_rqpkt_reason = CMD_CMPLT;
                            arqstat->sts_rqpkt_resid = 0;
                            arqstat->sts_rqpkt_state = STATE_GOT_BUS | STATE_GOT_TARGET | STATE_SENT_CMD | STATE_XFERRED_DATA;
                            *(uint8_t *)&arqstat->sts_rqpkt_status = STATUS_GOOD;
                            (void) memcpy(&arqstat->sts_sensedata, resp->sense, resp->sense_len);
                        }
                }
                pkt->pkt_resid = 0;
                pkt->pkt_state |= STATE_XFERRED_DATA;
                pkt->pkt_reason = CMD_CMPLT;

                break;
            default:
                pkt->pkt_reason = CMD_TRAN_ERR;
        }
        /* if packet is processed in polling mode - notify the caller that it may done */
        /* no races, because in this case we are not invoked by virtio interrupt */
        req->polling_done = 1;

        scsi_hba_pkt_comp(pkt);

        virtio_free_chain(ve);
    }
    printf("%s: while loop complete\n", __func__);
    printf("%s: returning DDI_INTR_CLAIMED\n", __func__);
    return (DDI_INTR_CLAIMED);
}

// RIGHT HERE TOO!
static int vioscsi_register_ints(struct vioscsi_softc *sc) {
    printf("%s: called\n", __func__);
    int ret;

    struct virtio_int_handler virtio_scsi_intr_h[] = {
        { vioscsi_intr_handler },
        { NULL },
    };

    ret = virtio_register_ints(&sc->sc_virtio, NULL, virtio_scsi_intr_h);

    printf("%s: returning %x\n", __func__, ret);
    return (ret);
}

// RIGHT HERE!
static int vioscsi_tran_start(struct scsi_address *ap, struct scsi_pkt *pkt) {
    printf("%s: called\n", __func__);
    //if (pkt->pkt_flags & (1 << FLAG_NOINTR)) {
    //    printf("%s: we have FLAG_NOINTR\n", __func__);
    //} else {
    //    printf("%s: no FLAG_NOINTR\n", __func__);
    //}



    /*
     * these are the cmd structures
     * //virtio SCSI command request
    struct vioscsi_cmd_req {
        uint8_t lun[8];                                             // logical unit number
        uint64_t tag;                                               // command identifier
        uint8_t	task_attr;                                          // task attribute
        uint8_t	prio;
        uint8_t crn;
        uint8_t cdb[VIRTIO_SCSI_CDB_SIZE];
    } __packed;

    virtio SCSI response, followed by sense data and data-in
    struct vioscsi_cmd_resp {
        uint32_t sense_len;                                         // Sense data length
        uint32_t res_id;                                            // Residual bytes in data buffer
        uint16_t status_qualifier;                                  // status qualifier
        uint8_t	status;                                             // command completion status
        uint8_t response;                                           // response values
        uint8_t sense[VIRTIO_SCSI_SENSE_SIZE];
    } __packed;
   */


  /** Heres what atapi looks like:
  0  -> scsi_transport                        scsi scsi_transport 36270910697339
  0    -> atapi_tran_start                    ata atapi_tran_start 36270910709172
  0      -> atapi_fsm_start                   ata atapi_fsm_start 36270910717759
  0        -> atapi_start_cmd                 ata atapi_start_cmd 36270910721188
  0        <- atapi_start_cmd                 ata atapi_start_cmd 36270910997410
  0      <- atapi_fsm_start                   ata atapi_fsm_start 36270911002807
  0      -> atapi_fsm_intr                    ata atapi_fsm_intr 36270911006439
  0        -> atapi_send_cdb                  ata atapi_send_cdb 36270911109713
  0        <- atapi_send_cdb                  ata atapi_send_cdb 36270911346976
  0        -> atapi_start_dma                 ata atapi_start_dma 36270911351763
  0        <- atapi_start_dma                 ata atapi_start_dma 36270911495789
  0      <- atapi_fsm_intr                    ata atapi_fsm_intr 36270911500028
  0      -> atapi_fsm_intr                    ata atapi_fsm_intr 36270911599890
  0        -> atapi_status                    ata atapi_status 36270911845297
  0        <- atapi_status                    ata atapi_status 36270911850312
  0      <- atapi_fsm_intr                    ata atapi_fsm_intr 36270911853101
  0      -> atapi_complete                    ata atapi_complete 36270911870732
  0        -> scsi_watch_request_intr         scsi scsi_watch_request_intr 36270911874610
  0          -> sd_media_watch_cb             sd sd_media_watch_cb 36270911879967
  0            -> sd_gesn_media_data_valid    sd sd_gesn_media_data_valid 36270911887288
  0            <- sd_gesn_media_data_valid    sd sd_gesn_media_data_valid 36270911890485
  0          <- sd_media_watch_cb             sd sd_media_watch_cb 36270911895269
  0        <- scsi_watch_request_intr         scsi scsi_watch_request_intr 36270911898147
  0      <- atapi_complete                    ata atapi_complete 36270911903780
  0    <- atapi_tran_start                    ata atapi_tran_start 36270911907589
  0  <- scsi_transport                        scsi scsi_transport 36270911911576
  */


    struct vq_entry *ve;
    struct vioscsi_request *req = pkt->pkt_ha_private;
    struct vioscsi_softc *sc = ap->a_hba_tran->tran_hba_private;
    struct vioscsi_cmd_req *cmd_req = NULL;

    struct vioscsi_buffer *req_buf = &req->virtio_headers_buf;
    int i;

    cmd_req = (struct vioscsi_cmd_req *)req_buf->buffer_virt;

    /* fill in cmd_req */
    cmd_req->lun[0] = 1;
    cmd_req->lun[1] = ap->a_target;
    cmd_req->lun[2] = (ap->a_lun >> 8) | 0x40;
    cmd_req->lun[3] = (ap->a_lun & 0xff);
    cmd_req->tag = (unsigned long)pkt;
    cmd_req->task_attr = 0;
    cmd_req->prio = 0;
    cmd_req->crn = 0;

    printf("%s: cmd_req->lun[0]==%d\n", __func__, cmd_req->lun[0]);
    printf("%s: cmd_req->lun[1]==%d\n", __func__, cmd_req->lun[1]);
    printf("%s: cmd_req->lun[2]==%x\n", __func__, cmd_req->lun[2]);
    printf("%s: cmd_req->lun[3]==%x\n", __func__, cmd_req->lun[3]);
    printf("%s: cmd_req->tag == OMITTING\n", __func__); /*, cmd_req->tag); */
    printf("%s: cmd_req->task_attr == %d\n", __func__, cmd_req->task_attr);
    printf("%s: cmd_req->prio == %d\n", __func__, cmd_req->prio);
    printf("%s: cmd_req->crn == %d\n", __func__, cmd_req->crn);


    if (pkt->pkt_cdbp == NULL) {
        printf("%s pkt with NULL CDB! pkt at 0x%p returning TRAN_BADPKT\n", __func__, (void *)pkt);
        return (TRAN_BADPKT);
    }

    (void) memcpy(cmd_req->cdb, pkt->pkt_cdbp, pkt->pkt_cdblen);

    /* allocate vq_entry */
    ve = vq_alloc_entry(sc->sc_request_vq);

    if (ve == NULL) {
        /* TODO: remove debug output, count some statistic */
        /* TODO: shall we implement some queuing logic here ? */
        /* i.e. queue request if there is no space in the VIRTIO queues */
        /* but just return TRAN_BUSY for now */
        /* TODO: do not forget to count some statistic! */
        printf("%s: cannot allocate VE entry!, returning TRAN_BUSY\n", __func__);
        return (TRAN_BUSY);
    }

    /* add request header */
    virtio_ve_add_indirect_buf(ve, req_buf->buffer_dmac.dmac_laddress, sizeof(struct vioscsi_cmd_req), B_TRUE);
    /* and some space for response */
    virtio_ve_add_indirect_buf(ve, req_buf->buffer_dmac.dmac_laddress + sizeof(struct vioscsi_cmd_req), sizeof(struct vioscsi_cmd_resp), B_FALSE);

    /* add some payload, if any */
    printf("%s: checking to see if pkt_numcookies\n", __func__);
    if (pkt->pkt_numcookies) {
        printf("%s: in pkt_numcookies\n", __func__);
        ddi_dma_cookie_t *dmac;
        for (i = 0; i < pkt->pkt_numcookies; i ++) {
            printf("%s: looping thru pkg_num_cookies, and calling virtio_ve_add_indirect_buf\n", __func__);
            dmac = &pkt->pkt_cookies[i];
            virtio_ve_add_indirect_buf(ve, dmac->dmac_laddress, dmac->dmac_size, pkt->pkt_dma_flags & DDI_DMA_WRITE);
        }
    }
    /* FIXME: use virtio_set_private stuff instead of directly pointing */
    printf("%s: putting req on ve->qe_private\n", __func__);
    ve->qe_private = req;
    //	virtio_set_private(ve, req);

    /* push vq_entry into the queue */


    // this happens.
    printf("%s: calling virtio_push_chain\n", __func__);
    virtio_push_chain(ve, B_TRUE);
    printf("%s: done calling virtio_push_chain\n", __func__);


    // getting warmer?!
    /*
     * TIME                           UUID                                 SUNW-MSG-ID
        Nov 03 2018 19:27:06.895201000 dcb2bb5a-e57b-e8f7-9eb6-ad210c142711 SUNOS-8000-KL

          TIME                 CLASS                                 ENA
          Nov 03 19:27:05.9331 ireport.os.sunos.panic.dump_available 0x0000000000000000
          Nov 03 19:27:03.7161 ireport.os.sunos.panic.dump_pending_on_device 0x0000000000000000

            nvlist version: 0
                    version = 0x0
                    class = list.suspect
                    uuid = dcb2bb5a-e57b-e8f7-9eb6-ad210c142711
                    code = SUNOS-8000-KL
                    diag-time = 1541273226 883458
                    de = fmd:///module/software-diagnosis
                    fault-list-sz = 0x1
                    fault-list = (array of embedded nvlists)
                    (start fault-list[0])
                    nvlist version: 0
                            version = 0x0
                            class = defect.sunos.kernel.panic
                            certainty = 0x64
                            asru = sw:///:path=/var/crash//.dcb2bb5a-e57b-e8f7-9eb6-ad210c142711
                            resource = sw:///:path=/var/crash//.dcb2bb5a-e57b-e8f7-9eb6-ad210c142711
                            savecore-succcess = 1
                            dump-dir = /var/crash/
                            dump-files = vmdump.0
                            os-instance-uuid = dcb2bb5a-e57b-e8f7-9eb6-ad210c142711
                            panicstr = BAD TRAP: type=e (#pf Page fault) rp=fffffe0003540e00 addr=8 occurred in module "genunix" due to a NULL pointer dereference
                            panicstack =
                                unix:real_mode_stop_cpu_stage2_end+bc1c () |
                                unix:trap+e08 () |
                                unix:cmntrap+e6 () |
                                genunix:list_remove_head+2c () |
                                virtio:vq_alloc_entry+43 () |
                                vioscsi:vioscsi_tran_start+175 () |
                                scsi:scsi_transport+7b () |
                                sd:sd_start_cmds+1ab () |
                                sd:sd_send_request_sense_command+c1 () |
                                sd:sdintr+1d2 () |
                                scsi:scsi_hba_pkt_comp+63 () |
                                vioscsi:vioscsi_intr_handler+8d () |
                                vioscsi:vioscsi_tran_start+337 () |
                                scsi:scsi_transport+7b () |
                                sd:sd_start_cmds+1ab () |
                                sd:sd_core_iostart+90 () |
                                sd:sd_pm_iostart+4a () |
                                sd:sd_uscsi_strategy+fa () |
                                scsi:scsi_uscsi_handle_cmd+11f () |
                                sd:sd_ssc_send+136 () |
                                sd:sd_send_scsi_TEST_UNIT_READY+121 () |
                                sd:sd_unit_attach+731 () |
                                sd:sdattach+19 () |
                                genunix:devi_attach+92 () |
                                genunix:attach_node+a7 () |
                                genunix:i_ndi_config_node+7d () |
                                genunix:i_ddi_attachchild+48 () |
                                genunix:devi_attach_node+5e () |
                                genunix:ndi_devi_online+9a () |
                                vioscsi:vioscsi_config_child+1dc () |
                                vioscsi:vioscsi_config_lun+1f3 () |
                                vioscsi:vioscsi_tran_bus_config+1c4 () |
                                scsi:scsi_hba_bus_config+70 () |
                                genunix:devi_config_common+a5 () |
                                genunix:mt_config_thread+58 () |
                                unix:thread_start+8 () |



                            crashtime = 1541273143
                            panic-time = Sat Nov  3 19:25:43 2018 UTC
                    (end fault-list[0])

                    fault-status = 0x1
                    severity = Major
                    __ttl = 0x1
                    __tod = 0x5bddf68a 0x355baee8
        */




    if (pkt->pkt_flags & FLAG_NOINTR) {
        printf("%s: we have a FLAG_NOINTR in pkt_flags\n", __func__);
        /* disable interrupts for a while */
        virtio_stop_vq_intr(sc->sc_request_vq);

        /* TODO: add timeout here */
        while (req->polling_done == 0) {
            printf("%s: req->polling_done == 0, so looping, and calling vioscsi_intr_handler\n", __func__);
            (void) vioscsi_intr_handler((caddr_t)&sc->sc_virtio, NULL);
            drv_usecwait(100);
        }
        printf("%s: polling is done, calling virtio_start_vq_intr\n", __func__);
        req->polling_done = 0;
        virtio_start_vq_intr(sc->sc_request_vq);
    } else {
        printf("%s: we do not have a FLAG_NOINTR in pkt_flags\n", __func__);
        /* disable interrupts for a while */
        virtio_stop_vq_intr(sc->sc_request_vq);

        /* TODO: add timeout here */
        while (req->polling_done == 0) {
            printf("%s: req->polling_done == 0, so looping, and calling vioscsi_intr_handler\n", __func__);
            (void) vioscsi_intr_handler((caddr_t)&sc->sc_virtio, NULL);
            drv_usecwait(100);
        }
        printf("%s: polling is done, calling virtio_start_vq_intr\n", __func__);
        req->polling_done = 0;
        virtio_start_vq_intr(sc->sc_request_vq);
    }


    printf("%s: returning TRAN_ACCEPT\n", __func__);
    return (TRAN_ACCEPT);
}

// helper for viosci_tran_tgt_init()
static int vioscsi_name_node(dev_info_t *dip, char *name, int len) {
    printf("%s: called\n", __func__);

    int tgt, lun;

    tgt = ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS, "target", -1);

    if (tgt == -1) {
        printf("%s: returning DDI_FAILURE because tgt == -1\n", __func__);
        return (DDI_FAILURE);
    }

    lun = ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS, "lun", -1);

    if (lun == -1) {
        printf("%s returning DDI_FAILURE because lun == -1\n", __func__);
        return (DDI_FAILURE);
    }

    (void) snprintf(name, len, "%x,%x", tgt, lun);

    printf("%s: returning DDI_SUCCESS with tgt=%x, lun=%x\n", __func__, tgt, lun);

    return (DDI_SUCCESS);
}

// helper for vioscsi_tran_tgt_init()
static dev_info_t* vioscsi_find_child(struct vioscsi_softc *sc, uint16_t tgt, uint8_t lun) {
    printf("%s: called\n", __func__);

    dev_info_t *child = NULL;
    char addr[SCSI_MAXNAMELEN];
    char tmp[MAXNAMELEN];

    if (tgt < sc->sc_max_target) {
        printf("%s: tgt < sc->sc_max_target\n", __func__);
        if (sc->sc_ld[tgt].dip != NULL) {
            child = sc->sc_ld[tgt].dip;
            printf("%s: found child becuase sc->sc_ld[tgt].dip != NULL\n", __func__);
        } else {
            (void) sprintf(addr, "%x,%x", tgt, lun);

            printf("%s: entering ddi_get_child..ddi_get_next_sibling() loop\n", __func__);
            for (child = ddi_get_child(sc->sc_dev); child; child = ddi_get_next_sibling(child)) {

                printf("%s: loop\n", __func__);

                if (ndi_dev_is_persistent_node(child) == 0) {
                    printf("%s: loop continue bc ndi_dev_is_persistent_node(child)\n", __func__);
                    continue;
                }

                if (vioscsi_name_node(child, tmp,sizeof(tmp)) != DDI_SUCCESS) {
                    printf("%s: loop continue bc vioscsi_name_node(child, tmp,sizeof(tmp)) != DDI_SUCCESS\n", __func__);
                    continue;
                }

                if (strcmp(addr, tmp) == 0) {
                    printf("%s: breaking loop because strcmp(addr,tmp) == 0\n", __func__);
                    break;
                }
            }
            printf("%s: exiting ddi_get_child..ddi_get_next_sibling() loop\n", __func__);
        }
    }
    printf("%s: returning\n", __func__);
    return child;
}

// done
static int vioscsi_tran_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip, scsi_hba_tran_t *hba_tran, struct scsi_device *sd) {
    printf("%s: called\n", __func__);
    struct vioscsi_softc *sc = sd->sd_address.a_hba_tran->tran_hba_private;

    uint16_t tgt = sd->sd_address.a_target;
    uint8_t lun = sd->sd_address.a_lun;

    if (ndi_dev_is_persistent_node(tgt_dip)) {
        if (vioscsi_find_child(sc, tgt, lun) != NULL) {
            if (ndi_merge_node(tgt_dip, vioscsi_name_node) != DDI_SUCCESS) {
                printf("%s: returning DDI_SUCCESS because ndi_merge_node() worked\n", __func__);
                return (DDI_SUCCESS);
            }
        }
        printf("vioscsi_tran_tgt_init returning DDI_SUCCESS\n");
        return (DDI_SUCCESS);
    }

    if (tgt > sc->sc_max_target) {
        printf("%s: returning DDI_FAILURE because (tgt > sc->sc_max_target)\n", __func__);
        return (DDI_FAILURE);
    }

    if (lun != 0 && (sc->sc_ld[tgt].dip == NULL)) {
        printf("%s: returning DDI_FAILURE because (lun != 0 && (sc->sc_ld[tgt].dip == NULL))\n", __func__);
        return (DDI_FAILURE);
    }

    sc->sc_ld[tgt].dip = tgt_dip;

    printf("%s: returning DDI_SUCCESS\n", __func__);
    return (DDI_SUCCESS);
}

// done
static int vioscsi_tran_tgt_probe(struct scsi_device *sd, int (*waitfunc)(void)) {
    printf("%s: called\n", __func__);
    return scsi_hba_probe(sd, waitfunc);
}















































// done
// https://illumos.org/man/9E/tran_setup_pkt
// https://illumos.org/man/9S/scsi_pkt
static int vioscsi_tran_setup_pkt(struct scsi_pkt *pkt, int (*callback)(caddr_t), caddr_t cbarg) {
    printf("%s: called\n", __func__);
    /* nothing to do, all resources are already preallocated from tran_pkt_constructor */
    return 0;
}

// done
static void vioscsi_tran_teardown_pkt(struct scsi_pkt *pkt) {
    printf("%s: called\n", __func__);
    return;
}

/* preallocate DMA handles and stuff for requests */
/* TODO: update vioscsi_scsi_buffer_setup to take into account kmflags */
static int vioscsi_tran_pkt_constructor(struct scsi_pkt *pkt, scsi_hba_tran_t *tran, int kmflags) {
    printf("%s: called\n", __func__);
    struct vioscsi_request *req = pkt->pkt_ha_private;
    struct vioscsi_softc *sc;
    sc = tran->tran_hba_private;

    printf("%s: calling memset\n", __func__);
    (void) memset(req, 0, sizeof(*req));
    req->req_pkt = pkt;

    struct vioscsi_buffer *buf;
    buf = &req->virtio_headers_buf;
    printf("%s: setting buf->state to VIRTIO_SCSI_BUFFER_FREE\n", __func__);
    buf->state = VIRTIO_SCSI_BUFFER_FREE;

    // inlining
    int err;
    size_t buffer_size = 1024;
    size_t len;

    if (buf->state != VIRTIO_SCSI_BUFFER_FREE) {
        printf("%s: THIS WILL NEVER HAPPEN LOOK ABOVE.\n", __func__);
        return ENOMEM;
    }

    err = ddi_dma_alloc_handle(sc->sc_dev, &vioscsi_data_dma_attr, DDI_DMA_SLEEP, NULL, &buf->buffer_dmah);
    if (err != DDI_SUCCESS) {
        printf("%s: ddi_dma_alloc_handle_failed.  returning ENOMEM\n", __func__);
        return ENOMEM;
    }

    err = ddi_dma_mem_alloc(buf->buffer_dmah, buffer_size, &vioscsi_acc_attr, DDI_DMA_STREAMING, DDI_DMA_SLEEP, NULL, &buf->buffer_virt, &len, &buf->buffer_acch);
    if (err != DDI_SUCCESS) {
        printf("%s: ddi_dma_mem_alloc_failed, releasing, and returning ENOMEM\n", __func__);
        goto unbind_handle;
    }

    err = ddi_dma_addr_bind_handle(buf->buffer_dmah, NULL, buf->buffer_virt, len, DDI_DMA_READ | DDI_DMA_WRITE, DDI_DMA_SLEEP, NULL, &buf->buffer_dmac, &buf->buffer_ncookies);
    if (err != DDI_SUCCESS) {
        printf("%s: ddi_dma_addr_bind_handle failed, releasing, and returning ENOMEM\n", __func__);
        goto release_dma_mem;
    }

    // Success Case, we made it to here.
    buf->state = VIRTIO_SCSI_BUFFER_ALLOCATED;
    buf->buffer_size = buffer_size;
    printf("%s: We made the success case, continuing.\n", __func__);

//    struct vioscsi_request {
//    struct scsi_pkt *req_pkt;           /* SCSA packet we are servicing */
//    struct vq_entry  *req_ve;           /* VQ entry we are using */
//    /* first buffer is for virtio scsi headers/stuff */
//    /* second one - for data payload */
//    struct vioscsi_buffer virtio_headers_buf;

//    boolean_t dir;                      /* request direction (to/from HBA) */
//    int polling_done;                   /* true if the request is completed */

//    unsigned char scbp[DEFAULT_SCBLEN];
//    unsigned char cdbp[DEFAULT_CDBLEN];
//    };

    // interrogate the packet a bit.
    printf("%s: req->dir == %d", __func__, req->dir);
    printf("%s: req->polling_done == %d", __func__, req->polling_done);
    printf("%s: req->cdbp == %s", __func__, req->cdbp);
    printf("%s: req->sdbp == %s", __func__, req->scbp);

    return 0;

unbind_handle:
        (void)ddi_dma_unbind_handle(buf->buffer_dmah);

release_dma_mem:
        (void)ddi_dma_mem_free(&buf->buffer_acch);

        // end inlining.

    return ENOMEM;
}


// helper for vioscsi_tran_dma_free() and vioscsi_tran_pkt_destructor
static void vioscsi_buffer_release(struct vioscsi_buffer *vb) {
    if (vb->state != VIRTIO_SCSI_BUFFER_ALLOCATED) {
        return;
    }

    (void) ddi_dma_unbind_handle(vb->buffer_dmah);

    if (vb->buffer_acch) {
        (void) ddi_dma_mem_free(&vb->buffer_acch);
    }

    (void) ddi_dma_free_handle(&vb->buffer_dmah);
    vb->state = VIRTIO_SCSI_BUFFER_FREE;
    return;
}

// done
static void vioscsi_tran_dma_free(struct scsi_address *ap, struct scsi_pkt *pkt) {
    printf("%s: called\n", __func__);
    struct vioscsi_request *req = pkt->pkt_ha_private;
    vioscsi_buffer_release(&req->virtio_headers_buf);
}

// done
static void vioscsi_tran_pkt_destructor(struct scsi_pkt *pkt, scsi_hba_tran_t *tran) {
    printf("%s: called\n", __func__);
    struct vioscsi_request *req = pkt->pkt_ha_private;
    struct vioscsi_softc *sc = tran->tran_hba_private;
    vioscsi_buffer_release(&req->virtio_headers_buf);
}

















































// helper for vioscsi_config_lun()
// ** THIS NEVER RETURNS?
static int vioscsi_probe_lun(struct scsi_device *sd) {
    printf("%s: called\n", __func__);
    int rval;
    int probe_result;

    printf("%s: calling scsi_hba_probe()\n", __func__);
    probe_result = scsi_hba_probe(sd, NULL_FUNC); // takes sd, and NULL_FUNC or SLEEP_FUNC
    printf("%s: after scsi_hba_probe()\n", __func__);


    rval =  (probe_result == SCSIPROBE_EXISTS) ? NDI_SUCCESS : NDI_FAILURE;

    if (rval == NDI_FAILURE) {
        printf("%s: returning NDI_FAILURE\n", __func__);
    } else {
        printf("%s: returning NDI_SUCCESS\n", __func__);
    }

    return (rval);

}

// helper for vioscsi_config_lun()
static int vioscsi_config_child(struct vioscsi_softc *sc, struct scsi_device *sd, dev_info_t **ddip) {
    printf("%s: entered\n", __func__);
    char *nodename = NULL;
    char **compatible = NULL;
    int ncompatible = 0;
    char *childname = NULL;
    dev_info_t *ldip = NULL;
    int tgt = sd->sd_address.a_target;
    int lun = sd->sd_address.a_lun;
    int dtype = sd->sd_inq->inq_dtype & DTYPE_MASK;
    int rval;

    scsi_hba_nodename_compatible_get(sd->sd_inq, NULL, dtype, NULL, &nodename, &compatible, &ncompatible);

    if (nodename == NULL) {
        printf("%s: no compatible driver for %d:%d\n", __func__, tgt, lun);
        rval = NDI_FAILURE;
        goto finish;
    }
    childname = (dtype == DTYPE_DIRECT) ? "sd" : nodename;


    rval = ndi_devi_alloc(sc->sc_dev, childname, DEVI_SID_NODEID, &ldip);

    if (rval == NDI_SUCCESS) {
#if 1
        printf("%s: ENTERING #if 1\n", __func__);
        /* TODO: replace debug output with dev_warn or something */
        if (ndi_prop_update_int(DDI_DEV_T_NONE, ldip, "target", tgt) != DDI_PROP_SUCCESS) {
            rval = NDI_FAILURE;
            printf("%s: cannot update target node\n", __func__);
            goto finish;
        }
        if (ndi_prop_update_int(DDI_DEV_T_NONE, ldip, "lun", lun) != DDI_PROP_SUCCESS) {
            rval = NDI_FAILURE;
            printf("%s: cannot update lun node\n", __func__);
            goto finish;
        }
        if (ndi_prop_update_string_array(DDI_DEV_T_NONE, ldip, "compatible", compatible, ncompatible) != DDI_PROP_SUCCESS) {
                printf("%s: cannot update compatible string array\n", __func__);
                rval = NDI_FAILURE;
                goto finish;
        }
        printf("%s: before ndi_devi_online: driver name %s\n", __func__, ddi_driver_name(ldip));
        printf("%s: EXITING #if 1\n", __func__);
#endif
        rval = ndi_devi_online(ldip, NDI_ONLINE_ATTACH);

        if (rval != NDI_SUCCESS) {
            printf("%s: unable to online\n", __func__);
            ndi_prop_remove_all(ldip);
            (void) ndi_devi_free(ldip);
        }
    }
finish:
    if (ddip) {
        *ddip = ldip;
    }
    scsi_hba_nodename_compatible_free(nodename, compatible);
    printf("%s: returning %x\n", __func__, rval);
    return (rval);
}

// helper for vioscsi_tran_bus_config()
static int vioscsi_parse_devname(char *devnm, int *tgt, int *lun) {
    printf("%s: called\n", __func__);
    char devbuf[SCSI_MAXNAMELEN];
    char *addr;
    char *p, *tp, *lp;
    long num;

    (void) strcpy(devbuf, devnm);
    addr = "";

    for (p = devbuf; *p != '\0'; p ++) {
        if (*p == '@') {
            addr = p + 1;
            *p = '\0';
        } else if (*p == ':') {
            *p = '\0';
            break;
        }
    }
    for (p = tp = addr, lp = NULL; *p != '\0'; p ++) {
        if (*p == ',') {
            lp = p + 1;
            *p = '\0';
            break;
        }
    }
    if (tgt && tp) {
        if (ddi_strtol(tp, NULL, 0x10, &num)) {
            printf("%s: returning DDI_FAILURE\n", __func__);
            return (DDI_FAILURE);
        }
        *tgt = (int)num;
    }
    if (lun && lp) {
        if (ddi_strtol(lp, NULL, 0x10, &num)) {
            printf("%s: returning DDI_FAILURE\n", __func__);
            return (DDI_FAILURE);
        }
        *lun = (int)num;
    }
    printf("%s: returning DDI_SUCCESS\n", __func__);
    return (DDI_SUCCESS);
}

// helper for vioscsi_tran_bus_config()
static int vioscsi_config_lun(struct vioscsi_softc *sc, int tgt, uint8_t lun, dev_info_t **ldip) {
    printf("%s: called\n", __func__);


    struct scsi_device sd;
    dev_info_t *child;
    int rval;

    /* doesnt enter */
    if ((child = vioscsi_find_child(sc, tgt, lun)) != NULL) {
        printf("%s: we have child!\n", __func__);
        if (ldip) {
            printf("%s: setting ldip to child\n", __func__);
            *ldip = child;
        }
        printf("%s: returning NDI_SUCCESS\n", __func__);
        return (NDI_SUCCESS);
    }
    /*end doesnt enter */

    printf("%s: bzero the sd and wire up the sd address to the target and lun.\n", __func__);
    bzero(&sd, sizeof(struct scsi_device));

    sd.sd_address.a_hba_tran = sc->sc_hba_tran;
    sd.sd_address.a_target = (uint16_t)tgt;
    sd.sd_address.a_lun = (uint8_t)lun;

    // maybe the sc is messed up..


    // is sd_address null or something?
    printf("%s: sd.sd_address.a_target = %d\n", __func__, sd.sd_address.a_target);
    printf("%s: sd.sd_address.a_lun = %d\n", __func__, sd.sd_address.a_lun);

    printf("%s: calling virtio_scsi_probe_lun()\n", __func__);
    if ((rval = vioscsi_probe_lun(&sd)) == NDI_SUCCESS) {
        printf("%s: virtio_scsi_probe_lun(&sd) == NDI_SUCCESS\n", __func__);
        rval = vioscsi_config_child(sc, &sd, ldip);
        printf("%s: NEED TO SEE THIS! after virtio_scsi_config_child()\n", __func__);
    }

    if (sd.sd_inq) {
        kmem_free(sd.sd_inq, SUN_INQSIZE);
        sd.sd_inq = NULL;
        printf("%s: freeing sd.sd_inq\n", __func__);
    }

    printf("%s: returning %x\n", __func__, rval);
    return (rval);
}

// TODO: kill the global vioscsi_softc!
static int vioscsi_tran_bus_config(dev_info_t *hba_dip, uint_t flags, ddi_bus_config_op_t op,  void *arg, dev_info_t **childs) {
    printf("%s: called\n", __func__);
    int circ;
    int ret = DDI_SUCCESS;
    int tgt, lun;

    struct vioscsi_softc *sc = global_virtio_scsi_softc;
    //struct vioscsi_softc *sc;
    //if ((sc = ddi_get_driver_private(hba_dip)) == NULL) {
    //    return DDI_FAILURE;
    //}

    ndi_devi_enter(hba_dip, &circ);

    /* TODO: investigate, and probably implement more ioclts */
    /* currently supported set is enough for sd */
    printf("%s: calling switch on op\n", __func__);
    switch (op) {
        case BUS_CONFIG_ONE:
            printf("%s: op is BUS_CONFIG_ONE\n", __func__);
            if (strchr((char *)arg, '@') == NULL) {
                ret = DDI_FAILURE;
                printf("%s: jumping to out because strchr((char*)arg, '@' == NULL\n", __func__);
                goto out;
            }

            if (vioscsi_parse_devname(arg, &tgt, &lun) != 0) {
                ret = DDI_FAILURE;
                printf("%s: jumping to out because virtio_scsi_parse_devname() != 0\n", __func__);
                goto out;
            }

            if (lun == 0) {
                ret = vioscsi_config_lun(sc, tgt, lun, childs);
                printf("%s: lun == 0, looks like a success\n", __func__);
            }
            else {
                printf("%s: lun != 0, NDI_FAILURE", __func__);
                ret = NDI_FAILURE;
            }
            goto out;

        case BUS_CONFIG_DRIVER:
        case BUS_CONFIG_ALL: {

            if (op == BUS_CONFIG_DRIVER) {
                printf("%s: op is BUS_CONFIG_DRIVER\n", __func__);
            } else if (op == BUS_CONFIG_ALL) {
                printf("%s: op is BUS_CONFIG_ALL\n", __func__);
            } else {
                printf("%s: op is unknown, should be BUS_CONFIG_DRIVER or BUS_CONFIG_ALL\n", __func__);
            }

            //uint32_t tgt; [jubal] commented this out.. it was declared above... as an int. so it's a uint32_t now.

            printf("%s: sc->max_target == %d\n", __func__, sc->sc_max_target);
            for (tgt = 0; tgt  < sc->sc_max_target; tgt++) {
                printf("%s: calling virtio_scsi_config_lun tgt==%d\n", __func__, tgt);
                (void) vioscsi_config_lun(sc, tgt, 0, NULL);
            }

        default:
            ret = NDI_FAILURE;
        }

    }
out:
    ndi_devi_exit(hba_dip, circ);
    printf("%s: returning %x\n", __func__, ret);
    return (ret);
}


/**
 * END SCSA CALLBACKS
 * ----------------------------------------------------------------------------------------
 */


















/**
  * ----------------------------------------------------------------------------------------
  * BEGIN vioscsi_dev_ops callbacks (from _init(), _fini(), _info() )
  */


/**
 * dev_t is just a typedef short dev_t
 *  ddi_create_minor_node():    Create a minor node for a device
 *  ddi_getiminor():            Get kernel internal minor number from an external dev_t
 *  ddi_remove_minor_node():    Remove a minor node for a device
 *  getmajor():                 Get major device number
 *  getminor():                 Get minor device number
 *  makedevice():               Make device number from major and minor numbers;
 *
 * dev_info_t functions:
 *  ddi_binding_name(): return driver binding name
 *  ddi_dev_is_sid():   Tell whether a device is self-identifying
 *  ddi_driver_major(): Return driver major device number
 *  ddi_driver_name():  Return normalized driver name
 *  ddi_node_name():    Return the devinfo node name
 *  ddi_get_devstate(): Check the device state
 *  ddi_get_instance(): Get device instance number
 *  ddi_get_name():     Return driver binding name
 *  ddi_get_parent():   Find the parent of a device information structure
 *  ddi_root_node():    Get the root of the dev_info tree
 */

// TODO: see FIXME!
static int vioscsi_getinfo(dev_info_t *devinfo, ddi_info_cmd_t cmd, void *dev /*this is a dev_t?*/, void **resultp) {
    printf("%s: called\n", __func__);
    int err = DDI_SUCCESS;

    unsigned int minor = getminor((dev_t)dev);

    // TODO: [jubal] get rid of this!!
    struct vioscsi_softc *sc = global_virtio_scsi_softc;

    // I think this takes care of the above TODO..
    //struct vioscsi_softc *sc;
    //if ((sc = ddi_get_driver_private(devinfo)) == NULL) {
    //    return DDI_FAILURE;
    //}


    // what ddi command was sent?
    switch (cmd) {
        case DDI_INFO_DEVT2DEVINFO: {
            /*FIXME: only one instance is supported */
            *resultp = sc->sc_dev; // TODO: can this come from the *devinfo?
            break;
        }
        case DDI_INFO_DEVT2INSTANCE: {
            *resultp = (void *)(intptr_t)(MINOR2INST(minor));
            break;
        }
        default: {
            err = DDI_FAILURE;
            *resultp = NULL;
        }
    }

    return err;
}





static int vioscsi_attach(dev_info_t *devinfo, ddi_attach_cmd_t cmd) {
    printf("%s: called\n", __func__);
    int err = DDI_SUCCESS;

    int instance;
    struct vioscsi_softc    *sc;
    struct virtio_softc     *vsc;
    scsi_hba_tran_t         *hba_tran;

    // filter the command out..  we only want to handle attach for now.
    printf("virtio_scsi_attach %s: kmem_cached SCSA version\n", __func__);
    switch (cmd) {
        case DDI_ATTACH:
            printf("%s: received DDI_ATTACH cmd 0x%x\n", __func__, cmd);
            break;

        case DDI_RESUME:
        case DDI_PM_RESUME:
            printf("%s: resume not supported yet\n", __func__);
            return DDI_FAILURE;

        default:
            printf("%s: cmd 0x%x not recognized\n", __func__, cmd);
            return DDI_FAILURE;
    }

    // this leaves instance null...
    //if ((instance = ddi_get_instance(devinfo)) == NULL) {
    //    printf("%s: ddi_get_instance() returned NULL\n", __func__);
    //    return DDI_FAILURE;
    //}
    // so we use this global that we need to kill still.?
    instance = ddi_get_instance(devinfo);
    printf("%s: got the ddi instance\n", __func__);



    /* TODO: rework static allocation. */
    sc = kmem_zalloc(sizeof(struct vioscsi_softc), KM_SLEEP);
    global_virtio_scsi_softc = sc; // back to the global...  ddi_set_driver_private i don't understand.

    vsc = &sc->sc_virtio;

    /* Duplicate for faster access / less typing */
    sc->sc_dev = devinfo;
    vsc->sc_dev = devinfo;


    // from vioblk.c
    // do we need mutexes?
    // cv_init(&sc->cv_devid, NULL, CV_DRIVER, NULL);
    // mutex_init(&sc->lock_devid, NULL, MUTEX_DRIVER, NULL);

    // TODO: kstats see vioblk.c line 852.

    /* map BAR0 */
    err = ddi_regs_map_setup(
                devinfo,
                1,
                (caddr_t *)&sc->sc_virtio.sc_io_addr,
                0,
                0,
                &vioscsi_acc_attr,
                &sc->sc_virtio.sc_ioh);

    printf("%s: mapped BAR0\n", __func__);

    // did ddi_regs_map_setup work?
    if (err != DDI_SUCCESS) {
        printf("%s: failure in ddi_regs_map_setup(), jumping to exit_sc\n", __func__);
    }

    virtio_device_reset(&sc->sc_virtio);
    virtio_set_status(&sc->sc_virtio, VIRTIO_CONFIG_DEVICE_STATUS_ACK);
    virtio_set_status(&sc->sc_virtio, VIRTIO_CONFIG_DEVICE_STATUS_DRIVER);
    printf("%s: did a device reset, and called set_status\n", __func__);


    // ok, we're blowing an ASSERT(sc->config_offset) in these virtio_read_device_config func's
    // likely due to MSI/MSIX..  we don't go msix until we virtio_enable_ints (below)
    // so, we should set sc->config_offset to VIRTIO_CONFIG_DEVICE_NOMSIX?
    //sc->config_offset = VIRTIO_CONFIG_DEVICE_CONFIG_NOMSIX);
    // or vsc->sc_config_offset = VIRTIO_CONFIG_DEVICE_CONFIG_NOMSIX
    vsc->sc_config_offset = VIRTIO_CONFIG_DEVICE_CONFIG_NOMSIX;

    // TODO: get device features and stuff.
    //sc->sc_max_lun      = virtio_read_device_config_4(&sc->sc_virtio, VIRTIO_SCSI_CONFIG_MAX_LUN);
    sc->sc_max_lun          = virtio_read_device_config_4(vsc, VIRTIO_SCSI_CONFIG_MAX_LUN);
    printf("%s: sc_max_lun == %d\n", __func__, sc->sc_max_lun);

    //sc->sc_max_channel  = virtio_read_device_config_4(&sc->sc_virtio, VIRTIO_SCSI_CONFIG_MAX_CHANNEL);
    sc->sc_max_channel  = virtio_read_device_config_4(vsc, VIRTIO_SCSI_CONFIG_MAX_CHANNEL);
    printf("%s: sc_max_channel == %d\n", __func__, sc->sc_max_channel);

    //sc->sc_max_req      = (sc->sc_max_lun * virtio_read_device_config_4(&sc->sc_virtio, VIRTIO_SCSI_CONFIG_CMD_PER_LUN));
    sc->sc_max_req      = (sc->sc_max_lun * virtio_read_device_config_4(vsc, VIRTIO_SCSI_CONFIG_CMD_PER_LUN));
    printf("%s: sc_max_req == %d\n", __func__, sc->sc_max_req);

    //sc->sc_cdb_size     = virtio_read_device_config_4(&sc->sc_virtio, VIRTIO_SCSI_CONFIG_CDB_SIZE);
    sc->sc_cdb_size     = virtio_read_device_config_4(vsc, VIRTIO_SCSI_CONFIG_CDB_SIZE);
    printf("%s: sc_cdb_size == %d\n", __func__, sc->sc_cdb_size);

    //sc->sc_max_seg      = virtio_read_device_config_4(&sc->sc_virtio, VIRTIO_SCSI_CONFIG_SEG_MAX);
    sc->sc_max_seg      = virtio_read_device_config_4(vsc, VIRTIO_SCSI_CONFIG_SEG_MAX);
    printf("%s: sc_max_seg == %d\n", __func__, sc->sc_max_seg);

    //sc->sc_max_target   = virtio_read_device_config_2(&sc->sc_virtio, VIRTIO_SCSI_CONFIG_MAX_TARGET);
    sc->sc_max_target   = virtio_read_device_config_2(vsc, VIRTIO_SCSI_CONFIG_MAX_TARGET);
    printf("%s: sc_max_target == %d\n", __func__, sc->sc_max_target);

    // register interrupts.
    if (vioscsi_register_ints(sc)) {
        printf("%s: failure in vioscsi_register_ints(sc), jumping to enable_intrs_fail\n", __func__);
        goto enable_intrs_fail;
    }

    /* allocate queues (128 indirect descriptors seems to be enough */
    sc->sc_control_vq = virtio_alloc_vq(&sc->sc_virtio, 0, 0, 128, "Virtio SCSI control queue");
    if (sc->sc_control_vq == NULL) {
        printf("%s: virtio_alloc_vq() failed, couldn't setup sc->sc_control_vq\n", __func__);
        goto enable_intrs_fail;
    }

    sc->sc_event_vq = virtio_alloc_vq(&sc->sc_virtio, 1, 0, 128, "Virtio SCSI event queue");
    if (sc->sc_event_vq == NULL) {
        printf("%s: virtio_alloc_vq() failed, couldn't setup sc->sc_event_vq\n", __func__);
        goto release_control;
    }

    sc->sc_request_vq = virtio_alloc_vq(&sc->sc_virtio, 2, 0, 128, "Virtio SCSI request queue");
    if (sc->sc_request_vq == NULL) {
        printf("%s: virtial_alloc_vq() failed, couldn't setup sc->sc_request_vq\n", __func__);
        goto release_event;
    }

    printf("%s: setting up the hba transport callbacks!\n", __func__);

    // setup the hba transport callbacks!
    hba_tran                        = scsi_hba_tran_alloc(devinfo, SCSI_HBA_CANSLEEP);
    sc->sc_hba_tran                 = hba_tran;
    hba_tran->tran_hba_len          = sizeof(struct vioscsi_request);
    hba_tran->tran_hba_private      = sc;
    hba_tran->tran_tgt_private      = NULL;
    hba_tran->tran_tgt_init         = vioscsi_tran_tgt_init;
    hba_tran->tran_tgt_probe        = vioscsi_tran_tgt_probe;
    hba_tran->tran_tgt_free         = vioscsi_tran_tgt_free;

    hba_tran->tran_start            = vioscsi_tran_start;
    hba_tran->tran_abort            = vioscsi_tran_abort;
    hba_tran->tran_reset            = vioscsi_tran_reset;
    hba_tran->tran_getcap           = vioscsi_tran_getcap;
    hba_tran->tran_setcap           = vioscsi_tran_setcap;

    hba_tran->tran_setup_pkt        = vioscsi_tran_setup_pkt;
    hba_tran->tran_teardown_pkt     = vioscsi_tran_teardown_pkt;
    hba_tran->tran_pkt_constructor  = vioscsi_tran_pkt_constructor;
    hba_tran->tran_pkt_destructor   = vioscsi_tran_pkt_destructor;

    hba_tran->tran_dmafree          = vioscsi_tran_dma_free;
    hba_tran->tran_sync_pkt         = vioscsi_tran_sync_pkt;
    hba_tran->tran_reset_notify     = vioscsi_tran_reset_notify;
    hba_tran->tran_quiesce          = vioscsi_tran_bus_quiesce;
    hba_tran->tran_unquiesce        = vioscsi_tran_bus_unquiesce;
    hba_tran->tran_bus_reset        = vioscsi_tran_bus_reset;
    hba_tran->tran_bus_config       = vioscsi_tran_bus_config;

    printf("%s: RIGHT BEFORE scsi_hba_attach_setup!\n", __func__);

    err = scsi_hba_attach_setup(devinfo, &vioscsi_data_dma_attr, hba_tran, SCSI_HBA_TRAN_CLONE | SCSI_HBA_TRAN_CDB | SCSI_HBA_TRAN_SCB);
    if (err != DDI_SUCCESS) {
        printf("%s: scsi_hba_attach_setup failed, going to release_request\n", __func__);
        goto release_request;
    }

    if (ddi_create_minor_node(devinfo, "devctl", S_IFCHR, INST2DEVCTL(instance), DDI_NT_SCSI_NEXUS, 0) != DDI_SUCCESS) {
        printf("%s: ddi_create_minor_node failed, going to detach_hba\n", __func__);
        goto detach_hba;
    }

    /* FIXME: have to destroy devctl node */
    if (ddi_create_minor_node(devinfo, "scsi", S_IFCHR, INST2DEVCTL(instance), DDI_NT_SCSI_ATTACHMENT_POINT, 0) != DDI_SUCCESS) {
        (void) ddi_remove_minor_node(devinfo, "devctl");
        printf("%s: ddi_create_minor_node failed, going to detach_hba\n", __func__);
        goto detach_hba;
    }
    ddi_report_dev(devinfo);

    (void) virtio_enable_ints(&sc->sc_virtio);
    printf("%s: returning DDI_SUCCESS\n", __func__);
    return (DDI_SUCCESS);



detach_hba:
    printf("%s: at detach_hba\n", __func__);
    (void) scsi_hba_detach(devinfo);

release_request:
    printf("%s: at release_request\n", __func__);
    virtio_free_vq(sc->sc_request_vq);

release_event:
    printf("%s: at release_event\n", __func__);
    virtio_free_vq(sc->sc_event_vq);

release_control:
    printf("%s: at release_control\n", __func__);
    virtio_free_vq(sc->sc_control_vq);

enable_intrs_fail:
    printf("%s: at enable_intrs_fail\n", __func__);
    ddi_regs_map_free(&sc->sc_virtio.sc_ioh);

exit_sc:
    printf("%s: at exit_sc (returning DDI_FAIL)\n", __func__);
    kmem_free(sc, sizeof(* sc));
    return (DDI_FAILURE);
}

static int vioscsi_detach(dev_info_t *devinfo, ddi_detach_cmd_t cmd) {
    printf("%s: called\n", __func__);
    struct vioscsi_softc *sc;

    if ((sc = ddi_get_driver_private(devinfo)) == NULL) {
        printf("%s: ddi_get_driver_private() failed\n", __func__);
        return (DDI_FAILURE);
    }

    switch (cmd) {
    case DDI_DETACH:
        break;

    case DDI_PM_SUSPEND:
        cmn_err(CE_WARN, "suspend not supported yet");
        printf("%s: returning DDI_FAILURE (suspend not supported yet)\n", __func__);
        return (DDI_FAILURE);

    default:
        cmn_err(CE_WARN, "cmd 0x%x unrecognized", cmd);
        printf("%s: returning (cmd 0x%x unrecognized\n", __func__, cmd);
        return (DDI_FAILURE);
    }

    virtio_stop_vq_intr(sc->sc_request_vq);

    virtio_release_ints(&sc->sc_virtio);

    /* SCSA will take care about kmem cache destruction */
    if (scsi_hba_detach(devinfo) != DDI_SUCCESS) {
        printf("%s: returning DDI_FAILURE (scsi_hba_detach failed)\n", __func__);
        return (DDI_FAILURE);
    }
    virtio_free_vq(sc->sc_request_vq);
    virtio_free_vq(sc->sc_event_vq);
    virtio_free_vq(sc->sc_control_vq);

    (void) ddi_remove_minor_node(devinfo, "scsi");
    (void) ddi_remove_minor_node(devinfo, "devctl");

    kmem_free(sc, sizeof(* sc));
    printf("%s: returning DDI_SUCCESS\n", __func__);
    return (DDI_SUCCESS);
}

static int vioscsi_quiesce(dev_info_t *devinfo) {
//    /* I dunno  look at freebsd http://src.illumos.org/source/xref/freebsd-head/sys/dev/virtio/scsi/virtio_scsi.c */
//    /*struct vioscsi_softc *sc = ddi_get_driver_private(devinfo);
//    virtio_stop_vq_intr(sc->sc_control_vq);
//    virtio_stop_vq_intr(sc->sc_event_vq);
//    virtio_stop_vq_intr(sc->sc_request_vq);
//    virtio_device_reset(&sc->sc_virtio);*/
    printf("%s: called\n", __func__);
    return DDI_SUCCESS;
}
/**
  * END vioscsi_dev_ops callbacks (from _init(), _fini(), _info() )
  * ----------------------------------------------------------------------------------------
  */
























/**
 * ----------------------------------------------------------------------------------------
 * BEGIN Standard illumos loadable driver module functions.
 */
static char vioscsi_ident[] = "VirtIO SCSI HBA driver";

static struct dev_ops vioscsi_dev_ops = {
    DEVO_REV,                               /* */
    0,                                      /* refcount */
    vioscsi_getinfo,                        /* getinfo */
    nulldev,                                /* identify */
    nulldev,                                /* probe */
    vioscsi_attach,                         /* attach */
    vioscsi_detach,                         /* detach */
    nodev,                                  /* reset */
    NULL,                                   /* cb_ops */
    NULL,                                   /* bus_ops */
    NULL,                                   /* power */
    vioscsi_quiesce                         /* quiesce */
};

/* standard module linkage initialization for a streams driver */
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
    &mod_driverops,                    /* type of module this one is a driver */
    vioscsi_ident,                     /* short description */
    &vioscsi_dev_ops                   /* driver specific ops */
};

static struct modlinkage modlinkage = {
  MODREV_1,
  {
      (void *)&modldrv,
      NULL,
  },
};


int _init(void) {
    printf("%s: called\n", __func__);
    int err = 0;

    if ((err = scsi_hba_init(&modlinkage)) == 0) {
        printf("%s: scsi_hba_init worked\n", __func__);
        if ((err = mod_install(&modlinkage)) != 0) {
            printf("%s: mod_install did not work, calling scsi_hba_fini\n", __func__);
            scsi_hba_fini(&modlinkage);
        } else {
            printf("%s: mod_install worked\n", __func__);
        }
    } else {
        printf("%s: scsi_hba_init_failed\n", __func__);
    }

    return err;
}

int _fini(void) {
    printf("%s: called\n", __func__);
    int err = 0;
    if ((err = mod_remove(&modlinkage)) == 0) {
        scsi_hba_fini(&modlinkage);
    } else {
        printf("%s: mod_remove failed\n", __func__);
    }
    return err;
}

int _info(struct modinfo *modinfop) {
    printf("%s: called\n", __func__);
    return mod_info(&modlinkage, modinfop);
}

/**
 * END Standard illumos loadable driver module functions.
 * ----------------------------------------------------------------------------------------
 */
