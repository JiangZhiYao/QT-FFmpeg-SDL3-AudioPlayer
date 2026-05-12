import os
import chardet
import shutil

# 需要处理的文件扩展名
TARGET_EXT = {".cpp", ".h", ".hpp", ".c", ".txt", ".ini", ".rc"}

# 是否使用 UTF-8 BOM
USE_BOM = True  # True = UTF-8 with BOM, False = UTF-8 without BOM

def convert_file(path, do_backup):
    with open(path, "rb") as f:
        raw = f.read()

    detect = chardet.detect(raw)
    enc = detect["encoding"]

    if enc is None:
        print(f"[跳过] 无法识别编码: {path}")
        return

    if enc.lower().startswith("utf-8"):
        print(f"[跳过] 已是 UTF-8: {path}")
        return

    # 备份原文件
    if do_backup:
        backup = path + ".bak"
        if not os.path.exists(backup):
            shutil.copy2(path, backup)

    try:
        text = raw.decode(enc)
        with open(path, "w", encoding="utf-8-sig" if USE_BOM else "utf-8") as f:
            f.write(text)
        print(f"[转换] {enc} → UTF-8 : {path}")
    except Exception as e:
        print(f"[失败] {path} : {e}")

def scan_folder(root, do_backup):
    for base, dirs, files in os.walk(root):
        for name in files:
            ext = os.path.splitext(name)[1].lower()
            if ext in TARGET_EXT:
                convert_file(os.path.join(base, name), do_backup)

if __name__ == "__main__":
    # 输入路径
    user_input = input("请输入要处理的目录（留空则使用当前目录）: ").strip()
    root_path = user_input if user_input else "."

    # 输入是否备份
    backup_input = input("是否备份原文件？(y/n，默认 y): ").strip().lower()
    do_backup = (backup_input != "n")

    print(f"\n开始处理目录: {os.path.abspath(root_path)}")
    print(f"备份原文件: {'是' if do_backup else '否'}\n")

    scan_folder(root_path, do_backup)

    print("\n全部处理完成。")
