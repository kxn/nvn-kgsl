import json
import random
import unittest
from pathlib import Path
from contract_model import *
import broken_models as broken

SEED = 0x4E564E
NEGATIVE_CONTROLS = []
GENERATED_MUTANT_KILLS = {}

def detects(name, correct, broken):
    if correct == broken:
        raise AssertionError('negative control escaped: ' + name)
    NEGATIVE_CONTROLS.append(name)

class Contracts(unittest.TestCase):
    def test_cross_pool_alias_and_overlapping_ssbo(self):
        a = Mapping('A', 0, 16, 'bo', 2, 0, 1)
        b = Mapping('B', 100, 8, 'bo', 2, 4, 7)
        self.assertEqual(a.resolve(4, 4), b.resolve(100, 4))
        h = Hazards(); q0 = Point('q0', 1, 1); q1 = Point('q1', 1, 1)
        h.access(a.resolve(4, 4), q0, True)
        self.assertEqual(h.access(b.resolve(100, 4), q1, False), {q0})
        bad = broken.PoolKeyHazards(); bad.access('A', 4, 4, q0, True)
        detects('pool_id_alias_key', {q0}, bad.access('B', 100, 4, q1, False))
        # Same invocation sees SSBO A[4] through SSBO B[0], without a draw boundary.
        backing = bytearray(16); av = memoryview(backing); bv = memoryview(backing)[4:]
        av[4] = 93
        independent_a = bytearray(16); independent_b = bytearray(8)
        independent_a[4] = 93
        self.assertEqual(bv[0], 93)
        detects('per_view_copy_breaks_intra_draw_alias', bv[0], independent_b[0])

    def test_range_publication_and_war(self):
        h = Hazards(); r = Point('reader', 1, 9); w = Point('writer', 1, 2)
        h.access(('bo', 1, 0, 8), r, False)
        self.assertEqual(h.access(('bo', 1, 4, 4), w, True), {r})
        bad = broken.ProducerOnlyHazards(); bad.access(r, False)
        detects('producer_only_omits_WAR', {r}, bad.access(w, True))
        self.assertEqual(h.access(('bo', 1, 8, 4), Point('other', 1, 1), True), set())
        rng = random.Random(SEED)
        for _ in range(300):
            start, size, other, osize = [rng.randrange(1, 17) for _ in range(4)]
            h = Hazards(); h.access(('bo', 1, start, size), r, True)
            expected = {r} if max(start, other) < min(start + size, other + osize) else set()
            self.assertEqual(h.access(('bo', 1, other, osize), w, False), expected)

    def test_multiqueue_retirement_epoch(self):
        ledger = Ledger(); ledger.open('A', 1); ledger.open('B', 1)
        a, b = Point('A', 1, 100), Point('B', 1, 2)
        ledger.complete(a)
        self.assertFalse(ledger.retired({a, b}))
        detects('global_max_scalar_retirement', ledger.retired({a, b}),
                broken.scalar_retired({a, b}, ledger.completed))
        ledger.complete(b); self.assertTrue(ledger.retired({a, b}))
        ledger.open('A', 2)
        ledger.complete(Point('A', 2, 100))
        self.assertFalse(ledger.retired({a}))
        with self.assertRaises(ValueError): ledger.complete(a)
        detects('epochless_completion_ABA', ledger.retired({a}),
                broken.epochless_retired({a}, ledger.completed))

    def test_mapping_and_descriptor_versions(self):
        roots = Roots()
        key = ('bo', 1, 0, 4, 1, 'rgba', 1)
        a = roots.get((key,))
        raw = bytearray(4); raw[0] = 12
        self.assertIs(a, roots.get((key,)))  # Content write does not change encoding.
        remap = key[:4] + (2,) + key[5:]
        self.assertIsNot(a, roots.get((remap,)))
        bad = broken.LogicalOnlyResolution(); bad.resolve('logical-A', key)
        detects('logical_resource_only_resolution_cache', remap, bad.resolve('logical-A', remap))
        b = roots.get((key[:-1] + (2,),))
        self.assertIsNot(a, b)
        self.assertIs(a, roots.get((key,)))  # A/B/A immutable root reuse.
        mutable = [key]; snapshot = mutable; mutable[0] = remap
        detects('mutable_root_overwrites_inflight_A', a, tuple(snapshot))
        # Record selector, not descriptor bytes; resolve at defined consumption boundary.
        pool = {3: key}; selector = 3; record_time = pool[selector]
        pool[3] = remap
        consumed = roots.get((pool[selector],))
        self.assertEqual(consumed, (remap,))
        detects('record_time_descriptor_freeze', consumed, (record_time,))

    def test_callback_patch_copy_and_handle_ABA(self):
        calls = []
        def grow(r): calls.append(len(r.storage)); r.add_capacity(1)
        r = Recorder(0, grow); r.record(('bind', 1))
        self.assertEqual(calls, [0]); self.assertEqual(len(r.storage), 1)
        submitted = r.decode(); copied = tuple(r.decode())
        r.patch(0, ('bind', 2))
        self.assertEqual(submitted, (('bind', 1),)); self.assertEqual(copied, submitted)
        self.assertEqual(r.decode(), (('bind', 2),))
        detects('handle_address_only_decoded_cache', r.decode(), submitted)
        detects('copy_is_call_alias', copied, r.decode())
        failing = Recorder(0, lambda r: None)
        with self.assertRaises(ValueError): failing.record('draw')
        self.assertEqual(failing.storage, [])
        def recursive(r): r.record('nested')
        reentrant = Recorder(0, recursive)
        with self.assertRaises(ValueError): reentrant.record('draw')
        self.assertEqual(reentrant.storage, [])

    def test_render_split_once_only(self):
        a = Pass(('color',), 1, 64, 'same_pixel_same_sample')
        self.assertTrue(tile_merge(a, a, 64))
        for b in [Pass(('color',), 1, 64, 'neighbor'), Pass(('color',), 4, 64, 'same_pixel_same_sample'),
                  Pass(('other',), 1, 64, 'same_pixel_same_sample'), Pass(('color',), 1, 65, 'same_pixel_same_sample'),
                  Pass(('color',), 1, 64, 'same_pixel_same_sample', True)]:
            self.assertFalse(tile_merge(a, b, 64))
        for n in (2, 3, 8, 64):
            events = []
            draws, _ = replay_schedule(n, lambda t: t, lambda: events.append('timestamp'))
            self.assertEqual(draws, list(range(n))); self.assertEqual(len(events), 1)
            detects('query_event_per_tile_' + str(n), 1, n)
        # Neighbor requires prior tile's finalized memory, not a tile-local input.
        previous = [2, 7]; expected = previous[0] + previous[1]
        detects('arbitrary_texture_as_tile_local_input', expected, 7)

    def test_seeded_broken_models(self):
        rng = random.Random(SEED + 2)
        kills = dict(pool_alias=0, war=0, scalar=0, old_value_diff=0)
        for trial in range(200):
            start = rng.randrange(32); size = rng.randrange(1, 17)
            logical = rng.randrange(100, 1000)
            a = Mapping('A', 0, 64, 'bo', 1, 0, 1)
            b = Mapping('B', logical, size, 'bo', 1, start, 1)
            p, q = Point('A', 1, rng.randrange(100, 1000)), Point('B', 1, rng.randrange(1, 50))
            good = Hazards(); bad = broken.PoolKeyHazards()
            good.access(a.resolve(start, size), p, True); bad.access('A', start, size, p, True)
            kills['pool_alias'] += good.access(b.resolve(logical, size), q, False) != bad.access('B', logical, size, q, False)
            good = Hazards(); bad = broken.ProducerOnlyHazards()
            good.access(a.resolve(start, size), p, False); bad.access(p, False)
            kills['war'] += good.access(a.resolve(start, size), q, True) != bad.access(q, True)
            ledger = Ledger(); ledger.open('A', 1); ledger.open('B', 1); ledger.complete(p)
            kills['scalar'] += ledger.retired({p, q}) != broken.scalar_retired({p, q}, ledger.completed)
            initial = bytes(rng.randrange(256) for _ in range(64))
            gpu = bytes((v + 1) % 256 for v in initial)
            model = RawMirror(initial); model.gpu_write(0, gpu)
            model.cpu_store(start, initial[start:start + size])
            guest = bytearray(initial); guest[start:start + size] = initial[start:start + size]
            kills['old_value_diff'] += model.cpu_read() != broken.diff_merge(initial, guest, gpu)
        self.assertEqual(kills, dict(pool_alias=200, war=200, scalar=200, old_value_diff=200))
        GENERATED_MUTANT_KILLS.update(kills)

    def test_event_poll_is_not_data_reader(self):
        token = SyncToken('bo', 1, 128, 4, frozenset({'payload'}), frozenset({'payload'}))
        waiter, signaler, other_reader = Point('Q0', 1, 1), Point('Q1', 1, 1), Point('Q2', 1, 1)
        model = AccessClassifier(); mutant = broken.PollAsDataReader(Hazards())
        model.poll(token, waiter); mutant.poll(token, waiter)
        good_deps = model.signal(token, signaler); bad_deps = mutant.signal(token, signaler)
        # Waiter's event wait already needs signaler. A reverse scheduling dependency is a cycle.
        def two_node_cycle(signal_dependencies): return waiter in signal_dependencies
        self.assertFalse(two_node_cycle(good_deps))
        detects('poll_as_data_reader_wait_cycle', two_node_cycle(good_deps), two_node_cycle(bad_deps))
        # CPU signal uses the same access class; it must not wait for GPU waiter completion either.
        cpu = Point('CPU', 1, 1)
        fresh = AccessClassifier(); fresh.poll(token, waiter)
        self.assertNotIn(waiter, fresh.signal(token, cpu))
        # Neighboring bytes in the same page do not inherit the event exemption.
        model.data.access(('bo', 1, 132, 4), other_reader, False)
        self.assertEqual(model.data.access(('bo', 1, 132, 4), signaler, True), {other_reader})
        self.assertTrue(model.scopes_match(token, {'payload'}, {'payload'}))
        self.assertFalse(model.scopes_match(token, {'unrelated'}, {'payload'}))
        self.assertFalse(model.scopes_match(token, {'payload'}, {'unrelated'}))
        # Metadata is a required proof input, not an implementation of cache release/acquire.

    def test_signal_wait_order_and_availability(self):
        def execute(commands):
            signaled = False
            result_visible = False
            for op in commands:
                if op == 'signal': signaled = True
                elif op == 'wait' and not signaled: return 'blocked'
                elif op == 'result': result_visible = True
                elif op == 'available' and not result_visible: return 'torn_query'
            return 'complete'
        guest_order = ['signal', 'wait', 'result', 'available']
        self.assertEqual(execute(guest_order), 'complete')
        broken_epilogue = [op for op in guest_order if op != 'signal'] + ['signal']
        detects('move_signal_to_epilogue_deadlock', execute(guest_order), execute(broken_epilogue))
        detects('availability_before_visible_result', execute(guest_order),
                execute(['signal', 'wait', 'available', 'result']))

    def test_submit_partial_failure(self):
        s = Submission(3); s.result(0, 'accepted'); s.result(1, 'rejected'); s.result(2, 'unknown')
        self.assertFalse(s.releasable(0)); self.assertTrue(s.releasable(1)); self.assertFalse(s.releasable(2))
        self.assertEqual([s.published(i) for i in range(3)], [False] * 3)
        with self.assertRaises(ValueError): s.complete(1)
        with self.assertRaises(ValueError): s.complete(2)
        detects('whole_submit_rollback_after_prefix_accepted', s.releasable(0),
                broken.rollback_entire_batch(['accepted', 'rejected', 'unknown'])[0])
        detects('unknown_ioctl_means_not_submitted', s.releasable(2), broken.unknown_is_rejected('unknown'))
        s.complete(0); self.assertTrue(s.published(0)); self.assertFalse(s.published(1))
        detects('failed_submit_fake_publication', s.published(1), broken.publish_on_return('rejected'))

    def test_raw_partial_store_sequential_oracle(self):
        rng = random.Random(SEED + 1)
        for _ in range(500):
            initial = bytes(rng.randrange(256) for _ in range(64))
            oracle = bytearray(initial); mirror = RawMirror(initial)
            for __ in range(20):
                start = rng.randrange(64); length = rng.randrange(1, 65 - start)
                data = bytes(rng.randrange(256) for _ in range(length))
                oracle[start:start + length] = data
                if rng.randrange(2): mirror.gpu_write(start, data)
                else: mirror.cpu_store(start, data)
            self.assertEqual(mirror.cpu_read(), bytes(oracle))
        initial = bytes([0, 0, 0, 0]); m = RawMirror(initial)
        m.gpu_write(0, bytes([1, 2, 3, 4])); m.cpu_store(1, bytes([0]))
        self.assertEqual(m.cpu_read(), bytes([1, 0, 3, 4]))
        # Broken snapshot diff sees guest byte 1 equal old snapshot 0: no dirty byte detected.
        old_snapshot = bytearray(initial); guest = bytearray(initial); guest[1] = 0
        broken_gpu = bytearray([1, 2, 3, 4])
        for i in range(4):
            if guest[i] != old_snapshot[i]: broken_gpu[i] = guest[i]
        detects('snapshot_diff_loses_store_of_old_value', m.cpu_read(), bytes(broken_gpu))
        detects('page_upload_clobbers_unwritten_GPU_bytes', m.cpu_read(), bytes(guest))

if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(Contracts)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {'kind': 'pure-host-reference-contract-tests', 'backend_validated': False,
              'seeds': [SEED, SEED + 1, SEED + 2], 'tests_run': result.testsRun,
              'failures': len(result.failures), 'errors': len(result.errors),
              'random_range_cases': 300, 'random_byte_sequences': 500,
              'operations_per_byte_sequence': 20,
              'negative_controls_detected': NEGATIVE_CONTROLS,
              'seeded_mutant_counterexamples': GENERATED_MUTANT_KILLS,
              'passed': result.wasSuccessful()}
    Path(__file__).with_name('check-report.json').write_text(json.dumps(report, indent=2) + '\n')
    raise SystemExit(not result.wasSuccessful())
