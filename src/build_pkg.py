# -*- coding: utf-8 -*-
"""
redapid (RED Brick API Daemon)
Copyright (C) 2014-2015 Matthias Bolte <matthias@tinkerforge.com>
Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>

build_pkg.py: Package builder for RED Brick API Daemon

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


def system(command):
    if os.system(command) != 0:
        sys.exit(1)


def check_output(*args, **kwargs):
    if 'stdout' in kwargs:
        raise ValueError('stdout argument not allowed, it will be overridden')

    process = subprocess.Popen(stdout=subprocess.PIPE, *args, **kwargs)
    output, error = process.communicate()
    exit_code = process.poll()

    if exit_code != 0:
        command = kwargs.get('args')

        if command == None:
            command = args[0]

        raise subprocess.CalledProcessError(exit_code, command, output=output)

    return output


def specialize_template(template_filename, destination_filename, replacements):
    template_file = open(template_filename, 'rb')
    lines = []
    replaced = set()

    for line in template_file.readlines():
        for key in replacements:
            replaced_line = line.replace(key, replacements[key])

            if replaced_line != line:
                replaced.add(key)

            line = replaced_line

        lines.append(line)

    template_file.close()

    if replaced != set(replacements.keys()):
        raise Exception('Not all replacements for {0} have been applied'.format(template_filename))

    destination_file = open(destination_filename, 'wb')
    destination_file.writelines(lines)
    destination_file.close()


def build_linux_pkg():
    print('building redapid Debian package')
    root_path = os.getcwd()

    print('removing old build directories')
    dist_path = os.path.join(root_path, 'dist')

    if os.path.exists(dist_path):
        shutil.rmtree(dist_path)

    architecture = check_output(['dpkg', '--print-architecture']).replace('\n', '')

    print('compiling for ' + architecture)
    system('cd redapid; make clean')
    system('cd redapid; env CC=gcc make')

    print('copying build data')
    build_data_path = os.path.join(root_path, 'build_data', 'linux')
    shutil.copytree(build_data_path, dist_path)

    print('copying redapid binary')
    bin_path = os.path.join(dist_path, 'usr', 'bin')
    os.makedirs(bin_path)
    shutil.copy('redapid/redapid', bin_path)

    print('creating DEBIAN/control from template')
    version = check_output(['./redapid/redapid', '--version']).replace('\n', '').replace(' ', '-')
    installed_size = int(check_output(['du', '-s', '--exclude', 'dist/DEBIAN', 'dist']).split('\t')[0])
    control_path = os.path.join(dist_path, 'DEBIAN', 'control')
    specialize_template(control_path, control_path,
                        {'<<VERSION>>': version,
                         '<<ARCHITECTURE>>': architecture,
                         '<<INSTALLED_SIZE>>': str(installed_size)})

    print('preparing files')
    system('objcopy --strip-debug --strip-unneeded dist/usr/bin/redapid')
    system('cp ../changelog dist/usr/share/doc/redapid/')

    system('gzip -9 dist/usr/share/doc/redapid/changelog')
    system('gzip -9 dist/usr/share/man/man8/redapid.8')
    system('gzip -9 dist/usr/share/man/man5/redapid.conf.5')

    system('cd dist; find usr -type f -exec md5sum {} \; >> DEBIAN/md5sums')

    system('find dist -type d -exec chmod 0755 {} \;')

    os.chmod('dist/DEBIAN/conffiles', 0644)
    os.chmod('dist/DEBIAN/md5sums', 0644)
    os.chmod('dist/DEBIAN/preinst', 0755)
    os.chmod('dist/DEBIAN/postinst', 0755)
    os.chmod('dist/DEBIAN/prerm', 0755)
    os.chmod('dist/DEBIAN/postrm', 0755)

    os.chmod('dist/usr/bin/redapid', 0755)
    os.chmod('dist/etc/redapid.conf', 0644)
    os.chmod('dist/etc/init.d/redapid', 0755)
    os.chmod('dist/etc/cron.d/redapid-delete-purged-programs', 0644)
    os.chmod('dist/etc/logrotate.d/redapid', 0644)
    os.chmod('dist/usr/share/doc/redapid/changelog.gz', 0644)
    os.chmod('dist/usr/share/doc/redapid/copyright', 0644)
    os.chmod('dist/usr/share/man/man8/redapid.8.gz', 0644)
    os.chmod('dist/usr/share/man/man5/redapid.conf.5.gz', 0644)

    print('changing owner to root')
    system('sudo chown -R root:root dist')

    print('building Debian package')
    system('dpkg -b dist redapid-{0}_{1}.deb'.format(version, architecture))

    print('changing owner back to original user')
    system('sudo chown -R `logname`:`logname` dist')

    print('checking Debian package')
    system('lintian --pedantic redapid-{0}_{1}.deb'.format(version, architecture))

    print('cleaning up')
    system('cd redapid; make clean')


# run 'python build_pkg.py' to build the linux package
if __name__ == '__main__':
    if sys.platform != 'win32' and os.geteuid() == 0:
        print('error: must not be started as root, exiting')
        sys.exit(1)

    if sys.platform.startswith('linux'):
        build_linux_pkg()
    else:
        print('error: unsupported platform: ' + sys.platform)
        sys.exit(1)

    print('done')
