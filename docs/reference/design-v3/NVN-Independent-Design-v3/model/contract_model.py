"""Executable reference specification, deliberately not a GPU/backend implementation."""
from dataclasses import dataclass

@dataclass(frozen=True)
class Point:
    queue: str
    epoch: int
    value: int

class Ledger:
    def __init__(self):
        self.epochs = {}
        self.completed = {}
    def open(self, queue, epoch):
        self.epochs[queue] = epoch
    def complete(self, point):
        if self.epochs.get(point.queue) != point.epoch:
            raise ValueError('stale completion')
        key = (point.queue, point.epoch)
        self.completed[key] = max(point.value, self.completed.get(key, 0))
    def retired(self, points):
        return all(self.epochs.get(p.queue) == p.epoch and
                   self.completed.get((p.queue, p.epoch), 0) >= p.value for p in points)

@dataclass(frozen=True)
class Mapping:
    pool: str
    logical: int
    size: int
    backing: str
    generation: int
    physical: int
    map_epoch: int
    def resolve(self, offset, size):
        if size < 0 or offset < self.logical or offset + size > self.logical + self.size:
            raise ValueError('out of mapping')
        return (self.backing, self.generation, self.physical + offset - self.logical, size)

class Hazards:
    """Byte-granularity oracle; an implementation should use intervals, not this representation."""
    def __init__(self):
        self.writers = {}
        self.readers = {}
    def access(self, span, point, write):
        backing, generation, start, length = span
        deps = set()
        for i in range(start, start + length):
            key = backing, generation, i
            deps |= self.writers.get(key, set())
            if write:
                deps |= self.readers.get(key, set())
                self.writers[key] = {point}
                self.readers[key] = set()
            else:
                self.readers.setdefault(key, set()).add(point)
        return deps - {point}

class Roots:
    def __init__(self):
        self.cache = {}
    def get(self, bindings):
        # Tuple fields include canonical backing generation, MapEpoch, view/encoding version.
        key = tuple(bindings)
        if key not in self.cache:
            self.cache[key] = key
        return self.cache[key]

class Recorder:
    def __init__(self, capacity, callback):
        self.capacity = capacity
        self.callback = callback
        self.storage = []
        self.generation = 0
        self.cache = {}
        self.callback_active = False
    def add_capacity(self, count):
        if count <= 0:
            raise ValueError('non-progressing callback')
        self.capacity += count
    def record(self, command):
        if self.callback_active:
            raise ValueError('unsupported recording reentrancy')
        if len(self.storage) == self.capacity:
            self.callback_active = True
            try:
                self.callback(self)  # Consumed synchronously, not postponed to GPU submission.
            finally:
                self.callback_active = False
            if len(self.storage) == self.capacity:
                raise ValueError('callback did not supply storage')
        self.storage.append(command)
        self.generation += 1
    def patch(self, index, command):
        self.storage[index] = command
        self.generation += 1
    def decode(self):
        if self.generation not in self.cache:
            self.cache[self.generation] = tuple(self.storage)
        return self.cache[self.generation]

@dataclass(frozen=True)
class Pass:
    attachments: tuple
    samples: int
    bytes_required: int
    locality: str
    side_effect: bool = False

def tile_merge(a, b, capacity):
    return (a.attachments == b.attachments and a.samples == b.samples
            and max(a.bytes_required, b.bytes_required) <= capacity
            and b.locality == 'same_pixel_same_sample'
            and not a.side_effect and not b.side_effect)

def replay_schedule(tiles, draw, event):
    # Once-only query/event lives outside repeated tile draw stream.
    return [draw(t) for t in range(tiles)], event()

class Submission:
    """Multiple chunks can be accepted before a later ioctl fails."""
    def __init__(self, chunks):
        self.states = ['prepared'] * chunks
    def result(self, index, outcome):
        if self.states[index] != 'prepared':
            raise ValueError('illegal transition')
        self.states[index] = {'accepted': 'accepted', 'rejected': 'rejected',
                              'unknown': 'quarantined'}[outcome]
    def complete(self, index):
        if self.states[index] != 'accepted':
            raise ValueError('completion without acceptance')
        self.states[index] = 'complete'
    def releasable(self, index):
        return self.states[index] in ('complete', 'rejected')
    def published(self, index):
        return self.states[index] == 'complete'

class RawMirror:
    def __init__(self, raw):
        self.raw = bytearray(raw)
        self.gpu = bytearray(raw)
        self.gpu_authoritative = False
    def gpu_write(self, start, data):
        self.gpu[start:start + len(data)] = data
        self.gpu_authoritative = True
    def cpu_store(self, start, data):
        # Fault boundary materializes BEFORE allowing instruction retry.
        if self.gpu_authoritative:
            self.raw[:] = self.gpu
            self.gpu_authoritative = False
        self.raw[start:start + len(data)] = data
        self.gpu[:] = self.raw
    def cpu_read(self):
        if self.gpu_authoritative:
            self.raw[:] = self.gpu
            self.gpu_authoritative = False
        return bytes(self.raw)

@dataclass(frozen=True)
class SyncToken:
    backing: str
    generation: int
    offset: int
    width: int
    release_scope: frozenset
    acquire_scope: frozenset

class AccessClassifier:
    """Event word polling is synchronization, not an ordinary data-read lifetime."""
    def __init__(self):
        self.data = Hazards()
        self.waits = []
    def poll(self, token, waiter):
        self.waits.append((token, waiter))
        # Hardware/acquire semantics handle the word, not the ordinary WAR frontier.
        return set()
    def signal(self, token, producer):
        # Exactly the token bytes; neighboring data reads retain ordinary WAR dependencies.
        return self.data.access((token.backing, token.generation, token.offset, token.width), producer, True)
    def scopes_match(self, token, published_resources, consumed_resources):
        return (set(published_resources) <= token.release_scope and
                set(consumed_resources) <= token.acquire_scope)
