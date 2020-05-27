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
