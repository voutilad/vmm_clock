# vmm_clock

TBA...but in short this will be an attempt at a bare-minimum Linux
clocksource implementation based on Linux's existing kvmclock for use
when running Linux distros as guests under OpenBSD's vmm(4)/vmd(8)
hypervisor framework.

Primary goals:
- provide a clocksource that doesn't suffer from unmanageable
  clock-drift as seen when using refined-jiffies with a more recent
  Linux LTS kernel (e.g. 5.4)
- provide a clocksource loadable as a module that does not require
  users build a complete kernel from source

Secondary goals:
- make the code as short and tight as possible
- identify a means of testing clock drift

## Building & Installing

For now, follow some of the pre-reqs for
[virtio_vmmci](https://github.com/voutilad/virtio_vmmci) and you
should just be able to:

```sh
# make && make install && modprobe vmm_clock
```

You can load it at boot by creating
`/etc/modules-load.d/vmm_clock.conf` with just the concents:

```
vmm_clock
```

## Tested Platforms and Configs

I'm testing with the following:
- Alpine 3.11.6 and the stock v5.4 `-virt` kernel
- Debian Buster 10.4 and its v4.19 kernel

In all cases, I'm booting with the following kernel arguments:
- `tsc=reliable`
- `tsc=noirqtime`

Of little to not consequence, I do also use:
- `nosmp`
- `elevator=noop`


As usual, no warranty...use at your own risk, etc.!

# License
GPLv2...see LICENSE file.
