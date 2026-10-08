import os
import sys
import time
import socket
import subprocess
from PIL import Image

QEMU_PATH = r"C:\Program Files\qemu\qemu-system-x86_64.exe"
OS_IMG = r"build\os.img"
DISK_IMG = r"build\disk.img"
MONITOR_PORT = 4555

def run():
    # Kill any running qemu
    os.system("taskkill /IM qemu-system-x86_64.exe /F >nul 2>&1")
    time.sleep(1)

    cmd = [
        QEMU_PATH,
        "-m", "512M",
        "-drive", f"format=raw,file={OS_IMG}",
        "-drive", f"format=raw,file={DISK_IMG},if=ide,index=1",
        "-netdev", "user,id=u1",
        "-device", "rtl8139,netdev=u1",
        "-display", "none",
        "-serial", "file:build/serial_pip.log",
        "-monitor", f"telnet:127.0.0.1:{MONITOR_PORT},server,nowait"
    ]

    print("Starting QEMU...")
    proc = subprocess.Popen(cmd)
    try:
        time.sleep(4)
        print("Connecting to QEMU monitor...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", MONITOR_PORT))

        def send_mon(command):
            sock.sendall((command + "\n").encode("ascii"))
            time.sleep(0.6)

        # Dump initial normal window
        print("Dumping normal window screen...")
        send_mon("screendump build/screen_normal.ppm")
        time.sleep(0.5)

        # Send Ctrl+P
        print("Sending Ctrl+P...")
        send_mon("sendkey ctrl-p")
        # Wait for smooth ease-out animation
        time.sleep(1.5)

        # Dump PiP mode window
        print("Dumping PiP mode screen...")
        time.sleep(2.5)
        send_mon("screendump build/screen_pip.ppm")
        time.sleep(0.5)

        # Toggle back out of PiP
        print("Sending Ctrl+P again (restore)...")
        send_mon("sendkey ctrl-p")
        time.sleep(1.5)
        send_mon("screendump build/screen_restored.ppm")
        time.sleep(0.5)

        sock.close()

        # Convert PPMs to PNG
        for name in ["screen_normal", "screen_pip", "screen_restored"]:
            ppm_path = f"build/{name}.ppm"
            png_path = f"build/{name}.png"
            if os.path.exists(ppm_path):
                img = Image.open(ppm_path)
                img.save(png_path)
                print(f"Saved {png_path} ({img.size})")

        print("PiP Test completed successfully!")
    finally:
        proc.kill()
        proc.wait()

if __name__ == "__main__":
    run()
