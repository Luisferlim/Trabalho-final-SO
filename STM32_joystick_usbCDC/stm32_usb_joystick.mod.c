#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x67a3e976, "usb_register_driver" },
	{ 0x5c77b92, "usb_kill_urb" },
	{ 0x7bf9cbb2, "input_unregister_device" },
	{ 0x3ef3ca48, "usb_free_urb" },
	{ 0x8803d267, "usb_free_coherent" },
	{ 0xe03885f3, "usb_put_dev" },
	{ 0x37a0cba, "kfree" },
	{ 0x92997ed8, "_printk" },
	{ 0x3845039c, "usb_submit_urb" },
	{ 0xbcab6ee6, "sscanf" },
	{ 0xc4734a4e, "input_event" },
	{ 0xa19b956, "__stack_chk_fail" },
	{ 0x193343b1, "usb_deregister" },
	{ 0xf02dd3c3, "kmalloc_caches" },
	{ 0x58ffa277, "kmalloc_trace" },
	{ 0x883f49fd, "usb_get_dev" },
	{ 0xb9231a77, "input_allocate_device" },
	{ 0x91c22118, "input_set_abs_params" },
	{ 0x98b29866, "input_register_device" },
	{ 0x93d7746e, "usb_alloc_coherent" },
	{ 0xad3d0440, "usb_alloc_urb" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x5219c37a, "module_layout" },
};

MODULE_INFO(depends, "usbcore");

MODULE_ALIAS("usb:v0483p5740d*dc*dsc*dp*ic*isc*ip*in*");
