# Copyright 2015 Dietrich Epp.
# This file is part of SGGL.  SGGL is licensed under the terms of the
# 2-clause BSD license.  For more information, see LICENSE.txt.
import argparse
import os
import platform
import sys
from . import Registry, API_LIST, parse_version, parse_api

def die(msg):
    """Show an error message and exit, indicating failure."""
    print('Error:', msg, file=sys.stderr)
    raise SystemExit(1)

_PLATFORMS = {'Darwin': 'osx', 'Linux': 'linux', 'Windows': 'windows'}
def get_platform():
    """Get the host platform."""
    s = platform.system()
    try:
        return _PLATFORMS[s]
    except KeyError:
        die('Unknown system {!r}, specify --platform.'.format(s))

def cmd_emit(reg, args):
    """Run the command-line 'emit' command."""
    import shutil
    platform = args.platform
    if platform is None:
        platform = get_platform()
    deps, data = reg.get_data(
        max_version=args.max_version,
        extensions=args.extensions,
        platform=platform)
    fnames = set(data.keys()).union(['opengl_load.c'])
    dirpath = os.path.dirname(__file__)
    for fname in sorted(fnames):
        path = os.path.join(args.out, fname)
        print('Creating {}...'.format(path))
        try:
            fdata = data[fname]
        except KeyError:
            shutil.copyfile(os.path.join(dirpath, fname), path)
        else:
            with open(path, 'wb') as fp:
                fp.write(fdata)

def library_functions(path):
    """Get a list of functions defined in a library."""
    import platform
    import subprocess
    strip_underscore = False
    args = ['nm']
    system = platform.system()
    print('System', system)
    if system == 'Darwin':
        strip_underscore = True
        args.append('-U')
    elif system == 'Linux':
        args.extend(('--dynamic', '--defined-only'))
    args.append(path)
    stdout = subprocess.check_output(args)
    for line in stdout.splitlines():
        fields = line.split()
        if fields[-2] != b'T':
            continue
        name = fields[2]
        if strip_underscore:
            if name.startswith(b'_'):
                name = name[1:]
            else:
                continue
        name = name.decode('ASCII')
        yield name

def cmd_scan(reg, args):
    """Run the command-line 'scan' command."""
    if args.library is None:
        path = ('/System/Library/Frameworks/OpenGL.framework/Versions'
                '/Current/Libraries/libGL.dylib')
    else:
        path = args.library
    functions = set()
    for func in library_functions(path):
        if func.startswith('gl'):
            functions.add(func[2:])
    dfunctions = set()
    max_version = 0, 0
    for version in sorted(reg.features):
        iface = reg._get_core(version, False)
        if not (functions >= iface.commands):
            break
        max_version = version
        dfunctions.update(iface.commands)
    print('Maximum version: {0[0]}.{0[1]}'.format(version))
    nsup, nuns, nzer, nred = 0, 0, 0, 0
    print('Extensions:')
    for ext in sorted(reg.extensions):
        iface = reg._get_extension(ext, False)
        if not iface.commands:
            nzer += 1
        elif dfunctions >= iface.commands:
            nred += 1
        elif not (functions >= iface.commands):
            nuns += 1
        else:
            nsup += 1
            print('  ' + ext)
    print('Extensions supported: {} ({} not redundant)'
          .format(nsup + nred, nsup))
    print('Extensions unsupported:', nuns)
    print('Extensions without entry points:', nzer)

def main():
    import argparse
    p = argparse.ArgumentParser()

    p.add_argument(
        '--reg-path',
        help='Path to the OpenGL XML registry file')
    p.add_argument(
        '--api',
        choices=API_LIST,
        default='gl:core',
        help='The OpenGL API')

    ss = p.add_subparsers()
    ss.required = True
    ss.dest = 'cmd'

    pp = ss.add_parser('emit', help='emit OpenGL headers and data')
    pp.add_argument(
        '--out', default='.',
        help='Output directory')
    pp.add_argument(
        '--platform', choices=('windows', 'osx', 'linux'))
    pp.add_argument(
        'max_version', type=parse_version,
        help='Maximum version to emit, e.g., 3.2')
    pp.add_argument(
        'extensions', nargs='*',
        help='Extensions to emit')
    pp.set_defaults(cmd=cmd_emit)

    pp = ss.add_parser('scan', help='calculate supported OpenGL version')
    pp.set_defaults(cmd=cmd_scan)
    pp.add_argument(
        '--library', help='library to scan')

    args = p.parse_args()
    api, profile = parse_api(args.api)
    reg_path = args.reg_path
    r = Registry.load(reg_path, api, profile)
    args.cmd(r, args)

if __name__ == '__main__':
    main()
