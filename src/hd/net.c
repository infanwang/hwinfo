#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hd.h"
#include "hd_int.h"
#include "net.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gather network interface info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


/*
 * This is independent of the other scans.
 */

void hd_scan_net(hd_data_t *hd_data)
{
  unsigned u;
  hd_t *hd, *hd2;
#if 0
#if defined(__s390__) || defined(__s390x__)
  hd_t *hd0;
#endif
#endif
  char *s, *hw_addr;
  hd_res_t *res, *res1;

  struct sysfs_class *sf_class;
  struct sysfs_class_device *sf_cdev;
  struct sysfs_device *sf_dev;
  struct sysfs_driver *sf_drv;
  struct dlist *sf_cdev_list;

  if(!hd_probe_feature(hd_data, pr_net)) return;

  hd_data->module = mod_net;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->net = free_str_list(hd_data->net);

  PROGRESS(1, 0, "get network data");

  sf_class = sysfs_open_class("net");

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: net\n");
    return;
  }

  sf_cdev_list = sysfs_get_class_devices(sf_class);
  if(sf_cdev_list) dlist_for_each_data(sf_cdev_list, sf_cdev, struct sysfs_class_device) {
    ADD2LOG(
      "  net interface: name = %s, classname = %s, path = %s\n",
      sf_cdev->name,
      sf_cdev->classname,
      hd_sysfs_id(sf_cdev->path)
    );

    hw_addr = NULL;
    if((s = hd_attr_str(sysfs_get_classdev_attr(sf_cdev, "address")))) {
      hw_addr = canon_str(s, strlen(s));
      ADD2LOG("    hw_addr = %s\n", hw_addr);
    }

    sf_dev = sysfs_get_classdev_device(sf_cdev);
    if(sf_dev) {
      ADD2LOG("    net device: path = %s\n", hd_sysfs_id(sf_dev->path));
    }

    sf_drv = sysfs_get_classdev_driver(sf_cdev);
    if(sf_drv) {
      ADD2LOG(
        "    net driver: name = %s, path = %s\n",
        sf_drv->name,
        hd_sysfs_id(sf_drv->path)
      );
    }

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_network_interface;

    res1 = NULL;
    if(hw_addr && sf_dev) {
      res1 = new_mem(sizeof *res1);
      res1->hwaddr.type = res_hwaddr;
      res1->hwaddr.addr = new_str(hw_addr);
      add_res_entry(&hd->res, res1);
    }

    hw_addr = free_mem(hw_addr);

    hd->unix_dev_name = new_str(sf_cdev->name);
    hd->sysfs_id = new_str(hd_sysfs_id(sf_cdev->path));

    if(sf_drv) add_str_list(&hd->drivers, sf_drv->name);

    if(sf_dev) {
      hd2 = hd_find_sysfs_id(hd_data, hd_sysfs_id(sf_dev->path));
      if(hd2) {
        hd->attached_to = hd2->idx;
        /* add hw addr to network card */
        if(res1) {
          u = 0;
          for(res = hd2->res; res; res = res->next) {
            if(
              res->any.type == res_hwaddr &&
              !strcmp(res->hwaddr.addr, res1->hwaddr.addr)
            ) {
              u = 1;
              break;
            }
          }
          if(!u) {
            res = new_mem(sizeof *res);
            res->hwaddr.type = res_hwaddr;
            res->hwaddr.addr = new_str(res1->hwaddr.addr);
            add_res_entry(&hd2->res, res);
          }
        }
      }
    }

    if(!strcmp(hd->unix_dev_name, "lo")) {
      hd->sub_class.id = sc_nif_loopback;
    }
    else if(sscanf(hd->unix_dev_name, "eth%u", &u) == 1) {
      hd->sub_class.id = sc_nif_ethernet;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "tr%u", &u) == 1) {
      hd->sub_class.id = sc_nif_tokenring;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "fddi%u", &u) == 1) {
      hd->sub_class.id = sc_nif_fddi;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "ctc%u", &u) == 1) {
      hd->sub_class.id = sc_nif_ctc;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "iucv%u", &u) == 1) {
      hd->sub_class.id = sc_nif_iucv;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "hsi%u", &u) == 1) {
      hd->sub_class.id = sc_nif_hsi;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "qeth%u", &u) == 1) {
      hd->sub_class.id = sc_nif_qeth;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "escon%u", &u) == 1) {
      hd->sub_class.id = sc_nif_escon;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "myri%u", &u) == 1) {
      hd->sub_class.id = sc_nif_myrinet;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "sit%u", &u) == 1) {
      hd->sub_class.id = sc_nif_sit;	/* ipv6 over ipv4 tunnel */
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "wlan%u", &u) == 1) {
      hd->sub_class.id = sc_nif_wlan;
      hd->slot = u;
    }
    /* ##### add more interface names here */
    else {
      hd->sub_class.id = sc_nif_other;
    }

    hd->bus.id = bus_none;
  }

  sysfs_close_class(sf_class);


#if 0

#if defined(__s390__) || defined(__s390x__)
      if(
        hd->sub_class.id != sc_nif_loopback &&
        hd->sub_class.id != sc_nif_sit && hd->sub_class.id != sc_nif_ethernet && hd->sub_class.id != sc_nif_qeth &&
	hd->sub_class.id != sc_nif_ctc
      ) {
        hd0 = hd;
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_network;
        hd->unix_dev_name = new_str(hd0->unix_dev_name);
        hd->slot = hd0->slot;
        hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6001);	// IBM
        switch(hd0->sub_class.id) {
          case sc_nif_tokenring:
            hd->sub_class.id = 1;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
            str_printf(&hd->device.name, 0, "Token ring card %d", hd->slot);
            break;
          case sc_nif_ctc:
            hd->sub_class.id = 0x04;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0004);
            str_printf(&hd->device.name, 0, "CTC %d", hd->slot);
            break;
          case sc_nif_iucv:
            hd->sub_class.id = 0x05;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0005);
            str_printf(&hd->device.name, 0, "IUCV %d", hd->slot);
            break;
          case sc_nif_hsi:
            hd->sub_class.id = 0x06;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0006);
            str_printf(&hd->device.name, 0, "HSI %d", hd->slot);
            break;
          case sc_nif_escon:
            hd->sub_class.id = 0x08;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0008);
            str_printf(&hd->device.name, 0, "ESCON %d", hd->slot);
            break;
          default:
            hd->sub_class.id = 0x80;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0080);
        }
      }
#endif

#endif


}

