#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/slab.h>

#define VENDOR_ID  0x0483  
#define PRODUCT_ID 0x5740

#define USB_BUFFER_SIZE 64

struct stm32_joy {
    struct usb_device *udev;
    struct input_dev *input;
    struct urb *urb;
    unsigned char *data;
    dma_addr_t data_dma;
};

static void stm32_joy_read_callback(struct urb *urb)
{
    struct stm32_joy *joy = urb->context;

    if (urb->status == 0) {
        int ax, ay, bx, by, bt1, bt2;

        // Garante terminação
        joy->data[urb->actual_length] = '\0';

        if (sscanf(joy->data, "aX:%d aY:%d bX:%d bY:%d bt1:%d bt2:%d",
                   &ax, &ay, &bx, &by, &bt1, &bt2) == 6) {

            input_report_abs(joy->input, ABS_X, ax);
            input_report_abs(joy->input, ABS_Y, ay);
            input_report_abs(joy->input, ABS_RX, bx);
            input_report_abs(joy->input, ABS_RY, by);
            input_report_key(joy->input, BTN_TRIGGER, bt1);
            input_report_key(joy->input, BTN_THUMB,   bt2);
            input_sync(joy->input);
        }
    }

    usb_submit_urb(urb, GFP_ATOMIC); // Reenvia a URB
}

static int stm32_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct stm32_joy *joy;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int pipe, maxp, error;

    iface_desc = interface->cur_altsetting;
    endpoint = &iface_desc->endpoint[0].desc;

    if (!usb_endpoint_is_bulk_in(endpoint)) {
        printk(KERN_ERR "STM32 joystick: endpoint incompatível\n");
        return -ENODEV;
    }

    pipe = usb_rcvbulkpipe(interface_to_usbdev(interface), endpoint->bEndpointAddress);
    maxp = usb_maxpacket(interface_to_usbdev(interface), pipe);

    joy = kzalloc(sizeof(*joy), GFP_KERNEL);
    if (!joy) return -ENOMEM;

    joy->udev = usb_get_dev(interface_to_usbdev(interface));

    joy->input = input_allocate_device();
    if (!joy->input) {
        usb_put_dev(joy->udev);
        kfree(joy);
        return -ENOMEM;
    }

    joy->input->name = "STM32 USB CDC Joystick";
    joy->input->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
    input_set_abs_params(joy->input, ABS_X, 0, 4095, 0, 0);
    input_set_abs_params(joy->input, ABS_Y, 0, 4095, 0, 0);
    input_set_abs_params(joy->input, ABS_RX, 0, 4095, 0, 0);
    input_set_abs_params(joy->input, ABS_RY, 0, 4095, 0, 0);
    set_bit(BTN_TRIGGER, joy->input->keybit);
    set_bit(BTN_THUMB,   joy->input->keybit);

    input_register_device(joy->input);

    // Aloca buffer e URB
    joy->data = usb_alloc_coherent(joy->udev, USB_BUFFER_SIZE, GFP_KERNEL, &joy->data_dma);
    joy->urb = usb_alloc_urb(0, GFP_KERNEL);
    usb_fill_bulk_urb(joy->urb, joy->udev, pipe, joy->data, USB_BUFFER_SIZE,
                      stm32_joy_read_callback, joy);
    joy->urb->transfer_dma = joy->data_dma;
    joy->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    usb_set_intfdata(interface, joy);

    error = usb_submit_urb(joy->urb, GFP_KERNEL);
    if (error) {
        printk(KERN_ERR "STM32 joystick: erro ao enviar URB\n");
        input_unregister_device(joy->input);
        usb_free_coherent(joy->udev, USB_BUFFER_SIZE, joy->data, joy->data_dma);
        usb_put_dev(joy->udev);
        kfree(joy);
        return error;
    }

    printk(KERN_INFO "STM32 joystick conectado\n");
    return 0;
}

static void stm32_disconnect(struct usb_interface *interface)
{
    struct stm32_joy *joy = usb_get_intfdata(interface);

    usb_kill_urb(joy->urb);
    input_unregister_device(joy->input);
    usb_free_urb(joy->urb);
    usb_free_coherent(joy->udev, USB_BUFFER_SIZE, joy->data, joy->data_dma);
    usb_put_dev(joy->udev);
    kfree(joy);

    printk(KERN_INFO "STM32 joystick desconectado\n");
}

static const struct usb_device_id stm32_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    {} // fim
};
MODULE_DEVICE_TABLE(usb, stm32_table);

static struct usb_driver stm32_driver = {
    .name = "stm32_joystick",
    .id_table = stm32_table,
    .probe = stm32_probe,
    .disconnect = stm32_disconnect,
};

module_usb_driver(stm32_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LuisFerlim");
MODULE_DESCRIPTION("Driver de Joystick com STM32 USB CDC");

