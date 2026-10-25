# build_spiffs.py
Import("env")
import subprocess

def before_build(source, target, env):
    subprocess.run([
        "python",
        env.subst("$PROJECT_PACKAGES_DIR") + "/framework-espidf/components/spiffs/spiffsgen.py",
        "0x200000",
        "../../components/esp_ot_br_server/frontend",
        env.subst("$BUILD_DIR") + "/web_storage.bin"
    ], check=True)

def after_upload(source, target, env):
    port = env.subst("$UPLOAD_PORT")
    subprocess.run([
        "python",
        env.subst("$PROJECT_PACKAGES_DIR") + "/tool-esptoolpy/esptool.py",
        "--chip", "esp32c6",
        "-p", port,
        "-b", "460800",
        "write_flash", "0x820000",
        env.subst("$BUILD_DIR") + "/web_storage.bin"
    ], check=True)

env.AddPreAction("buildprog", before_build)
env.AddPostAction("upload", after_upload)