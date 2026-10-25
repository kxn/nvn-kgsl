#!/usr/bin/env python3
"""Host-only examples for memory-contract.md. No driver, ioctl or GPU execution."""
import unittest
from dataclasses import dataclass

@dataclass(frozen=True)
class Span:
    backing: tuple
    begin: int
    end: int
    def overlaps(self, other):
        return self.backing == other.backing and max(self.begin, other.begin) < min(self.end, other.end)

class Ledger:
    def __init__(self):
        self.authority = 'V0'
        self.writer = set()
        self.readers = set()
        self.pending = {}
        self.reps = {}
    def prepare(self, rid, mode, version):
        deps = self.writer | (self.readers if mode == 'write' else set())
        self.pending[rid] = (mode, version, set(deps))
        return deps
    def cancel(self, rid):
        del self.pending[rid]
    def accept(self, rid, point):
        mode, version, deps = self.pending.pop(rid)
        if mode == 'write':
            self.authority, self.writer, self.readers = version, {point}, set()
        else:
            self.readers.add(point)
        return deps
    def finish_conversion(self, source_version):
        self.reps['raw'] = source_version
        return source_version == self.authority

class Receipt:
    def __init__(self, count):
        self.count = count
        self.accepted = []
        self.unknown = None
        self.attempted = set()
        self.export_error = False
    def attempt(self, index, outcome):
        self.attempted.add(index)
        if outcome == 'accept': self.accepted.append(index)
        elif outcome == 'unknown': self.unknown = index
        elif outcome != 'reject': raise ValueError(outcome)
    def classification(self):
        if self.unknown is not None: return 'UNKNOWN'
        if len(self.accepted) == self.count: return 'ALL'
        return 'PREFIX' if self.accepted else 'NONE'
    def releasable(self, i, completed=frozenset(), teardown=False):
        if i == self.unknown: return teardown
        if i in self.accepted: return i in completed or teardown
        return True
    def proven_unsubmitted(self):
        return set(range(self.count)) - set(self.accepted) - ({self.unknown} if self.unknown is not None else set())

class MemoryModelTests(unittest.TestCase):
    def test_m01_cross_pool_canonical_and_generation(self):
        self.assertTrue(Span((9,1),64,128).overlaps(Span((9,1),80,160)))
        self.assertFalse(Span((9,1),64,128).overlaps(Span((9,2),64,128)))
    def test_m02_alias_union_not_view_copy(self):
        union_base = 0x10000
        a, b = union_base, union_base + 64
        self.assertEqual(a + 64, b)
        storage = bytearray(192)
        storage[(a+64)-union_base] += 1
        self.assertEqual(storage[b-union_base], 1)
        isolated_a, isolated_b = bytearray(128), bytearray(128)
        isolated_a[64] += 1
        self.assertNotEqual(isolated_a[64], isolated_b[0])
    def test_m03_war_and_stale_conversion(self):
        x = Ledger()
        x.prepare('convert', 'read', 'V0'); x.accept('convert', ('Q0',1))
        self.assertEqual(x.prepare('write', 'write', 'V1'), {('Q0',1)})
        x.accept('write', ('Q1',1))
        self.assertFalse(x.finish_conversion('V0'))
        self.assertEqual(x.authority, 'V1')
    def test_m03_cancel_does_not_publish(self):
        x = Ledger()
        x.prepare('cancelled', 'write', 'V1'); x.cancel('cancelled')
        self.assertEqual(x.authority, 'V0')
        self.assertEqual(x.prepare('next', 'read', 'V0'), set())
    def test_m04_snapshot_extent_vector(self):
        before = ((0,4096,'A',7), (4096,8192,'B',7))
        after  = ((0,4096,'A',7), (4096,8192,'C',8))
        self.assertEqual(before[:1], after[:1])
        self.assertNotEqual(before, after)
        pinned = before
        self.assertEqual(pinned[1][2], 'B')
    def test_m05_fault_wait_cycle(self):
        graph = {'cpu': ['owner'], 'owner': ['gpu'], 'gpu': ['cpu']}
        def cycle(n, stack):
            if n in stack: return True
            return any(cycle(v, stack|{n}) for v in graph.get(n, []))
        self.assertTrue(cycle('cpu', set()))
    def test_m06_line_false_sharing(self):
        def cover(a,b,line=64): return (a//line*line, (b+line-1)//line*line)
        self.assertEqual(cover(0,4), cover(60,64))
        memory = bytearray([0]*64)
        cpu_cache = memory[:]
        cpu_cache[0] = 1
        memory[63] = 2
        memory[:] = cpu_cache  # counterexample: whole-line writeback loses GPU byte
        self.assertEqual(memory[63], 0)
        # Qualified ownership transfer preserves latest neighbor before CPU write.
        memory[63] = 2
        cpu_cache = memory[:]; cpu_cache[0] = 1; memory[:] = cpu_cache
        self.assertEqual((memory[0],memory[63]), (1,2))
    def test_m07_unknown_preserves_prefix(self):
        r = Receipt(3); r.attempt(0,'accept'); r.attempt(1,'unknown')
        self.assertEqual(r.classification(), 'UNKNOWN')
        self.assertEqual(r.proven_unsubmitted(), {2})
        self.assertFalse(r.releasable(0)); self.assertFalse(r.releasable(1))
        self.assertTrue(r.releasable(2)); self.assertTrue(r.releasable(1,teardown=True))
    def test_m07_export_failure_does_not_unsubmit(self):
        r = Receipt(1); r.attempt(0,'accept'); r.export_error=True
        self.assertEqual(r.classification(), 'ALL')
        self.assertFalse(r.releasable(0))
        self.assertTrue(r.releasable(0,completed={0}))
    def test_m08_generation_and_all_refs(self):
        old, new = ('context',1,0xffffffff), ('context',2,0)
        self.assertNotEqual(old[:2],new[:2])
        refs = {'prepared':1,'observer':1,'gpu':0,'logical':0}
        self.assertFalse(all(x==0 for x in refs.values()))
    def test_m10_signal_does_not_wait_poll_retirement(self):
        readers = {'event_word': set(), 'neighbor_data': {('Q0',1)}}
        # SYNC_POLL is tracked by its predicate, not inserted into data readers.
        poll = ('Q0',2)
        signal_waits = readers['event_word']
        self.assertNotIn(poll, signal_waits)
        self.assertEqual(readers['neighbor_data'], {('Q0',1)})
        atomic_group = {('Q0',3), ('Q1',1)}
        self.assertEqual(len(atomic_group), 2)  # observer waits both; no max winner

    def test_m09_sealed_command_copy(self):
        raw = bytearray((1).to_bytes(8,'little')); generation=1
        snapshot = (generation, bytes(raw))
        raw[:] = (2).to_bytes(8,'little')
        self.assertEqual(int.from_bytes(snapshot[1],'little'),1)
        self.assertNotEqual((generation+1,bytes(raw)),snapshot)

if __name__ == '__main__':
    unittest.main(verbosity=2)
