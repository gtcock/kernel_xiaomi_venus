// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/version.h>

static struct kobject *cpu_power_kobj;
static int limit_enabled = 0;

static unsigned int low_max_freq[] = {
    1804800, 1804800, 1804800, 1804800,
    2227200, 2227200, 2227200,
    2496000
};

static unsigned int normal_max_freq[] = {
    1804800, 1804800, 1804800, 1804800,
    2400000, 2400000, 2400000,
    2841600
};

static void set_gpu_pwrlevel(int level)
{
    struct file *f;
    mm_segment_t oldfs;
    loff_t pos = 0;
    char buf[8];

    snprintf(buf, sizeof(buf), "%d\n", level);

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    f = filp_open("/sys/class/kgsl/kgsl-3d0/max_pwrlevel", O_WRONLY, 0);
    if (!IS_ERR(f)) {
        kernel_write(f, buf, strlen(buf), &pos);
        filp_close(f, NULL);
    }
    set_fs(oldfs);
}

static void set_max_freqs(bool low)
{
    int cpu;
    for_each_online_cpu(cpu) {
        struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
        if (!policy)
            continue;

        if (cpu < ARRAY_SIZE(low_max_freq)) {
    	    unsigned int freq = low ? low_max_freq[cpu] : normal_max_freq[cpu];
    	    policy->max = freq;
    	    if (policy->cur > policy->max)
        	cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_H);
	}

        cpufreq_cpu_put(policy);
    }

    set_gpu_pwrlevel(low ? 4 : 0);
}

static ssize_t limit_mode_show(struct kobject *kobj,
                                struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", limit_enabled);
}

static ssize_t limit_mode_store(struct kobject *kobj,
                                struct kobj_attribute *attr,
                                const char *buf, size_t count)
{
    int val;
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;

    limit_enabled = !!val;
    set_max_freqs(limit_enabled);
    return count;
}

static struct kobj_attribute limit_mode_attribute =
    __ATTR(limit_mode, 0664, limit_mode_show, limit_mode_store);

static int __init cpu_power_toggle_init(void)
{
    cpu_power_kobj = kobject_create_and_add("cpu_power_toggle", kernel_kobj);
    if (!cpu_power_kobj)
        return -ENOMEM;

    return sysfs_create_file(cpu_power_kobj, &limit_mode_attribute.attr);
}

static void __exit cpu_power_toggle_exit(void)
{
    sysfs_remove_file(cpu_power_kobj, &limit_mode_attribute.attr);
    kobject_put(cpu_power_kobj);
}

module_init(cpu_power_toggle_init);
module_exit(cpu_power_toggle_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple toggle to limit CPU/GPU performance");
MODULE_AUTHOR("KamiKaonashi");
