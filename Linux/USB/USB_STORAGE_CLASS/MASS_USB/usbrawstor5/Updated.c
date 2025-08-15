/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi.h>

#define DRV_NAME "usb_mass_storage_simple"
#define CBW_SIGNATURE    0x43425355
#define CSW_SIGNATURE    0x53425355

struct bulk_cb_wrap {
    __le32 Signature;
    __le32 Tag;
    __le32 DataTransferLength;
    u8 Flags;
    u8 LUN;
    u8 Length;
    u8 CDB[16];
} __packed;

struct bulk_cs_wrap {
    __le32 Signature;
    __le32 Tag;
    __le32 Residue;
    u8 Status;
} __packed;

struct usb_mass_dev {
    struct usb_device    *udev;
    struct delayed_work   scan_work;
    struct usb_interface *interface;
    struct Scsi_Host     *host;
    u8                   ep_in;
    u8                   ep_out;
};

static atomic_t next_tag = ATOMIC_INIT(1);
static int usb_mass_request_sense(struct usb_mass_dev *dev, u8 lun);
static void my_usbstor_scan(struct work_struct *work);
static int send_mass_command(struct usb_mass_dev *dev, struct scsi_cmnd *scmd);

static const char *get_opcode_name(u8 opcode)
{
    switch (opcode) {
    case 0x00: return "TEST_UNIT_READY";
    case 0x03: return "REQUEST_SENSE";
    case 0x08: return "READ_6";
    case 0x12: return "INQUIRY";
    case 0x25: return "READ_CAPACITY";
    case 0x28: return "READ10";
    case 0x2A: return "WRITE10";
    case 0x3C: return "READ_BUFFER";
    case 0x9E: return "READ_CAPACITY_16";
    default:   return "UNKNOWN";
    }
}

static void usb_mass_clear_bulk_halt(struct usb_mass_dev *dev, u8 ep, bool is_out)
{
    int ret;
    pr_info("%s: Clearing halt on ep 0x%02x\n", DRV_NAME, ep);
    ret = usb_clear_halt(dev->udev,
                is_out ? usb_sndbulkpipe(dev->udev, ep)
                       : usb_rcvbulkpipe(dev->udev, ep));
    pr_info("%s: usb_clear_halt returned %d\n", DRV_NAME, ret);
}

static void my_usbstor_scan(struct work_struct *work)
{
    struct usb_mass_dev *dev =
        container_of(work, struct usb_mass_dev, scan_work.work);

    pr_info("Delayed scan triggered\n");
    scsi_scan_host(dev->host);
    pr_info("usb_mass_request_sense returned %d\n",
            usb_mass_request_sense(dev, 0));
}

static int usb_mass_request_sense(struct usb_mass_dev *dev, u8 lun)
{
    struct bulk_cb_wrap *cbw;
    struct bulk_cs_wrap *csw;
    u8 *sense_buf;
    int ret, actual_len, retries;

    cbw = kmalloc(sizeof(*cbw), GFP_KERNEL);
    csw = kmalloc(sizeof(*csw), GFP_KERNEL);
    sense_buf = kmalloc(18, GFP_KERNEL);
    if (!cbw || !csw || !sense_buf) {
        ret = -ENOMEM;
        goto free_rs;
    }

    memset(cbw, 0, sizeof(*cbw));
    cbw->Signature = cpu_to_le32(CBW_SIGNATURE);
    cbw->Tag       = cpu_to_le32(atomic_inc_return(&next_tag));
    cbw->DataTransferLength = cpu_to_le32(18);
    cbw->Flags     = 0x80;
    cbw->LUN       = lun;
    cbw->Length    = 6;
    cbw->CDB[0]    = 0x03;
    cbw->CDB[4]    = 18;

    /* CBW */
    retries = 3;
    do {
        ret = usb_bulk_msg(dev->udev,
            usb_sndbulkpipe(dev->udev, dev->ep_out),
            cbw, sizeof(*cbw), &actual_len, 5000);
        if (ret == -EPIPE)
            usb_mass_clear_bulk_halt(dev, dev->ep_out, true);
    } while ((ret == -EPIPE || ret == -EAGAIN) && --retries);
    if (ret) goto free_rs;

    /* Data */
    retries = 3;
    do {
        ret = usb_bulk_msg(dev->udev,
            usb_rcvbulkpipe(dev->udev, dev->ep_in),
            sense_buf, 18, &actual_len, 5000);
        if (ret == -EPIPE)
            usb_mass_clear_bulk_halt(dev, dev->ep_in, false);
    } while ((ret == -EPIPE || ret == -EAGAIN) && --retries);
    if (ret) goto free_rs;

    print_hex_dump_debug("Sense Data:", DUMP_PREFIX_OFFSET, 16, 1,
                         sense_buf, actual_len, true);

    /* CSW */
    retries = 3;
    do {
        ret = usb_bulk_msg(dev->udev,
            usb_rcvbulkpipe(dev->udev, dev->ep_in),
            csw, sizeof(*csw), &actual_len, 5000);
        if (ret == -EPIPE)
            usb_mass_clear_bulk_halt(dev, dev->ep_in, false);
    } while ((ret == -EPIPE || ret == -EAGAIN) && --retries);

    if (!ret && le32_to_cpu(csw->Signature) != CSW_SIGNATURE)
        ret = -EIO;

    pr_info("%s: RS status=%d residue=%u\n", DRV_NAME,
            ret, (unsigned)le32_to_cpu(csw->Residue));

free_rs:
    kfree(cbw);
    kfree(csw);
    kfree(sense_buf);
    return ret;
}

