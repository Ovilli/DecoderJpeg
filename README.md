# DecoderJpeg

A baseline JPEG decoder written from scratch in C. One file, no image libraries, only the C standard library and `math.h`.

Every stage of the pipeline is implemented by hand, from the marker segments in the file down to the final RGB pixels. The goal was to understand how a JPEG file is really put together on the byte level.

## How it works

```
JPEG file
  -> marker parser        SOI, APP0, DQT, SOF0, DHT, SOS
  -> Huffman decoding     bit reader with 0xFF00 byte unstuffing, DC/AC tables
  -> coefficient blocks   DC differences, AC run-length, zigzag reordering
  -> dequantization       per-component quantization tables
  -> inverse DCT          straightforward 8x8 float implementation
  -> upsampling + color   4:2:0 chroma upsampling, YCbCr -> RGB
  -> out.ppm (+ out.png)
```

| Stage | Where |
| --- | --- |
| Marker dispatch loop | `main()` |
| Quantization tables | `parse_dqt()` |
| Image size and components | `parse_sof0()` |
| Huffman table + canonical code ranges | `parse_dht()` |
| Bit reader and symbol decoding | `read_huffman()`, `read_huffman_symbols()` |
| Block decoding and dequantization | `read_block()` |
| Inverse DCT | `inverse_dct()` |
| MCU assembly and color conversion | `parse_sos()`, `ycbcr_to_rgb()` |

## Build and run

```sh
make                      # builds ./decode
./decode 44-baseline.jpg  # writes out.ppm and out.png
```

or `make build` to clean, build and run on the sample image in one step.

The decoder always writes `out.ppm`. The extra `out.png` is produced with ImageMagick's `convert`; without it you still get the PPM, which most image viewers and tools can open.

## Verification

The output was compared pixel by pixel against Pillow's decoder on the included 1000x667 sample (`44-baseline.jpg`). The mean absolute difference is below 1 out of 255 per channel. The small remaining difference is expected from the simple chroma upsampling and the float IDCT with rounding.

## Supported files

This is a learning project, so it only handles one JPEG variant and exits with an error message on anything else:

- Baseline DCT (SOF0), 8 bit precision
- 3 components (YCbCr) with 4:2:0 chroma subsampling
- Huffman coding
- One quantization table per DQT segment
- No restart markers, no progressive JPEG, no grayscale, no EXIF parsing (APP segments are skipped)

## Possible next steps

- 4:4:4 and 4:2:2 subsampling, grayscale
- Restart markers and progressive JPEG
- A fast integer IDCT instead of the direct formula
- Write PNG directly instead of calling `convert`
