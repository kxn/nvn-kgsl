"""Deliberately incorrect simplifications. Never import into a real implementation."""

def scalar_retired(points, completed):
    global_max = max(completed.values(), default=0)
    return all(global_max >= p.value for p in points)

def epochless_retired(points, completed):
    return all(max((v for (q, epoch), v in completed.items() if q == p.queue), default=0)
               >= p.value for p in points)

class ProducerOnlyHazards:
    def __init__(self): self.writer = None
    def access(self, point, write):
        deps = {self.writer} if self.writer else set()
        if write: self.writer = point
        return deps

class LogicalOnlyResolution:
    def __init__(self): self.cache = {}
    def resolve(self, logical_id, encoding):
        if logical_id not in self.cache: self.cache[logical_id] = encoding
        return self.cache[logical_id]

def rollback_entire_batch(outcomes):
    return [True] * len(outcomes) if 'rejected' in outcomes else [False] * len(outcomes)

def unknown_is_rejected(outcome):
    return outcome != 'accepted'

def publish_on_return(outcome):
    # Wrong: host return (success or error) is not GPU completion/publication.
    return outcome in ('accepted', 'rejected', 'unknown')

class PoolKeyHazards:
    def __init__(self): self.writers = {}
    def access(self, pool, offset, size, point, write):
        keys = [(pool, i) for i in range(offset, offset + size)]
        deps = {self.writers[k] for k in keys if k in self.writers}
        if write:
            for k in keys: self.writers[k] = point
        return deps

def diff_merge(snapshot, guest, gpu):
    out = bytearray(gpu)
    for i in range(len(out)):
        if snapshot[i] != guest[i]: out[i] = guest[i]
    return bytes(out)

class PollAsDataReader:
    def __init__(self, hazards): self.data = hazards
    def poll(self, token, waiter):
        return self.data.access((token.backing, token.generation, token.offset, token.width), waiter, False)
    def signal(self, token, producer):
        return self.data.access((token.backing, token.generation, token.offset, token.width), producer, True)
