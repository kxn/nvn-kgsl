"""Boundary classification must preserve a native policy rejection as failure."""
import importlib.util
from pathlib import Path
import unittest
spec = importlib.util.spec_from_file_location('pool_policy_runner', Path(__file__).resolve().parents[1] / 'tools/run_d2r_host.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
class PoolPolicyDiagnostic(unittest.TestCase):
    def test_identity_and_range_are_observations(self):
        text = ('[dm] native >>> nnMain\n'
                '[native NVN D2R] pool_access: pool=0x123456789abc flags=0x00000014 policy=1 offset=4886718345 size=9468016794 consumer=nvnCommandBufferDrawArrays guest_pc=unavailable\n'
                '[native NVN D2R] driver error: nvnCommandBufferDrawArrays; errno=95; phase=after; terminating execution (exit 78)\n')
        result = runner.classify('run',78,False,'',text)
        self.assertEqual(result['pool_access_observations'],[dict(pool='0x123456789abc',flags=0x14,policy=1,offset=4886718345,size=9468016794,consumer='nvnCommandBufferDrawArrays',guest_pc=None)])
        self.assertEqual(result['outcome'],'unsupported-nvn-boundary')
        self.assertFalse(result['passed'])
        self.assertFalse(result['first_frame'])
    def test_observation_alone_is_not_a_boundary(self):
        result = runner.classify('run',78,False,'','[native NVN D2R] pool_access: pool=(nil) flags=0x00000014 policy=1 offset=0 size=64 consumer=synthetic source read guest_pc=unavailable\n')
        self.assertFalse(result['unsupported_boundary'])
        self.assertFalse(result['passed'])
        self.assertEqual(result['pool_access_observations'][0]['consumer'],'synthetic source read')
if __name__=='__main__':unittest.main()
