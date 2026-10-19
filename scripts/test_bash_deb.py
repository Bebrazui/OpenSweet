import subprocess
import time
import socket
import os
from PIL import Image

def run():
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
            time.sleep(0.15)

        def send_text(text):
            keymap = {' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus', '_': 'shift-minus', '\n': 'ret', '"': 'shift-apostrophe'}
            for ch in text:
                if ch in keymap:
                    send(f'sendkey {keymap[ch]}')
                elif ch.isupper():
                    send(f'sendkey shift-{ch.lower()}')
                else:
                    send(f'sendkey {ch}')
            send('sendkey ret')

        print("Step 1: Unpacking /bash.deb with dpkg-deb -x...")
        send_text('dpkg-deb -x /bash.deb /')
        time.sleep(45)

        print("Step 2: Checking /bin/bash...")
        send_text('ls /bin/bash')
        time.sleep(6)

        print("Step 3: Running /bin/bash --version...")
        send_text('/bin/bash --version')
        time.sleep(10)

        send('screendump build/bash_test_result.ppm')
        time.sleep(1)
        send('quit')
        time.sleep(1)
        s.close()
    except Exception as e:
        print("Error:", e)
    finally:
        try:
            proc.kill()
        except:
            pass

    if os.path.exists("build/bash_test_result.ppm"):
        img = Image.open("build/bash_test_result.ppm")
        img.save("build/bash_test_result.png")
        print("Successfully saved build/bash_test_result.png!")
        w, h = img.size
        crop_box = (60, 40, int(w * 0.75), int(h * 0.75))
        cropped = img.crop(crop_box)
        cropped.save("build/bash_test_result_crop.png")
        print("Successfully saved build/bash_test_result_crop.png!")

if __name__ == '__main__':
    run()
