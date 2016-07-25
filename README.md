# reactos
ReactOS Mirror

Простой PnP AHCI SATA драйвер.
Работает в связке с AtaX (простой аналог драйвера Atapi (WinXP)).

В этой точке я протестировал в  VirtualBox5.0.8 r103449 AtaX и AhciX. Были отключены звук, сеть, USB.

Для SATA контроллера:
*.vdi - SATA port 0,
livecd.iso -  SATA port 1.

Для IDE контроллера (PIIX4):
*.vdi - Primary Master IDE,
livecd.iso -  Primary Slave IDE.

Другие варианты не тестировал.
