#!/usr/bin/env python
import os
import os.path
import subprocess
from optparse import OptionParser

parser = OptionParser()
parser.add_option('--debug', dest='debug', action='store_true')
parser.add_option('--release', dest='debug', action='store_false', default=False)
(opts, args) = parser.parse_args()

cenv=os.environ
if cenv['PLATFORM'] == 'Linux-mipsel':
    cdir=cenv['CHROMIUM_BASE']
    scrd=cenv['WORKSPACE'].replace(cenv['CHROMIUM_CHROOT'],'',1)
    prefix=[os.path.join(cenv['DEPOT_TOOLS'], 'cros_sdk'), '--']
    if 'OHDEVTOOLS_ROOT' in cenv:
        prefix += ['OHDEVTOOLS_ROOT='+ cenv['OHDEVTOOLS_ROOT'],]
else:
    cdir=cenv['WORKSPACE']
    scrd=cenv['WORKSPACE']
    prefix=[]

cargs = ['-t', cenv['PLATFORM']]
if opts.debug:
    cargs += ['--debug',]
else:
    cargs += ['--release',]
if cenv['RELEASE_VERSION']:
    cargs += ['--steps=default,publish', '--publish-version', cenv['RELEASE_VERSION']]
else:
    cargs += ['--steps=default',]

subprocess.check_call(args=prefix + [os.path.join(scrd, 'ohNetmon', 'go'), 'ci-build'] + cargs, cwd=cdir)

