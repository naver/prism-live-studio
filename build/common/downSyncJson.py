# coding:utf-8
import hmac,base64,time
import json, requests
from urllib import parse
import sys, os, shutil, platform
import zipfile, tempfile
import argparse


"""
eg: Python downSyncJson.py --path="C:/rootSource/prism-live-studio/src/prism-live-studio/PRISMLiveStudio" -v=4.3.0.587 --dev
"""

if __name__ == '__main__':
    print("done")