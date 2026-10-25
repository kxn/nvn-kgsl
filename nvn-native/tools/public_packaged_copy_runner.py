"""Ordinary UID existing Activity/strict-stop transport, experimental8B profile."""
from public_copy_runner import exercise as shared_exercise
def exercise(args,build,record,adb,package,component,run):return shared_exercise(args,build,record,adb,package,component,run,packaged=True)
