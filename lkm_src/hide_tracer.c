// SPDX-License-Identifier: GPL-3.0
/*
 * hide_tracer.c — Hides TracerPid inside /proc/PID/status using kretprobes.
 *
 * This module hooks 'proc_pid_status', the kernel function that fills the
 * /proc/[PID]/status file, with a kretprobe. Once the function completes, it
 * locates the "TracerPid:" value in the output buffer and dynamically
 * overwrites it with "0".
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kprobes.h>
#include <linux/seq_file.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Enes Hamza");
MODULE_DESCRIPTION("Dynamically masks TracerPid in /proc/PID/status");
MODULE_VERSION("1.0");

static struct kretprobe status_krp;

/*
 * Entry handler: saves the first argument ('struct seq_file *m', in RDI on
 * x86_64) into ri->data. The register is only guaranteed to hold the argument
 * at function entry; by the time the return handler runs it may have been
 * clobbered, so it must be captured here.
 */
static int status_entry_handler(struct kretprobe_instance *ri,
                                struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
    *(void **)ri->data = (void *)regs->di;
#else
    *(void **)ri->data = NULL;
#endif
    return 0;
}

/*
 * Return handler: runs when proc_pid_status completes. The buffer to fill is
 * 'struct seq_file *m', read back from ri->data (captured at entry).
 */
static int status_ret_handler(struct kretprobe_instance *ri,
                              struct pt_regs *regs)
{
    struct seq_file *m = *(struct seq_file **)ri->data;

    if (m && m->buf && m->count > 0) {
        char *pos = strnstr(m->buf, "TracerPid:", m->count);
        if (pos) {
            // Skip past the "TracerPid:" label (its length is 10) to the value.
            char *ptr = pos + 10;

            // Skip tabs and spaces.
            while (ptr < (m->buf + m->count) &&
                   (*ptr == '\t' || *ptr == ' ')) {
                ptr++;
            }

            // If the next character is a digit, replace it with '0' and pad
            // the remaining digits with spaces, masking TracerPid without
            // breaking the file format.
            if (ptr < (m->buf + m->count) && *ptr >= '0' && *ptr <= '9') {
                // Log the real TracerPid value (only when it is non-zero).
                if (*ptr != '0') {
                    char real_pid[16] = {0};
                    int i = 0;
                    char *temp = ptr;
                    while (temp < (m->buf + m->count) && *temp >= '0' &&
                           *temp <= '9' && i < 15) {
                        real_pid[i++] = *temp;
                        temp++;
                    }
                    pr_info("[hide_tracer] TracerPid detected and masked: %s -> 0\n", real_pid);
                }

                *ptr = '0'; // TracerPid: 0
                ptr++;
                // Overwrite the remaining digits with spaces.
                while (ptr < (m->buf + m->count) && *ptr >= '0' && *ptr <= '9') {
                    *ptr = ' ';
                    ptr++;
                }
            }
        }
    }
    return 0;
}

static int __init hide_tracer_init(void)
{
    int ret;

    status_krp.handler = status_ret_handler;
    status_krp.entry_handler = status_entry_handler;
    status_krp.data_size = sizeof(struct seq_file *);
    status_krp.maxactive = 20;

    // The kernel function that fills the /proc/[PID]/status file.
    status_krp.kp.symbol_name = "proc_pid_status";

    ret = register_kretprobe(&status_krp);
    if (ret < 0) {
        pr_err("[hide_tracer] kretprobe registration failed: %d\n", ret);
        return ret;
    }

    pr_info("[hide_tracer] TracerPid masking module loaded successfully.\n");
    return 0;
}

static void __exit hide_tracer_exit(void)
{
    unregister_kretprobe(&status_krp);
    pr_info("[hide_tracer] TracerPid masking module unloaded.\n");
}

module_init(hide_tracer_init);
module_exit(hide_tracer_exit);