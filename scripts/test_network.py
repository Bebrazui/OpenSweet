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
        "-netdev", "user,id=net0",
        "-device", "rtl8139,netdev=net0",
        "-display", "none",
        "-serial", "file:build\\serial.log",
        "-monitor", "telnet:127.0.0.1:4444,server,nowait"
    ]
    
    proc = subprocess.Popen(args)
    print("QEMU started with RTL8139, waiting for OS boot & network init...")
    time.sleep(8)

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', 4444))

        def send(cmd):
            s.sendall((cmd + '\n').encode('ascii'))
            time.sleep(0.12)

        def send_text(text):
            for ch in text:
                if ch == ' ':
                    send('sendkey spc')
                elif ch == '-':
                    send('sendkey minus')
                elif ch == '/':
                    send('sendkey slash')
                elif ch == '.':
                    send('sendkey dot')
                elif ch == '_':
                    send('sendkey shift-minus')
                else:
                    send(f'sendkey {ch}')
            send('sendkey ret')

        print("Sending 'ifconfig' command...")
        send_text('ifconfig')
        time.sleep(3)

        send('screendump build/net_test_result.ppm')
        time.sleep(1)
        send('quit')
        time.sleep(1)
        s.close()
    finally:
        subprocess.run(["powershell", "-Command", "Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)

    if os.path.exists('build/net_test_result.ppm'):
        img = Image.open('build/net_test_result.ppm')
        img.save('build/net_test_result.png')
        print("Successfully saved build/net_test_result.png!")
        crop_box = (100, 80, 920, 650)
        img_crop = img.crop(crop_box)
        img_crop.save('build/net_test_result_crop.png')
        print("Successfully saved build/net_test_result_crop.png!")

if __name__ == '__main__':
    run()
