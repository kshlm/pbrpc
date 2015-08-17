package pbcodec

import (
	"encoding/binary"
	"io"
)

func WriteRpc(w io.Writer, data []byte) (int, error) {
	sizebuf := make([]byte, 8)
	binary.BigEndian.PutUint64(sizebuf, uint64(len(data)))

	n, err := w.Write(sizebuf)
	if err != nil {
		return n, err
	}

	return w.Write(data)
}

func ReadRpc(r io.Reader) ([]byte, error) {
	sizebuf := make([]byte, 8)

	_, err := r.Read(sizebuf)
	if err != nil {
		return nil, err
	}

	size := binary.BigEndian.Uint64(sizebuf)

	data := make([]byte, size)
	_, err = r.Read(data)
	if err != nil {
		return nil, err
	}

	return data, nil
}
