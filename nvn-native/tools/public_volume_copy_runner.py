"""True3D profile of the same audited public copy collector/stop protocol."""
import public_copy_runner as shared
stopped=shared.stopped
def profile(record):return shared.profile(record,volume=True)
def exercise(args,build,record,adb,package,component,run):
 return shared.exercise(args,build,record,adb,package,component,run,volume=True)
