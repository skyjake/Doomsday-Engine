#!/usr/bin/env python3
#
# macOS App Notarization
# Usage: notarize.py (app) (apple-id-email)
#
# The given app bundle is (eventually) stapled with a notarization ticket.
# Waits until Apple's servers finish checking the files. Returns non-zero
# on error.

import json
import os
import subprocess
import sys
import time
import urllib.request
import xml.etree.ElementTree as xet
pjoin = os.path.join

BUNDLE_PATH   = sys.argv[1]
APPLE_ID      = sys.argv[2]
POLL_INTERVAL = 60 # seconds
START_TIME    = time.time()


def parse_dict(root):
    res = {}
    key = None
    for child in root:
        if key:
            if child.tag == 'string':
                res[key] = child.text
            elif child.tag == 'integer':
                res[key] = int(child.text)
            key = None
        elif child.tag == 'key':
            key = child.text                    
    return res

def key_value(root, key):
    found_key = False
    for child in root:
        if child.tag == 'dict':
            for elem in child:
                if elem.tag == 'key':
                    if elem.text == key:
                        found_key = True
                elif found_key:
                    if elem.tag == 'string':
                        return elem.text
                    elif elem.tag == 'dict':
                        return parse_dict(elem)
    return None    


# Check the bundle ID.
app_info = xet.parse(pjoin(BUNDLE_PATH, 'Contents/Info.plist')).getroot()
bundle_id = key_value(app_info, 'CFBundleIdentifier')
print('Bundle Identifier:', bundle_id)

# Compress the application for upload.
pbid = bundle_id + '.zip'
print('Compressing:', pbid)
subprocess.check_call(['zip', '-9', '-r', '-y', pbid, BUNDLE_PATH])

# Submit the compressed app.
print('Submitting for notarization...')
result_text = subprocess.check_output(['/usr/bin/xcrun', 'altool',
    '--notarize-app',
    '--primary-bundle-id', pbid,
    '--username', APPLE_ID,
    '--password', '@keychain:notarize.py',
    '--file', pbid,
    '--output-format', 'xml'
])

# Check the submission result.
result = xet.fromstring(result_text)
req_uuid = key_value(result, 'notarization-upload')['RequestUUID']
msg = key_value(result, 'success-message')
print('Request UUID:', req_uuid)
print('Status:', msg)
os.remove(pbid) # clean up

# Wait for the processing to finish.
print('Waiting for result...')
while True:
    result_text = subprocess.check_output(['/usr/bin/xcrun', 'altool',
        '--notarization-info', req_uuid,
        '--username', APPLE_ID,
        '--password', '@keychain:notarize.py',
        '--output-format', 'xml'
    ])
    print(result_text)
    result = xet.fromstring(result_text)
    info = key_value(result, 'notarization-info')
    if info['Status'] != 'in progress':
        break
    time.sleep(POLL_INTERVAL)
    
print('Finished at:', info['Date'])
log_file_url = info['LogFileURL']
status = info['Status']
status_code = info['Status Code']
status_msg = info['Status Message']
print('%s (%d): %s' % (status, status_code, status_msg))

# Check errors and warnings.
local_fn, headers = urllib.request.urlretrieve(log_file_url)
log = json.load(open(local_fn))
print(log)
# TODO: If there are issues, abort build with an error code.

# Staple.
subprocess.check_call(['/usr/bin/xcrun', 'stapler', 'staple', BUNDLE_PATH])

# Verify.
print(subprocess.check_output(['/usr/sbin/spctl', '-a', '-v', BUNDLE_PATH]))
# TODO: If verification fails, abort build with an error code.

# Successful.
nb_sec  = int(time.time() - START_TIME)
nb_hour = int(nb_sec / 3600)
nb_min  = int(nb_sec / 60) % 60
print('Notarization finished in %dh %dm %ds' % (nb_hour, nb_min, nb_sec % 60))
sys.exit(0)