static int send_mass_command(struct usb_mass_dev *dev, struct scsi_cmnd *scmd)
{
    struct bulk_cb_wrap *cbw;
    struct bulk_cs_wrap *csw;
    void *data_buf = NULL;
    int ret, actual_len, retries;
    bool skip_data = false;
    u32 expected = scsi_bufflen(scmd);
    u32 tag;

    cbw = kmalloc(sizeof(*cbw), GFP_KERNEL);
    csw = kmalloc(sizeof(*csw), GFP_KERNEL);
    if (!cbw || !csw) { ret = -ENOMEM; goto out; }

    memset(cbw, 0, sizeof(*cbw));
    cbw->Signature = cpu_to_le32(CBW_SIGNATURE);
    tag = atomic_inc_return(&next_tag);
    cbw->Tag = cpu_to_le32(tag);
    cbw->DataTransferLength = cpu_to_le32(expected);
    cbw->LUN = scmd->device->lun;
    cbw->Length = scmd->cmd_len;
    memcpy(cbw->CDB, scmd->cmnd, scmd->cmd_len);
    cbw->Flags = (scmd->sc_data_direction == DMA_FROM_DEVICE) ? 0x80 : 0x00;

    pr_info("%s: CMD %s(0x%02x) Tag=%u exp=%u\n", DRV_NAME,
            get_opcode_name(scmd->cmnd[0]), scmd->cmnd[0], tag, expected);

    /* CBW */
    retries = 5;
    do {
        ret = usb_bulk_msg(dev->udev,
            usb_sndbulkpipe(dev->udev, dev->ep_out),
            cbw, sizeof(*cbw), &actual_len, 5000);
        if (ret == -EPIPE) {
            usb_mass_clear_bulk_halt(dev, dev->ep_out, true);
            if (scmd->cmnd[0] == 0x00) /* TEST_UNIT_READY */
                usb_mass_request_sense(dev, scmd->device->lun);
        }
    } while ((ret == -EPIPE || ret == -EAGAIN) && --retries);
    if (ret) goto cleanup;

    /* Data stage */
    if (expected) {
        data_buf = kmalloc(expected, GFP_KERNEL);
        if (!data_buf) { ret = -ENOMEM; goto cleanup; }
        if (scmd->sc_data_direction == DMA_TO_DEVICE)
            sg_copy_to_buffer(scsi_sglist(scmd), scsi_sg_count(scmd), data_buf, expected);
        retries = 5;
        do {
            ret = usb_bulk_msg(dev->udev,
                (scmd->sc_data_direction == DMA_FROM_DEVICE) ?
                  usb_rcvbulkpipe(dev->udev, dev->ep_in) :
                  usb_sndbulkpipe(dev->udev, dev->ep_out),
                data_buf, expected, &actual_len, 10000);
            if (ret == -EPIPE) {
                usb_mass_clear_bulk_halt(dev,
                    (scmd->sc_data_direction == DMA_FROM_DEVICE) ? dev->ep_in : dev->ep_out,
                    scmd->sc_data_direction == DMA_FROM_DEVICE ? false : true);
                break;
            }
        } while ((ret == -EPIPE || ret == -EAGAIN) && --retries);
        if (!ret && scmd->sc_data_direction == DMA_FROM_DEVICE)
            sg_copy_from_buffer(scsi_sglist(scmd), scsi_sg_count(scmd), data_buf, actual_len);
    }

    /* CSW */
    retries = 5;
    do {
        ret = usb_bulk_msg(dev->udev,
            usb_rcvbulkpipe(dev->udev, dev->ep_in),
            csw, sizeof(*csw), &actual_len, 5000);
        if (ret == -EPIPE)
            usb_mass_clear_bulk_halt(dev, dev->ep_in, false);
    } while ((ret == -EPIPE || ret == -EAGAIN) && --retries);
    if (!ret) {
        if (le32_to_cpu(csw->Signature) != CSW_SIGNATURE) ret = -EIO;
        else if (le32_to_cpu(csw->Tag) != tag) ret = -EIO;
        else if (csw->Status) ret = -EIO;
    }

cleanup:
    kfree(data_buf);
out:
    pr_info("%s: send_mass_command() returning %d\n", DRV_NAME, ret);
    kfree(cbw);
    kfree(csw);
    return ret;
}

