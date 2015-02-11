A Linux kernel module that registers a keyboard IRQ
handler (ISR) and logs the keystrokes as they are received. To keep
the ISR as short as possible, each scancode is passed to a tasklet
that is then run at the next available time and logs the key.
