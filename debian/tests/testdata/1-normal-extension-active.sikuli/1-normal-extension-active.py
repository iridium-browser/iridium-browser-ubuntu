import os
import subprocess
import signal
import sys

app = subprocess.Popen(["chromium-browser", "--window-size=1000,2200", "--window-position=50,100", "--user-data-dir=profile_storage", "--new-window", os.environ["LOCALURL"]])

try:
    wait("menu-button.png", 60)
    click("menu-button.png")
    click("Settings-1.png")
    click("Extensions.png")
    find("UnityWEBAPPS.png")   # NOT grayed out
    # If the color varies, then add find().right.find(a checkbox)
finally:
    if app.pid:
        os.kill(app.pid, signal.SIGTERM)
        app.wait()
    else:
        # Jython python2.5 hackey. So so sorry.
        app._process.destroy()
