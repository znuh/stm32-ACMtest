/*
 * This file is based on the cdcacm.c file of the libopencm3 project:
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 * 
 * Changes Copyright (C) 2025 Benedikt Heinz <hunz@mailbox.org>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/crs.h>
#include <libopencm3/stm32/st_usbfs.h>

#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>

#include <libopencm3/cm3/nvic.h>

#include <libopencmsis/core_cm3.h>

#include <string.h> // needed for memcmp & memcpy

#include "platform.h"
#include "utils.h"
#include "console.h"

#include <unistd.h>

static const struct usb_device_descriptor dev = {
  .bLength = USB_DT_DEVICE_SIZE,
  .bDescriptorType = USB_DT_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = USB_CLASS_CDC,
  .bDeviceSubClass = 0,
  .bDeviceProtocol = 0,
  .bMaxPacketSize0 = 64,
  .idVendor = 0x0483,
  .idProduct = 0x5740,
  .bcdDevice = 0x0200,
  .iManufacturer = 1,
  .iProduct = 2,
  .iSerialNumber = 3,
  .bNumConfigurations = 1,
};

/*
 * This notification endpoint isn't implemented. According to CDC spec its
 * optional, but its absence causes a NULL pointer dereference in Linux
 * cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x83,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 16,
	.bInterval = 255,
}};

static const struct usb_endpoint_descriptor data_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x82,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength =
			sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = 1,
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = 0,
		.bSubordinateInterface0 = 1,
	 },
};

static const struct usb_interface_descriptor comm_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_CDC,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	.iInterface = 0,

	.endpoint = comm_endp,

	.extra = &cdcacm_functional_descriptors,
	.extralen = sizeof(cdcacm_functional_descriptors),
}};

static const struct usb_interface_descriptor data_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = data_endp,
}};

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = comm_iface,
}, {
	.num_altsetting = 1,
	.altsetting = data_iface,
}};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static const char *usb_strings[] = {
	"znu",
	"ACMconsole",
	"0",
};

void usb_setup(void);

volatile int ACM_active     = 0;
static usbd_device *usb_dev = NULL;

/* Buffer to be used for control requests. */
static uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes cdcacm_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
		uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req)) {
	(void)complete;
	(void)buf;
	(void)usbd_dev;

	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/*
		 * This Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		char local_buf[10];
		struct usb_cdc_notification *notif = (void *)local_buf;

		/* We echo signals back to host as notification. */
		notif->bmRequestType = 0xA1;
		notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
		notif->wValue = 0;
		notif->wIndex = 0;
		notif->wLength = 2;
		local_buf[8] = req->wValue & 3;
		local_buf[9] = 0;
		// usbd_ep_write_packet(0x83, buf, 10);
		return USBD_REQ_HANDLED;
		}
	case USB_CDC_REQ_SET_LINE_CODING:
		if (*len < sizeof(struct usb_cdc_line_coding))
			return USBD_REQ_NOTSUPP;
		return USBD_REQ_HANDLED;
	}
	return USBD_REQ_NOTSUPP;
}

static const char bl_string[] = "ICANHAZBOOTLOADER";

#define  ACM_RXBUF_SZ             128
static   uint32_t ACM_rx_put    = 0;
volatile uint32_t ACM_rx_fill   = 0;
uint8_t  ACM_rxbuf[ACM_RXBUF_SZ];

volatile uint32_t SIGINT        = 0;

/* called by cdcacm_data_rx_cb in USB ISR context */
static inline void rx_put(uint8_t d) {
	ACM_rxbuf[ACM_rx_put++] = d;
	ACM_rx_put &= (ACM_RXBUF_SZ-1);
}

static uint32_t ACM_rx_get=0;

/* called by user from non-ISR context */
static uint32_t ACM_rx_request(void) {
	return MIN(ACM_rx_fill, ACM_RXBUF_SZ-ACM_rx_get);
}

/* called by user from non-ISR context */
static void ACM_rx_free(uint32_t chunk) {
	uint8_t isr_state = nvic_get_irq_enabled(NVIC_USB_IRQ);
	ACM_rx_get+=chunk;
	ACM_rx_get &= (ACM_RXBUF_SZ-1);
	
	nvic_disable_irq(NVIC_USB_IRQ);
	ACM_rx_fill-=chunk;
	if(isr_state)
		nvic_enable_irq(NVIC_USB_IRQ);
}

/* called by user from non-ISR context */
void ACM_to_console(void) {
	uint8_t buf[64];
	const uint32_t chunk = MIN(ACM_rx_request(), sizeof(buf));
	memcpy(buf, ACM_rxbuf+ACM_rx_get, chunk);
	ACM_rx_free(chunk);
	console_process(buf, chunk);
}

/* called by user from non-ISR context */
int ACM_readbyte(void) {
	int res=-1;
	if(!ACM_active)
		goto out;
	SLEEP_UNTIL(SIGINT || ACM_rx_request());
	if(!SIGINT) {
		res=*(ACM_rxbuf+ACM_rx_get);
		ACM_rx_free(1);
	}
out:
	return res;
}

void usb_shutdown(void) {
	nvic_disable_irq(NVIC_USB_IRQ);
	ACM_active = 0;
	usbd_disconnect(usb_dev, true);
	usb_dev = NULL;
}

