import unittest
from append_contiguity import append_contiguity


def row(at=100, count=2, links=4, prefix=1024, invalid=0):
    return (f'exfat-append-contiguous: version=1 snapshot_us={at} local_complete={count} '
            f'links_avoided={links} prefix_bytes={prefix} invalid={invalid}\n').encode()


class Tests(unittest.TestCase):
    def test_complete_copies_and_monotonic_snapshots(self):
        result = append_contiguity([row(), row(), row(200, 3, 5, 1536)], require_exercised=True)
        self.assertEqual((result['local_complete'], result['snapshots']), (3, 2))
        self.assertNotIn('commits', result)

    def test_zero_is_valid_unexercised_evidence(self):
        self.assertFalse(append_contiguity([row(count=0, links=0, prefix=0)])['exercised'])
        with self.assertRaises(ValueError):
            append_contiguity([row(count=0, links=0, prefix=0)], require_exercised=True)

    def test_local_completion_does_not_imply_operation_success(self):
        result = append_contiguity([row() + b'operation=FAIL volume_quarantined=1\n'])
        self.assertEqual(result['status'], 'VALID')
        self.assertNotIn('operation_success', result)
        self.assertIn('outer mutation can still fail', result['scope'])

    def test_missing_embedded_and_malformed_rows(self):
        for value in [b'', b'UART interrupted ' + row(), row().replace(b'version=1', b'version=2'),
                      row().replace(b'prefix_bytes=1024', b'prefix_bytes=-1'), row().rstrip() + b' extra']:
            with self.subTest(value=value), self.assertRaises(ValueError):
                append_contiguity([value])

    def test_invalid_bounds_and_accounting(self):
        for value in [row(at=0), row(at=1 << 64), row(count=1 << 64), row(links=1),
                      row(prefix=1023), row(count=0), row(invalid=1), row(prefix=1 << 64)]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                append_contiguity([value])

    def test_conflict_and_regression(self):
        for values in [[row(), row(links=5)], [row(), row(200, 1, 4, 1024)],
                       [row(), row(200, 2, 3, 1024)], [row(prefix=2048), row(200)]]:
            with self.subTest(values=values), self.assertRaises(ValueError):
                append_contiguity(values)

    def test_valid_uint64_boundary(self):
        maximum = (1 << 64) - 1
        self.assertEqual(append_contiguity([row(maximum, 1, maximum, maximum)])['links_avoided'], maximum)


if __name__ == '__main__':
    unittest.main()
