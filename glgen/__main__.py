# Copyright 2015 Dietrich Epp.
# This file is part of SGGL.  SGGL is licensed under the terms of the
# 2-clause BSD license.  For more information, see LICENSE.txt.
import argparse
import os
from . import Registry, API_LIST, parse_version, parse_api

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

    pp = ss.add_parser('emit', help='emit OpenGL headers and data')
    pp.add_argument(
        '--out', default='.',
        help='Output directory')
    pp.add_argument(
        'platform', choices=('windows', 'osx', 'linux'))
    pp.add_argument(
        'max_version', type=parse_version,
        help='Maximum version to emit, e.g., 3.2')
    pp.add_argument(
        'extensions', nargs='*',
        help='Extensions to emit')
    pp.set_defaults(cmd='emit')

    pp = ss.add_parser('scan', help='calculate supported OpenGL version')
    pp.set_defaults(cmd='scan')
    pp.add_argument(
        '--library', help='library to scan')

    args = p.parse_args()
    api, profile = parse_api(args.api)
    reg_path = args.reg_path
    r = Registry.load(reg_path, api, profile)
    getattr(r, 'cmd_' + args.cmd)(args)

if __name__ == '__main__':
    main()
