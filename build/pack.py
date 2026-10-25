"""Pack the release zip: a confirm-on-x folder to drop into the recomp's mods folder."""
import os
import sys
import zipfile


def main(nro, readme, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(nro, 'confirm-on-x/romfs/nativeNX/plugins/confirm_on_x.nro')
        z.write(readme, 'confirm-on-x/README.txt')
    print('wrote %s' % dst)


if __name__ == '__main__':
    main(*sys.argv[1:4])
