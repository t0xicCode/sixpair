/*
 * sixpair.c version 2007-04-18
 * Modified to recognize DS4 controllers by
 * t0xicCode 2015-06-04
 *
 * Compile with: gcc -o sixpair sixpair.c -lusb
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <usb.h>

#define VENDOR 0x054C
#define DS3_PRODUCT 0x0268
#define DS4_PRODUCT 0x05C4

#define USB_DIR_IN 0x80
#define USB_DIR_OUT 0

void fatal(char *msg) {
	perror(msg); exit(1);
}

void show_master(usb_dev_handle *devh, int itfnum, int product_id) {
	int value = product_id == DS4_PRODUCT ? 0x0312 : 0x03f5;
	unsigned char msg[50];
	int res;

	res = usb_control_msg(devh, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
	                      0x01, value, itfnum, (void*)msg, sizeof(msg), 5000);
	if ( res < 0 ) { perror("USB_REQ_GET_CONFIGURATION"); return; }

	printf("Current Bluetooth master: ");
	if (product_id == DS4_PRODUCT) {
		printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
		       msg[15], msg[14], msg[13], msg[12], msg[11], msg[10]);
	}
	else {
		printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
		       msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);
	}
}

void set_master(usb_dev_handle *devh, int itfnum, int mac[6], int product_id) {
	int res = -1;

	printf("Setting master bd_addr to %02x:%02x:%02x:%02x:%02x:%02x\n",
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	if (product_id == DS4_PRODUCT) {
		unsigned char msg[23] = { 0x13, mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], 0x56, 0xE8, 0x81, 0x38, 0x08, 0x06, 0x51, 0x41, 0xC0, 0x7F, 0x12, 0xAA, 0xD9, 0x66, 0x3C, 0xCE };

		res = usb_control_msg(devh, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0x09, 0x0313, itfnum, (char*)msg, sizeof(msg), 5000);
	}
	else {
		unsigned char msg[8] = { 0x01, 0x00, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] };

		res = usb_control_msg(devh, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0x09, 0x03f5, itfnum, (char*)msg, sizeof(msg), 5000);
	}

	if ( res < 0 ) fatal("USB_REQ_SET_CONFIGURATION");
}

void process_device(int argc, char **argv, struct usb_device *dev,
                    struct usb_config_descriptor *cfg, int itfnum) {
	int mac[6];

	usb_dev_handle *devh = usb_open(dev);
	if ( !devh ) fatal("usb_open");

	usb_detach_kernel_driver_np(devh, itfnum);

	int res = usb_claim_interface(devh, itfnum);
	if ( res < 0 ) fatal("usb_claim_interface");

	show_master(devh, itfnum, dev->descriptor.idProduct);

	if ( argc < 2 || sscanf(argv[1], "%x:%x:%x:%x:%x:%x",
	                        &mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]) != 6 ) {

		printf("usage: %s [<bd_addr of master>]\n", argv[0]);
		exit(1);
	}

	set_master(devh, itfnum, mac, dev->descriptor.idProduct);

	usb_close(devh);
}

int main(int argc, char *argv[]) {

	usb_init();
	if ( usb_find_busses() < 0 ) fatal("usb_find_busses");
	if ( usb_find_devices() < 0 ) fatal("usb_find_devices");
	struct usb_bus *busses = usb_get_busses();
	if ( !busses ) fatal("usb_get_busses");

	int found = 0;

	struct usb_bus *bus;
	for ( bus=busses; bus; bus=bus->next ) {
		struct usb_device *dev;
		for ( dev=bus->devices; dev; dev=dev->next) {
			struct usb_config_descriptor *cfg;
			for ( cfg = dev->config;
			      cfg < dev->config + dev->descriptor.bNumConfigurations;
			      ++cfg ) {

				if (cfg == NULL)
					continue;

				int itfnum;
				for ( itfnum=0; itfnum<cfg->bNumInterfaces; ++itfnum ) {
					struct usb_interface *itf = &cfg->interface[itfnum];
					struct usb_interface_descriptor *alt;
					for ( alt = itf->altsetting;
					      alt < itf->altsetting + itf->num_altsetting;
					      ++alt ) {
						if ( dev->descriptor.idVendor == VENDOR &&
						     (dev->descriptor.idProduct == DS3_PRODUCT || dev->descriptor.idProduct == DS4_PRODUCT) &&
						     alt->bInterfaceClass == 3 ) {
							process_device(argc, argv, dev, cfg, itfnum);
							++found;
						}
					}
				}
			}
		}
	}

	if ( !found ) printf("No controller found on USB busses.\n");
	return 0;

}