static int mass_queuecommand(struct Scsi_Host *shost, struct scsi_cmnd *scmd)
{
    struct usb_mass_dev *dev = shost_priv(shost);
    int ret;

    pr_info("%s: Queueing %s(0x%02x)\n", DRV_NAME,
            get_opcode_name(scmd->cmnd[0]), scmd->cmnd[0]);
    ret = send_mass_command(dev, scmd);
    if (scmd->cmnd[0] == 0x00 && ret == -EPIPE) {
        /* TEST_UNIT_READY stall: requeue */
        scmd->result = (DID_REQUEUE << 16);
    } else {
        scmd->result = ret ? (DID_ERROR << 16) : (DID_OK << 16);
    }
    pr_info("%s: Command %s result %#x\n", DRV_NAME,
            get_opcode_name(scmd->cmnd[0]), scmd->result);
    scsi_done(scmd);
    return 0;
}

static struct scsi_host_template mass_sht = {
    .module        = THIS_MODULE,
    .name          = DRV_NAME,
    .queuecommand  = mass_queuecommand,
    .can_queue     = 1,
    .this_id       = -1,
    .sg_tablesize  = SG_ALL,
    .cmd_per_lun   = 1,
    .max_lun	   = 1;
    .max_sectors   = 240,
    .skip_settle_delay = 1,
};

static const struct usb_device_id mass_table[] = {
    { USB_DEVICE(0x05e3, 0x0751) }, {}
};
MODULE_DEVICE_TABLE(usb, mass_table);

static int usb_mass_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct usb_mass_dev *dev;
    struct Scsi_Host *host;
    int i, ret;
    struct usb_host_interface *alt = intf->cur_altsetting;
    struct usb_endpoint_descriptor *epd;

    pr_info("%s: Probe %04x:%04x\n", DRV_NAME, id->idVendor, id->idProduct);
    host = scsi_host_alloc(&mass_sht, sizeof(*dev));
    if (!host) return -ENOMEM;
    dev = shost_priv(host);

    dev->udev = usb_get_dev(interface_to_usbdev(intf));
    dev->interface = intf;
    dev->host = host;

    for (i = 0; i < alt->desc.bNumEndpoints; i++) {
        epd = &alt->endpoint[i].desc;
        if (usb_endpoint_is_bulk_in(epd))  dev->ep_in  = epd->bEndpointAddress;
        if (usb_endpoint_is_bulk_out(epd)) dev->ep_out = epd->bEndpointAddress;
    }
    if (!dev->ep_in || !dev->ep_out) { ret = -ENODEV; goto err; }

    usb_set_intfdata(intf, dev);
    ret = scsi_add_host(host, &intf->dev);
    if (ret) goto err;

    INIT_DELAYED_WORK(&dev->scan_work, my_usbstor_scan);
    schedule_delayed_work(&dev->scan_work, msecs_to_jiffies(100));
    return 0;
err:
    usb_put_dev(dev->udev);
    scsi_host_put(host);
    return ret;
}

static void usb_mass_disconnect(struct usb_interface *intf)
{
    struct usb_mass_dev *dev = usb_get_intfdata(intf);
    cancel_delayed_work_sync(&dev->scan_work);
    scsi_remove_host(dev->host);
    scsi_host_put(dev->host);
    usb_put_dev(dev->udev);
}

static struct usb_driver usb_mass_driver = {
    .name       = DRV_NAME,
    .id_table   = mass_table,
    .probe      = usb_mass_probe,
    .disconnect = usb_mass_disconnect,
    .supports_autosuspend = 1,
};

module_usb_driver(usb_mass_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vishnu");
MODULE_DESCRIPTION("USB Mass Storage Simple v0.9");
MODULE_VERSION("0.9");

