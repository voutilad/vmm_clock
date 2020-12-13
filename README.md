# vmm_clock

This is a Linux clocksource implementation based on the existing Linux
`kvmclock`. It's specifically designed for using when running a Linux
kernel as a guest under OpenBSD's vmm(4)/vmd(8) hypervisor framework.

Primary goals:
- provide a clocksource that doesn't suffer from unmanageable
  clock-drift as seen when using refined-jiffies with a more recent
  Linux LTS kernel (e.g. 5.4)
- provide a clocksource loadable as a module that does not require
  users build a complete kernel from source
- be platform independent, i.e. work the same on Intel and AMD hosts.

Secondary goals:
- make the code as short and tight as possible
- identify a means of testing clock drift

Known issues:
- if you try to unload the module, you will probably panic the kernel

## Prerequisites

You'll need OpenBSD 6.8 or newer as it contains fixes I provided for
race conditions in vmd(8) that cause stability problems with Linux
guests. It is 100% required for this kernel module to work. If you try
to use this with an older OpenBSD version, I guarantee you will have
runaway vmd(8) processes or crashes.

## Building & Installing

For now, follow some of the pre-reqs for
[virtio_vmmci](https://github.com/voutilad/virtio_vmmci) and you
should just be able to:

```sh
# make && make install && modprobe vmm_clock
```

You can load it at boot by creating
`/etc/modules-load.d/vmm_clock.conf` with just the contents:

```
vmm_clock
```

## Tested Platforms and Configs

I'm testing with the following:
- Alpine 3.12.2 and their stock v5.4 `-virt` kernel
- Debian Buster 10.4 and its v4.19 kernel

As usual, no warranty...use at your own risk, etc.!

# License
GPLv2...see LICENSE file.