/* called by USB stack in USB ISR context */
static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep) {
	char buf[64], *d=buf;
	int len;
	if(ep != 0x01)
		return;
	len = usbd_ep_read_packet(usbd_dev, 0x01, buf, 64);
	if(!len)
		return;
	if((len >= (int)(sizeof(bl_string)-1)) && (!memcmp(buf,bl_string,sizeof(bl_string)-1))) {
		usb_shutdown();
		erase_page0(0xAA55);
	}
	len = MIN(len, (int)(ACM_RXBUF_SZ-ACM_rx_fill)); // clamp len to avoid rxbuf overruns
	ACM_rx_fill+=len;
	for(;len;len--,d++) {
		rx_put(*d == '\r' ? '\n' : *d);
		SIGINT += (*d == 0x03);
	}
}

#define ACM_TXBUF_SZ          1024
static volatile uint32_t ACM_tx_fill   = 0;
static uint8_t  ACM_txbuf[ACM_TXBUF_SZ];

static volatile uint32_t ACM_tx_active = 0;

// called from non-ISR context only
void ACM_waitfor_txdone(void) {
	if(ACM_active)
		SLEEP_UNTIL(!ACM_tx_active);
}

static void cdcacm_data_tx_cb(usbd_device *usbd_dev, uint8_t ep) {
	static uint32_t ACM_tx_get    = 0;

	if((!ACM_active) || (!ACM_tx_fill)) {
		ACM_tx_active = 0;
		return;
	}

	ACM_tx_active = 1;
	ep&=0x7f;
	if ((*USB_EP_REG(ep) & USB_EP_TX_STAT) != USB_EP_TX_STAT_VALID) { /* check if busy */
		volatile uint16_t *PM = (volatile void *)USB_GET_EP_TX_BUFF(ep);
		uint32_t i, len = MIN(ACM_tx_fill, 64);
		uint16_t buf = 0;

		/* write 16bit words */
		for(i=0;i<len;i++) {
			buf>>=8;
			buf|=ACM_txbuf[ACM_tx_get++]<<8;
			ACM_tx_get&=(ACM_TXBUF_SZ-1);
			if(i&1)
				*PM++ = buf;
		}

		/* write remaining byte if odd length */
		if(len&1)
			*PM++ = buf>>8;

		USB_SET_EP_TX_COUNT(ep, len);
		USB_SET_EP_TX_STAT(ep, USB_EP_TX_STAT_VALID);

		ACM_tx_fill -= len;
	}
	usbd_dev = usbd_dev; /* mute compiler warning */
}

static inline void tx_put(uint8_t d) {
	static uint32_t ACM_tx_put = 0;
	ACM_txbuf[ACM_tx_put++] = d;
	ACM_tx_put &= (ACM_TXBUF_SZ-1);
}

/* only called from non-ISR context */
static int ACM_tx(const void *p, size_t n, int ascii) {
	uint8_t isr_state = nvic_get_irq_enabled(NVIC_USB_IRQ);
	int res;

	/* drop data if USB not active */
	if(!ACM_active)
		return n;

	if(isr_state)
		nvic_disable_irq(NVIC_USB_IRQ);

	if(ascii) {
		const char *d = p, *orig = p;
		res=0;
		for(;(n) && ((ACM_TXBUF_SZ - ACM_tx_fill) >= 2);n--,d++,ACM_tx_fill++) {
			if(*d == '\n') {
				tx_put((uint8_t)'\r');
				ACM_tx_fill++;
			}
			tx_put((uint8_t)*d);
		}
		res = d - orig;
	}
	else {
		const uint8_t *d = p;
		res = MIN(ACM_TXBUF_SZ - ACM_tx_fill, n);
		for(n=res;n;n--,d++)
			tx_put(*d);
		ACM_tx_fill += res;
	}

	if((ACM_tx_fill) && (!ACM_tx_active))
		cdcacm_data_tx_cb(usb_dev, 0x82); /* send 1st chunk */

	if(isr_state)
		nvic_enable_irq(NVIC_USB_IRQ);
	return res;
}

int _write(int file, char *ptr, int len);

int _write(int file, char *ptr, int len) {
	int drop = (file != STDOUT_FILENO) && (file != STDERR_FILENO);
	return drop ? len : ACM_tx(ptr, len, 1);
}

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue) {
	usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, cdcacm_data_tx_cb);
	usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);
	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				cdcacm_control_request);
	wValue = wValue;
	ACM_active = 1;
}

void usb_isr(void) {
	if(usb_dev)
		usbd_poll(usb_dev);
}

void usb_setup(void) {
/* for PLL USB clock source an external HSE and PLL output of 48MHz is necessary */
#ifdef USBCLK_USE_PLL
	rcc_set_usbclk_source(RCC_PLL);
#else
	/* default to HSI48 w/ CRS unless user sets USBCLK_USE_PLL */
	rcc_set_usbclk_source(RCC_HSI48);
	crs_autotrim_usb_enable();
#endif

	usb_dev = usbd_init(&st_usbfs_v2_usb_driver, &dev, &config, usb_strings,
						sizeof(usb_strings)/sizeof(char *),
						usbd_control_buffer, sizeof(usbd_control_buffer));

	usbd_register_set_config_callback(usb_dev, cdcacm_set_config);

	nvic_set_priority(NVIC_USB_IRQ, 255);  // lowest priority
	nvic_enable_irq(NVIC_USB_IRQ);
}
