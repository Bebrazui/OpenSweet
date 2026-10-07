import subprocess
import time
import socket
import os
from PIL import Image

def run():
    # Kill any existing QEMU
    subprocess.run(["powershell", "-Command", "Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)
    time.sleep(1)

    qemu_exe = r"C:\Program Files\qemu\qemu-system-x86_64.exe"
    args = [
        qemu_exe,
        "-accel", "whpx", "-accel", "tcg",
        "-cpu", "max",
        "-m", "512M",
        "-drive", "format=raw,file=build\\os.img",
        "-drive", "format=raw,file=build\\disk.img,if=ide,index=1",
        "-display", "none",
        "-serial", "file:build\\serial.log",
        "-monitor", "telnet:127.0.0.1:4444,server,nowait"
    ]
    
    proc = subprocess.Popen(args)
    print("QEMU started, waiting for desktop & terminal...")
    time.sleep(12)

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', 4444))

        def send(cmd):
            s.sendall((cmd + '\n').encode('ascii'))
            time.sleep(0.12)

        def send_text(text):
            keymap = {' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus', '_': 'shift-minus', '\n': 'ret'}
            for ch in text:
                if ch in keymap:
                    send(f'sendkey {keymap[ch]}')
                elif ch.isupper():
                    send(f'sendkey shift-{ch.lower()}')
                else:
                    send(f'sendkey {ch}')
            send('sendkey ret')

        print("Sending 'linux_test.elf'...")
        send_text('linux_test.elf')
        time.sleep(10)

        send('screendump build/linux_test_result.ppm')
        time.sleep(1)
        send('quit')
        time.sleep(1)
        s.close()
    finally:
        subprocess.run(["powershell", "-Command", "Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)

    if os.path.exists('build/linux_test_result.ppm'):
        im = Image.open('build/linux_test_result.ppm')
        im.save('build/linux_test_result.png')
        crop = im.crop((10, 10, 800, 600))
        crop.save('build/linux_test_result_crop.png')
        print("Successfully saved build/linux_test_result.png!")
        print("Successfully saved build/linux_test_result_crop.png!")

if __name__ == '__main__':
    run()
