from tfrecord import TFRecordWriter, tf_record_iterator


def test_tfrecord():
    import tempfile

    path = tempfile.mktemp()

    with TFRecordWriter(path) as writer:
        writer.write(b'first')
        writer.write(b'second')

    data = open(path, 'rb').read()
    assert data == (
        b'\x05\x00\x00\x00\x00\x00\x00\x00\xea\xb2\x04>firstU\xff#\xe5'
        b'\x06\x00\x00\x00\x00\x00\x00\x00si\xd57second\xd3\xe0\xd3\xca'
    )

    assert list(tf_record_iterator(path)) == [b'first', b'second']
    print('OK')


if __name__ == '__main__':
    test_tfrecord()
