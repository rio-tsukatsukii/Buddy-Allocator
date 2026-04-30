#!/usr/bin/env python3

import struct
import argparse
from ctypes import c_uint64
from enum import Enum
from pprint import pp
from dataclasses import dataclass

class LogType(Enum):
    LOG_ERR = 0
    ALLOC_REQ = 1
    ALLOC_OK = 2
    ALLOC_FAIL = 3
    FREE = 4
    SPLIT = 5
    MERGE = 6
    SEQ_END = 7

class ErrType(Enum):
    SPLIT_ERR = 1
    MERGE_ERR = 2
    FREE_ERR = 3
    ALLOC_OK = 4

@dataclass(slots=True)
class Event:
    seq: int
    log_type: int
    order: int | None = None
    chunk_order: int | None = None
    a: int | None = None
    b: int | None = None
    c: int | None = None

class BuddyAllocator:
    def __init__(self, max_order):
        self.MIN_ORDER = 0
        self.MAX_ORDER = max_order
        self.free_lists = [[] for _ in range(0, self.MAX_ORDER + 1)]
        # The () at the end is needed to actually create an array
        # otherwise it just creates a type
        self.split_bitmap = (c_uint64 * ((1 << (self.MAX_ORDER - 6))))()
        self.free_bitmap = (c_uint64 * ((1 << (self.MAX_ORDER - 5))))()

        self.free_lists[self.MAX_ORDER].append(0)

    def free_list_remove(self, order, chunk):
        self.free_lists[order].remove(chunk)

    def free_list_pop(self, order):
        return self.free_list[order].pop(0)

    def free_list_push(self, order, chunk):
        self.free_lists[order].insert(0, chunk)

    def set_split_bit(self, node):
        self.split_bitmap[node >> 6] |= 1 << (node & 63)

    def set_free_bit(self, node):
        self.free_bitmap[node >> 6] |= 1 << (node & 63)

    def clear_split_bit(self, node):
        self.split_bitmap[node >> 6] &= ~(1 << (node & 63))

    def clear_free_bit(self, node):
        self.free_bitmap[node >> 6] &= ~(1 << (node & 63))

    def test_split_bit(self, node):
        return ((self.split_bitmap[node >> 6] >> (node & 63)) & 1) == 1

    def test_free_bit(self, node):
        return ((self.free_bitmap[node >> 6] >> (node & 63)) & 1) == 1


