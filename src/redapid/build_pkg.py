# -*- coding: utf-8 -*-
"""
redapid (RED Brick API Daemon)
Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>

redapid_pkg.py: Package builder for RED Brick API Daemon

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
"""

import sys
import os
import shutil
import subprocess
import re
import glob

def check_output(*popenargs, **kwargs):
    if 'stdout' in kwargs:
        raise ValueError('stdout argument not allowed, it will be overridden.')
    process = subprocess.Popen(stdout=subprocess.PIPE, *popenargs, **kwargs)
    output, unused_err = process.communicate()
    retcode = process.poll()
    if retcode:
        cmd = kwargs.get("args")
        if cmd is None:
            cmd = popenargs[0]
        raise subprocess.CalledProcessError(retcode, cmd, output=output)
    return output

def build_linux_pkg():
    if os.geteuid() != 0:
        sys.stderr.write("build_pkg for Linux has to be started as root, exiting\n")
        sys.exit(1)

    architecture = check_output(['dpkg', '--print-architecture']).replace('\n', '')

    print 'Building version for ' + architecture

    os.system('make clean')
    os.system('CC=gcc make')

    version = check_output(['./redapid', '--version']).replace('\n', '').replace(' ', '-')

    dist_dir = os.path.join(os.getcwd(), 'dist')
    if os.path.exists(dist_dir):
        shutil.rmtree(dist_dir)

    build_data_dir = os.path.join(os.getcwd(), '..', 'build_data', 'linux')
    shutil.copytree(build_data_dir, dist_dir)

    bin_dir = os.path.join(os.getcwd(), 'dist', 'usr', 'bin')
    os.makedirs(bin_dir)
    shutil.copy('redapid', bin_dir)

    control_name = os.path.join(os.getcwd(), 'dist', 'DEBIAN', 'control')
    lines = []
    for line in file(control_name, 'rb').readlines():
        line = line.replace('<<REDAPID_VERSION>>', version)
        line = line.replace('<<REDAPID_ARCHITECTURE>>', architecture)
        lines.append(line)
    file(control_name, 'wb').writelines(lines)

    os.system('objcopy --strip-debug --strip-unneeded dist/usr/bin/redapid')

    os.system('cp ../../changelog dist/usr/share/doc/redapid/')

    os.system('gzip -9 dist/usr/share/doc/redapid/changelog')
    os.system('gzip -9 dist/usr/share/man/man8/redapid.8')
    os.system('gzip -9 dist/usr/share/man/man5/redapid.conf.5')

    os.system('cd dist; find usr -type f -exec md5sum {} \; >> DEBIAN/md5sums')

    os.system('chown -R root:root dist/usr')
    os.system('chown -R root:root dist/etc')

    os.system('find dist -type d -exec chmod 0755 {} \;')

    os.chmod('dist/DEBIAN/conffiles', 0644)
    os.chmod('dist/DEBIAN/md5sums', 0644)
    os.chmod('dist/DEBIAN/preinst', 0755)
    os.chmod('dist/DEBIAN/postinst', 0755)
    os.chmod('dist/DEBIAN/prerm', 0755)
    os.chmod('dist/DEBIAN/postrm', 0755)

    os.chmod('dist/etc/redapid.conf', 0644)
    os.chmod('dist/etc/init.d/redapid', 0755)
    os.chmod('dist/etc/cron.d/redapid-delete-purged-programs', 0644)
    os.chmod('dist/etc/logrotate.d/redapid', 0644)
    os.chmod('dist/usr/share/doc/redapid/copyright', 0644)
    os.chmod('dist/usr/share/man/man8/redapid.8.gz', 0644)
    os.chmod('dist/usr/share/man/man5/redapid.conf.5.gz', 0644)

    print 'Packaging...'
    os.system('dpkg -b dist redapid-' + version + '_' + architecture + '.deb')

    print 'Checking...'
    os.system('lintian --allow-root --pedantic redapid-' + version + '_' + architecture + '.deb')

    os.system('make clean')

    print 'Done'


# call python build_pkg.py to build the linux package
if __name__ == "__main__":
    build_linux_pkg()