class Validator:
    def __init__(self, order, file, fmt):
        self.events = []
        self.parse_log(file, fmt)
        self.allocator = BuddyAllocator(order)

    def parse_log(self, file, fmt):
        '''
        Parse raw binary log into a list.

        Args:
            file - log file
            fmt - format of log info structure

        Returns:
            None

        Log format for log types:
            seq, the step in log and type, the type of record is common to all types.

            ALLOC_REQ - req_order: Requested order

            ALLOC_OK -  req_order: Requested order
                        alloc_order: Order which was allocated after splitting.
                        addr: Offset of allocated block
                        source_order: Order of block found in free list

            ALLOC_FAIL - req_order: Requested order

            FREE -  addr: Freed block offset
                    order: Freed block order

            Split - parent_addr: Offset of parent block
                    order: Order of parent block
                    left_addr: Offset of left child block
                    right_addr: Offset of right child block

            MERGE - order: Child blocks order
                    parent_order: Merged blocks order
                    left_addr: Offset of left child block
                    right_addr: Offset of right child block
                    merged_addr: Offset of merged block
        '''

        SIZE = struct.calcsize(fmt)

        with open(file, 'rb') as f:
            while chunk := f.read(SIZE):
                seq, log_type, order, chunk_order, a, b, c = struct.unpack(fmt, chunk)
                self.events.append(Event(seq, log_type, order, chunk_order, a, b, c))

    def validate_alloc_req(self, event):
        req_order = event.order

        if req_order > self.allocator.MAX_ORDER:
            return LogType.ALLOC_FAIL

        if self.allocator.free_lists[req_order]:
            return LogType.ALLOC_OK

        if not sum(self.allocator.free_lists[req_order:], []):
            return LogType.ALLOC_FAIL
        else:
            return LogType.SPLIT

        return LogType.SEQ_END

    def validate_alloc_ok(self, event):
        req_order = event.order
        addr = event.a
        alloc_order = event.b
        source_order = event.chunk_order
        node = (1 << (self.allocator.MAX_ORDER - alloc_order)) - 1 + (addr >> (alloc_order + 12))

        if req_order != alloc_order:
            return (ErrType.ALLOC_OK, "Requested order does not match allocated order")

        self.allocator.set_free_bit(node)
        if req_order == source_order:
            self.allocator.free_list_remove(alloc_order, addr)

        return LogType.SEQ_END

    def validate_split_addr(self, event):
        parent_addr = event.a
        order = event.chunk_order - 1
        left_addr = event.b
        right_addr = event.c
        node = (1 << (self.allocator.MAX_ORDER - order)) - 1 + (parent_addr >> (order + 12))

        if self.allocator.test_split_bit((node - 1) >> 1):
            return False

        if self.allocator.test_free_bit((node - 1) >> 1):
            return False

        if self.allocator.test_free_bit(node):
            return False

        if right_addr != (parent_addr + (4096 << (order))):
            return False

        if left_addr != parent_addr:
            return False

        self.allocator.set_split_bit((node - 1) >> 1)
        self.allocator.free_list_push(order, right_addr)

        return True

    def validate_split(self, event):
        self.allocator.free_list_remove(event[0].chunk_order, event[0].a)
        for split in event:
            if not self.validate_split_addr(split):
                return (ErrType.SPLIT_ERR, split)

        return LogType.ALLOC_OK

    def validate_merge(self, event):
        order = event.order
        parent_order = event.chunk_order
        left_addr = event.a
        right_addr = event.b
        merged_addr = event.c
        left_node = (1 << (self.allocator.MAX_ORDER - order)) - 1 + (left_addr >> (order + 12))
        right_node = (1 << (self.allocator.MAX_ORDER - order)) - 1 + (right_addr >> (order + 12))
        merged_node = (1 << (self.allocator.MAX_ORDER - parent_order)) - 1 + (merged_addr >> (parent_order + 12))

        if (order + 1) != parent_order:
            return (ErrType.MERGE_ERR, f'Parent order is {parent_order} while order is {order}')

        if self.allocator.test_free_bit(left_node):
            return (ErrType.MERGE_ERR, f'{hex(left_addr)} is not free')

        if self.allocator.test_free_bit(right_node):
            return (ErrType.MERGE_ERR, f'{hex(right_addr)} is not free')

        if order > 0:
            if self.allocator.test_split_bit(right_node):
                return (ErrType.MERGE_ERR, f'{hex(right_addr)} is split into smaller blocks')

        if not right_addr in self.allocator.free_lists[order]:
            return (ErrType.MERGE_ERR, f'{hex(right_addr)} buddy address not in free list')

        if merged_addr > min(left_addr, right_addr):
            return (ErrType.MERGE_ERR, f'{hex(merged_addr)} is incorrect, it should be {min(left_addr, right_addr)}')

        self.allocator.free_list_remove(order, left_addr)
        self.allocator.free_list_remove(order, right_addr)
        self.allocator.free_list_push(parent_order, merged_addr)

        self.allocator.clear_split_bit(merged_node)

        return LogType.SEQ_END

    def validate_free(self, event):
        addr = event.a
        order = event.order
        node = (1 << (self.allocator.MAX_ORDER - order)) - 1 + (addr >> (order + 12))

        if addr in self.allocator.free_lists[order]:
            return (ErrType.FREE_ERR, 'Double free')

        if not self.allocator.test_free_bit(node):
            return (ErrType.FREE_ERR, 'Chunk freed but free bitmap not updated')

        self.allocator.clear_free_bit(node)
        self.allocator.free_list_push(order, addr)

        buddy = addr ^ (4096 << order)

        if buddy in self.allocator.free_lists[order]:
            return LogType.MERGE

        return LogType.SEQ_END

    def validate(self, start, end):
        i = start
        while i < (end - 1):
            match self.events[i].log_type:
                case 1:
                    next_event = self.validate_alloc_req(self.events[i])

                    if next_event == LogType.SEQ_END:
                        break

                    if next_event.value != self.events[i+1].log_type:
                        print(f'Requested order {self.events[i].order}')
                        print(f'Free list from {self.events[i].order} to {self.allocator.MAX_ORDER}')
                        pp(self.allocator.free_lists[self.events[i].order:])
                        print(f'Inconsistency found at event={self.events[i]}')
                        print(f'Expected {next_event}')
                        break

                case 2:
                    next_event = self.validate_alloc_ok(self.events[i])

                    if type(next_event) is tuple:
                        print(f'Inconsistency found at event={self.events[i]}')
                        print(f'{next_event[1]}')
                        break

                case 4:
                    next_event = self.validate_free(self.events[i])

                    if type(next_event) is tuple:
                        print(f'Inconsistency found at event={self.events[i]}')
                        print(f'{next_event[1]}')
                        break

                    if next_event.value != self.events[i+1].log_type and next_event != LogType.SEQ_END:
                        chunk = self.events[i].a
                        order = self.events[i].order
                        buddy = chunk ^ (4096 << order)
                        chunk_node = (1 << (self.allocator.MAX_ORDER - order)) - 1 + (chunk >> (order + 12))
                        buddy_node = (1 << (self.allocator.MAX_ORDER - order)) - 1 + (buddy >> (order + 12))
                        print(f'Chunk - {hex(chunk)}')
                        print(f'Buddy - {hex(buddy)}')
                        print(f'chunk free: {not self.allocator.test_free_bit(chunk_node)}')
                        print(f'buddy free: {not self.allocator.test_free_bit(buddy_node)}')
                        if order > 0:
                            print(f'buddy split: {self.allocator.test_split_bit(buddy_node)}')
                        print(f'Inconsistency found at event={self.events[i]}')
                        print(f'Expected {next_event}')
                        break

                case 5:
                    req_order = self.events[i - 1].order
                    src_order = self.events[i].chunk_order

                    next_event = self.validate_split(self.events[i:src_order - req_order + i])

                    if type(next_event) is tuple:
                        print(f'Inconsistency found at event={self.events[i:(src_order - req_order + i)]}')
                        print(f'{next_event[1]}')
                        i = i + (src_order - req_order) - 1
                        break

                    i = i + (src_order - req_order) - 1

                    if next_event.value != self.events[i+1].log_type and i < (len(self.events) - 1):
                        print(f'Requested order {req_order}')
                        print(f'Free list for {req_order}: {self.allocator.free_lists[req_order]}')
                        print(f'Inconsistency found at event=',end="")
                        pp(self.events[i:(src_order - req_order + i)])
                        print(f'Expected {next_event}')
                        break

                case 6:
                    next_event = self.validate_merge(self.events[i])

                    if next_event != LogType.SEQ_END:
                        print(f'Inconsistency found at event={self.events[i]}')
                        print(f'{next_event[1]}')
                        break

            i = i + 1
        if i == (end - 1):
            print(f'No inconsistency found')


if __name__ == '__main__':
    parser = argparse.ArgumentParser('validate_log')
    parser.add_argument('file', help='Name of log file')

    args = parser.parse_args()

    validator = Validator(18, args.file, 'QBBBxIII')

    validator.validate(0, len(validator.events))
